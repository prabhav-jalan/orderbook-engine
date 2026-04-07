# Limit Order Book Engine

![Build](https://github.com/prabhav-jalan/orderbook-engine/actions/workflows/ci.yml/badge.svg)
![License](https://img.shields.io/badge/license-MIT-blue.svg)
![C++](https://img.shields.io/badge/C%2B%2B-20-blue.svg)

A high-performance limit order book and matching engine built from scratch in modern C++20. Designed for low-latency order processing with price-time priority matching.

## Features

| Feature                        | Status |
|--------------------------------|--------|
| Core order book data structures | 📋 Planned |
| Price-time priority matching    | 📋 Planned |
| Benchmark suite                 | 📋 Planned |
| Memory pool / arena allocator   | 📋 Planned |
| Lock-free order ingestion       | 📋 Planned |
| FIX 4.2 protocol parser         | 📋 Planned |
| Market data snapshots (L2)      | 📋 Planned |

## Performance

> Benchmarks coming soon — see [benchmarks/](benchmarks/) for methodology.

## Building

### Prerequisites

- C++20 compatible compiler (GCC 12+, Clang 15+, MSVC 2022+)
- CMake 3.20+
- Google Test (fetched automatically via CMake)

### Build & Run

```bash
# Clone the repository
git clone https://github.com/YOUR_USERNAME/orderbook-engine.git
cd orderbook-engine

# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run tests
cd build && ctest --output-on-failure

# Run benchmarks
./build/benchmarks/orderbook_bench
```

## Project Structure

```
src/
├── core/           # Order, PriceLevel, OrderBook
├── matching/       # Matching engine logic
├── protocol/       # FIX / binary message parsing
└── gateway/        # Network layer (TCP server)
include/orderbook/  # Public headers
tests/              # Unit and integration tests
benchmarks/         # Performance benchmarks and results
docs/               # Architecture docs and roadmap
```

## Documentation

- [Architecture](docs/ARCHITECTURE.md) — Design decisions and system overview
- [Roadmap](docs/ROADMAP.md) — Development phases and progress

## License

MIT — see [LICENSE](LICENSE) for details.
