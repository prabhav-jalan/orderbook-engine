# Limit Order Book Engine

![Build](https://github.com/prabhav-jalan/orderbook-engine/actions/workflows/ci.yml/badge.svg)
![License](https://img.shields.io/badge/license-MIT-blue.svg)
![C++](https://img.shields.io/badge/C%2B%2B-20-blue.svg)

A high-performance limit order book and matching engine built from scratch in modern C++20. Features price-time priority matching across four order types, intrusive linked list price levels with zero per-order heap allocations, and a reproducible benchmark suite with nanosecond-precision latency measurement.

## Features

| Feature                        | Status |
|--------------------------------|--------|
| Core order book data structures | ✅ Complete |
| Price-time priority matching    | ✅ Complete |
| Limit / Market / IOC / FOK orders | ✅ Complete |
| Benchmark suite                 | ✅ Complete |

## Architecture

```
              Incoming Orders
                    │
                    ▼
          ┌─────────────────┐
          │ Matching Engine │   Price-time priority matching
          │                 │   Limit, Market, IOC, FOK
          └────────┬────────┘
                   │
          ┌────────▼────────┐
          │   Order Book    │   Per-instrument bid/ask sides
          │                 │   std::map + intrusive linked lists
          │  ┌────┐ ┌────┐  │
          │  │Bids│ │Asks│  │
          │  └────┘ └────┘  │
          └────────┬────────┘
                   │
            Trade Events
```

**Key design choices:**
- **Fixed-point pricing** — prices stored as `int64_t` ticks, not floating point. No rounding errors.
- **Intrusive doubly-linked lists** — O(1) insert/remove with zero heap allocations per order.
- **`std::map` with comparator trick** — bids use `std::greater` so `begin()` = best bid; asks use `std::less` so `begin()` = best ask. O(1) best price access.
- **Execution at resting price** — aggressors get price improvement (buy at $11 hitting ask at $10 trades at $10).
- **Single-threaded matching** — matches real exchange architecture (CME, NASDAQ). Parallelism is per-instrument, not within.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the full design document.

## Building

### Prerequisites

- C++20 compatible compiler (GCC 12+, Clang 15+)
- CMake 3.20+
- Google Test (fetched automatically via CMake)

### Build & Test

```bash
git clone https://github.com/prabhav-jalan/orderbook-engine.git
cd orderbook-engine

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run tests
cd build && ctest --output-on-failure
```

### Run Benchmarks

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON
cmake --build build
./build/benchmarks/orderbook_bench
```

## Project Structure

```
include/orderbook/
├── types.hpp              # Order, Price, Side, OrderType (fixed-point pricing)
├── price_level.hpp        # Intrusive linked list FIFO queue
├── order_book.hpp         # Bid/ask sides with std::map price levels
└── matching_engine.hpp    # Price-time priority matching, Trade events

src/
├── core/                  # PriceLevel and OrderBook implementations
├── matching/              # Matching engine implementation
├── protocol/              # FIX / binary message parsing (planned)
└── gateway/               # Network layer (planned)

tests/unit/
├── order_test.cpp         # Order struct basics and fixed-point pricing
├── price_level_test.cpp   # FIFO ordering, removal, quantity tracking
├── order_book_test.cpp    # Add/cancel/modify, best bid/ask, multi-level
└── matching_engine_test.cpp # All order types, sweeps, FIFO, edge cases

benchmarks/                # Performance benchmarks (planned)
```

## Documentation

- [Architecture](docs/ARCHITECTURE.md) — System design, data structure choices, and trade-offs
- [Roadmap](docs/ROADMAP.md) — Development phases and progress

## License

MIT — see [LICENSE](LICENSE) for details.
