# Architecture

## System Overview

```
┌─────────────────────────────────────────────────────────┐
│                      Gateway Layer                       │
│              (TCP server, protocol parsing)               │
└──────────────────────┬──────────────────────────────────┘
                       │ Order messages
                       ▼
┌─────────────────────────────────────────────────────────┐
│                    Matching Engine                        │
│         (price-time priority order matching)              │
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
└─────────────────────────────────────────────────────────┘
```

## Core Components

### Order

Represents a single order in the system.

**Key design decisions:**
- **Fixed-point pricing:** Prices are stored as `int64_t` in ticks (e.g., $150.25 → 15025 with tick size 0.01). Floating-point arithmetic introduces rounding errors that are unacceptable in financial systems.
- **Intrusive linked list pointers:** Each Order contains `prev`/`next` pointers for its price level's queue. This avoids separate node allocations and improves cache locality vs. `std::list`.

### PriceLevel

All resting orders at a single price, maintained as a FIFO queue.

**Key design decisions:**
- **Intrusive doubly-linked list:** O(1) insert at tail, O(1) removal of any node (given pointer). No heap allocation per insert.
- **Cached total quantity:** Maintained incrementally on every add/remove. Querying volume at a price is O(1), not O(n).

### OrderBook

One book per instrument. Contains bid and ask sides.

**Key design decisions:**
- **Phase 1:** `std::map<Price, PriceLevel>` for price level lookup. Provides O(log N) insert/lookup with ordered iteration. Simple and correct.
- **Future optimization:** If the price range is bounded and dense, an array indexed by price offset gives O(1) access. This is a Phase 4 optimization — correctness first.
- **Order ID index:** `std::unordered_map<OrderId, Order*>` for O(1) cancel/modify by ID.

### Matching Engine

Processes incoming order messages and produces execution events.

**Key design decisions:**
- **Single-threaded matching:** Real exchanges (CME, NASDAQ) use single-threaded matching per instrument. Multithreading adds complexity without improving latency for the critical path. Parallelism happens at the instrument level (different books on different threads).
- **Event-driven output:** Matching produces a sequence of events (fills, cancels, accepts) rather than returning a single result. This maps cleanly to downstream consumers (market data feed, risk engine, logging).

## Memory Management

- **Phase 1:** Standard `new`/`delete` via smart pointers. Correct but slow.
- **Phase 4:** Arena/pool allocator for Order objects. Pre-allocate a large block, hand out fixed-size chunks. Dramatically reduces allocator overhead and improves cache locality.

## Threading Model (Planned)

```
[Network Thread] → SPSC Ring Buffer → [Matching Thread] → SPSC Ring Buffer → [Market Data Thread]
```

- Network thread: Parses incoming messages, validates, writes to ring buffer
- Matching thread: Reads orders, runs matching logic (hot path, pinned to a core)
- Market data thread: Consumes events, builds and publishes snapshots
