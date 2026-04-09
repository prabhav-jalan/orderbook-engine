# Architecture

## System Overview

```
┌─────────────────────────────────────────────────────────┐
│                      Gateway Layer                       │
│              (TCP server, protocol parsing)               │
│                       [Planned]                          │
└──────────────────────┬──────────────────────────────────┘
                       │ Order messages
                       ▼
┌─────────────────────────────────────────────────────────┐
│                    Matching Engine                        │
│         (price-time priority order matching)              │
│                                                          │
│  Supported order types:                                  │
│    Limit · Market · IOC · FOK                            │
│                                                          │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐  │
│  │  Order Book  │  │  Order Book  │  │   Order Book    │  │
│  │   (AAPL)    │  │   (GOOG)    │  │     (MSFT)      │  │
│  └─────────────┘  └─────────────┘  └─────────────────┘  │
└──────────────────────┬──────────────────────────────────┘
                       │ Trade events, execution reports
                       ▼
┌─────────────────────────────────────────────────────────┐
│                   Market Data Feed                        │
│           (L2 snapshots, trade stream)                    │
│                       [Planned]                          │
└─────────────────────────────────────────────────────────┘
```

## Core Components

### Order (`include/orderbook/types.hpp`)

Represents a single order in the system.

**Key design decisions:**
- **Fixed-point pricing:** Prices are stored as `int64_t` in ticks (e.g., $150.25 → 15025 with tick size $0.01). Floating-point arithmetic introduces rounding errors that are unacceptable in financial systems. All price comparisons are exact integer comparisons.
- **Intrusive linked list pointers:** Each Order contains `prev`/`next` pointers for its price level's queue. This avoids separate node allocations and improves cache locality compared to `std::list`, which allocates a wrapper node per element on the heap.
- **Status tracking:** Orders carry their own status (`New`, `Accepted`, `PartiallyFilled`, `Filled`, `Cancelled`) so consumers can inspect state without querying the book.

### PriceLevel (`include/orderbook/price_level.hpp`)

All resting orders at a single price, maintained as a FIFO queue.

**Key design decisions:**
- **Intrusive doubly-linked list:** O(1) insert at tail, O(1) removal of any node (given pointer). Zero heap allocations per insert — the `Order` struct itself is the list node.
- **Cached total quantity:** Maintained incrementally on every add/remove/fill. Querying volume at a price is O(1), not O(n) walking the list. The `reduce_total_quantity()` method allows the matching engine to update the cache after partial fills without a costly remove+re-add cycle.
- **Non-copyable, movable:** PriceLevels contain raw pointers into orders. Copying would create dangling pointer issues. Move semantics are supported for `std::map` emplacement.

### OrderBook (`include/orderbook/order_book.hpp`)

One book per instrument. Contains bid and ask sides.

**Key design decisions:**
- **`std::map` with comparator-driven ordering:**
  - Bids: `std::map<Price, PriceLevel, std::greater<Price>>` — `begin()` always points to the highest price (best bid).
  - Asks: `std::map<Price, PriceLevel, std::less<Price>>` — `begin()` always points to the lowest price (best ask).
  - This gives O(1) best bid/ask access via `begin()` and O(log N) insert/lookup. The comparator is set at construction — a subtle but important design choice.
- **Order ID index:** `std::unordered_map<OrderId, Order*>` for O(1) cancel/modify by ID. Without this, cancellation would require walking price levels to find the order.
- **Empty level cleanup:** When the last order at a price is removed, the price level is erased from the map. This prevents the map from growing unboundedly and keeps `begin()` always pointing to a valid best price.

### Matching Engine (`include/orderbook/matching_engine.hpp`)

Processes incoming orders against an order book using price-time priority.

**Matching algorithm:**
1. Incoming buy order checks the ask side. If `buy.price >= best_ask.price` (or Market type), matching begins.
2. Walk ask levels from best (lowest) upward. Within each level, match against orders in FIFO order.
3. Fill quantity = `min(incoming.remaining, resting.remaining)`.
4. Execution price = resting order's price (price improvement for the aggressor).
5. Generate a `Trade` event for each fill.
6. Remove fully filled resting orders. Update partially filled orders.
7. After exhausting matchable levels, handle the remainder by order type:
   - **Limit:** Rest unfilled quantity in the book.
   - **Market / IOC:** Cancel unfilled remainder.
   - **FOK:** Pre-checked before any matching — if full quantity isn't available, the order is rejected without touching the book.

**Key design decisions:**
- **Single-threaded matching:** Real exchanges (CME, NASDAQ) use single-threaded matching per instrument. Multithreading adds complexity and lock contention without improving latency for the critical path. Parallelism happens at the instrument level (different books on different threads).
- **Event-driven output:** Matching produces a `MatchResult` containing a vector of `Trade` events plus the order's final disposition (resting, filled, or cancelled). This maps cleanly to downstream consumers.
- **FOK pre-check via `matchable_ask/bid_quantity()`:** Before executing any trades, FOK orders verify that sufficient matchable volume exists at acceptable prices. This walks the book's price levels and sums available quantity — an O(L) operation where L is the number of price levels, which is acceptable since FOK is uncommon in practice.

## Benchmarking Infrastructure

### Harness (`benchmarks/bench_harness.hpp`)
- `std::chrono::high_resolution_clock` for nanosecond-precision timing.
- `fence()` compiler barriers prevent instruction reordering around measured regions.
- `do_not_optimize()` prevents dead code elimination of benchmark results.
- Percentile computation (p50, p95, p99, p99.9) from sorted latency samples.

### Order Generator (`benchmarks/order_generator.hpp`)
- **Uniform distribution:** Prices spread evenly across a configurable range. Useful for measuring average-case behavior.
- **Clustered distribution:** Normal distribution around the mid-price. More realistic — real order flow concentrates near the spread, causing more frequent matches.
- Fixed RNG seed (42) for reproducibility. Anyone cloning the repo gets identical benchmark runs.
- Configurable cancel ratio (default 30%). Realistic markets have 60-80% cancel rates.

### Scenarios (`benchmarks/throughput_bench.cpp`)
- **Per-operation latency:** Each operation individually timed for percentile analysis.
- **Bulk throughput:** Wall-clock time for a full batch (avoids per-operation timer overhead). Best of 3 runs.
- **Warmup phase:** 10,000 operations before timing to prime caches and data structures.

## Memory Management

- **Current (Phases 1-3):** Standard heap allocation. Orders are caller-managed. Simple and correct.
- **Planned (Phase 4):** Arena/pool allocator for Order objects. Pre-allocate a large block, hand out fixed-size chunks. This will reduce allocator overhead and improve cache locality — both of which show up clearly in the benchmarks.

## Threading Model (Planned)

```
[Network Thread] → SPSC Ring Buffer → [Matching Thread] → SPSC Ring Buffer → [Market Data Thread]
```

- Network thread: Parses incoming messages, validates, writes to ring buffer
- Matching thread: Reads orders, runs matching logic (hot path, pinned to a core)
- Market data thread: Consumes events, builds and publishes snapshots
