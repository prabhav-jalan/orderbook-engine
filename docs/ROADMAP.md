# Roadmap

## Phase 0: Project Scaffolding ✅
- [x] Repository structure with CMake build system
- [x] GitHub Actions CI (multi-compiler, sanitizer builds)
- [x] Architecture and roadmap documentation
- [x] Public headers with fixed-point pricing types

## Phase 1: Core Data Structures ✅
- [x] `Order` struct with fixed-point pricing and intrusive list pointers
- [x] `PriceLevel`: intrusive doubly-linked FIFO queue, O(1) add/remove, cached total quantity
- [x] `OrderBook`: `std::map` bid/ask sides, O(1) best bid/ask, O(1) cancel via ID index
- [x] 25 unit tests covering FIFO ordering, removal edge cases, modify, multi-level scenarios

## Phase 2: Matching Engine ✅
- [x] Price-time priority matching
- [x] Limit orders (match or rest)
- [x] Market orders (match or cancel)
- [x] IOC — Immediate or Cancel
- [x] FOK — Fill or Kill (pre-check before execution)
- [x] Price improvement (execution at resting order's price)
- [x] Multi-level sweep for large orders
- [x] Trade event generation
- [x] 22 test cases covering all order types and edge cases

## Phase 3: Benchmarking ✅
- [x] Custom benchmark harness (nanosecond timing, percentiles, compiler fences)
- [x] Synthetic order generator (uniform and clustered price distributions)
- [x] Four benchmark scenarios (raw book ops, matching uniform/clustered, add-only)
- [x] Bulk wall-clock throughput measurement (best of 3 runs)
- [x] Baseline results documented
- [x] Helper script (`tools/run_benchmarks.sh`)
