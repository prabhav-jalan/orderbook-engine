# Benchmark Results — Phase 3 Baseline

> These are the initial baseline numbers before any optimization work (Phase 4).
> Every future optimization PR will include updated numbers against this baseline.
>
> **To reproduce:** Build in Release mode and run the benchmark binary.
> Results will vary by hardware — what matters is relative improvement.

## How to Run

```bash
# Build with optimizations
cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-O3 -march=native" \
    -DBUILD_BENCHMARKS=ON
cmake --build build

# Run the benchmark suite
./build/benchmarks/orderbook_bench

# Or use the helper script
./tools/run_benchmarks.sh
```

## Results

> Run these benchmarks on YOUR machine and replace the table below with your
> actual numbers. The numbers below are from a containerized environment and
> won't reflect your hardware's true performance.

| Benchmark | Throughput | p50 | p99 | p99.9 |
|-----------|-----------|-----|-----|-------|
| *(Run benchmarks and paste your results here)* | | | | |

## Scenarios Explained

### OrderBook add+cancel (uniform)
Raw `OrderBook::add_order()` + `OrderBook::cancel_order()` throughput without
the matching engine. Measures the cost of the core data structures in isolation:
`std::map` lookup for price levels, intrusive list insert/remove, and
`unordered_map` index updates.

### MatchingEngine submit+cancel (uniform)
Full matching engine path: orders are submitted, potentially matched against the
opposite side, and cancelled. Prices are uniformly distributed across a 200-tick
range, so some orders will cross and generate trades.

### MatchingEngine submit+cancel (clustered)
Same as above but with a normal distribution around the mid-price. This is more
realistic — in real markets, most order flow clusters near the current price.
Expect better cache behavior (fewer unique price levels accessed).

### MatchingEngine add-only (no cancels)
Pure order insertion throughput with no cancellations. The book grows continuously.
This stresses the `std::map` insertion path and shows how performance degrades
as the book gets deeper.

### Bulk throughput (best of 3)
Wall-clock measurement of total time to process 500K operations, reported as
the best of 3 runs. This avoids per-operation timer overhead and gives the
truest throughput number.

## Methodology

- **Operations per scenario:** 500,000 (configurable via command line argument)
- **Warmup:** 10,000 operations before timing begins
- **Timing:** `std::chrono::high_resolution_clock` with compiler fences to prevent reordering
- **Optimizer prevention:** `do_not_optimize()` barriers prevent dead code elimination
- **Reproducibility:** Fixed RNG seed (42) for all scenarios
- **Build:** Release mode with `-O3 -march=native`
- **Architecture:** Single-threaded (matches real exchange design)
- **Order generation:** Pre-allocated before timing to exclude allocation cost

## What to Look For in Phase 4

The baseline numbers establish our starting point. Key areas to optimize:

1. **`std::map` overhead** — O(log N) per price level lookup. An array-backed
   structure could give O(1) for dense price ranges.
2. **Allocation overhead** — `std::map` allocates tree nodes on the heap.
   A pool allocator could eliminate this.
3. **Cache misses** — Orders are scattered in memory. An arena allocator
   with contiguous storage would improve locality.
4. **`std::vector<Trade>` in MatchResult** — Allocates on every match.
   A pre-allocated trade buffer would eliminate this hot-path allocation.
