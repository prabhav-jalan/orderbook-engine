// benchmarks/throughput_bench.cpp
//
// Benchmark suite for the order book and matching engine.
//
// Measures:
//   1. OrderBook add/cancel throughput (no matching)
//   2. MatchingEngine submit throughput (with matching)
//   3. Per-operation latency with percentile breakdown
//   4. Different workload profiles (uniform, clustered, add-only)
//
// Run with:
//   cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON
//   cmake --build build
//   ./build/benchmarks/orderbook_bench
//
// IMPORTANT: Always benchmark in Release mode (-O2/-O3).
// Debug mode includes assertions and sanitizer overhead.

#include "orderbook/matching_engine.hpp"
#include "bench_harness.hpp"
#include "order_generator.hpp"

#include <iostream>
#include <memory>
#include <vector>
#include <cstring>

using namespace orderbook;
using namespace bench;

// ══════════════════════════════════════════════════════════
// Benchmark 1: Raw OrderBook add + cancel (no matching)
// ══════════════════════════════════════════════════════════

LatencyStats bench_orderbook_add_cancel(std::size_t num_ops) {
    auto ops = generate_uniform(GeneratorConfig{
        .num_orders = num_ops,
        .cancel_ratio = 0.3,
        .seed = 42
    });

    // Pre-allocate orders. The book holds raw pointers, so we need
    // stable storage. Use a vector of Order indexed by ID.
    std::vector<Order> order_storage(num_ops + 1);

    OrderBook book;
    std::vector<Duration> latencies;
    latencies.reserve(num_ops);

    for (auto& op : ops) {
        if (op.action == Action::Add) {
            // Copy into stable storage.
            order_storage[op.order.id] = op.order;
            Order* o = &order_storage[op.order.id];

            fence();
            auto start = Clock::now();
            book.add_order(o);
            auto end = Clock::now();
            fence();

            latencies.push_back(
                std::chrono::duration_cast<Duration>(end - start)
            );
        } else {
            fence();
            auto start = Clock::now();
            book.cancel_order(op.order.id);
            auto end = Clock::now();
            fence();

            latencies.push_back(
                std::chrono::duration_cast<Duration>(end - start)
            );
        }
    }

    do_not_optimize(book);
    return compute_stats("OrderBook add+cancel (uniform)", latencies);
}

// ══════════════════════════════════════════════════════════
// Benchmark 2: Matching engine throughput (with matching)
// ══════════════════════════════════════════════════════════

LatencyStats bench_matching_engine(std::size_t num_ops,
                                   const std::string& profile) {
    GeneratorConfig cfg{
        .num_orders = num_ops,
        .cancel_ratio = 0.3,
        .seed = 42
    };

    auto ops = (profile == "clustered")
        ? generate_clustered(cfg)
        : generate_uniform(cfg);

    // Storage for orders.
    std::vector<Order> order_storage(num_ops + 1);

    MatchingEngine engine;
    std::vector<Duration> latencies;
    latencies.reserve(num_ops);

    for (auto& op : ops) {
        if (op.action == Action::Add) {
            order_storage[op.order.id] = op.order;
            Order* o = &order_storage[op.order.id];

            fence();
            auto start = Clock::now();
            auto result = engine.submit_order(o);
            auto end = Clock::now();
            fence();

            do_not_optimize(result);
            latencies.push_back(
                std::chrono::duration_cast<Duration>(end - start)
            );
        } else {
            fence();
            auto start = Clock::now();
            engine.cancel_order(op.order.id);
            auto end = Clock::now();
            fence();

            latencies.push_back(
                std::chrono::duration_cast<Duration>(end - start)
            );
        }
    }

    do_not_optimize(engine);
    std::string name = "MatchingEngine submit+cancel (" + profile + ")";
    return compute_stats(name, latencies);
}

// ══════════════════════════════════════════════════════════
// Benchmark 3: Add-only throughput (pure insertion, no matching)
// ══════════════════════════════════════════════════════════

LatencyStats bench_add_only(std::size_t num_ops) {
    GeneratorConfig cfg{
        .num_orders = num_ops,
        .cancel_ratio = 0.0,
        .seed = 42
    };
    auto ops = generate_uniform(cfg);

    std::vector<Order> order_storage(num_ops + 1);
    MatchingEngine engine;
    std::vector<Duration> latencies;
    latencies.reserve(num_ops);

    for (auto& op : ops) {
        order_storage[op.order.id] = op.order;
        Order* o = &order_storage[op.order.id];

        fence();
        auto start = Clock::now();
        auto result = engine.submit_order(o);
        auto end = Clock::now();
        fence();

        do_not_optimize(result);
        latencies.push_back(
            std::chrono::duration_cast<Duration>(end - start)
        );
    }

    do_not_optimize(engine);
    return compute_stats("MatchingEngine add-only (no cancels)", latencies);
}

// ══════════════════════════════════════════════════════════
// Benchmark 4: Bulk throughput measurement (wall-clock)
//
// Instead of timing each operation individually (which adds
// timer overhead), this measures total wall-clock time for
// processing a batch. Gives a truer throughput number.
// ══════════════════════════════════════════════════════════

void bench_bulk_throughput(std::size_t num_ops) {
    GeneratorConfig cfg{
        .num_orders = num_ops,
        .cancel_ratio = 0.3,
        .seed = 42
    };
    auto ops = generate_uniform(cfg);
    std::vector<Order> order_storage(num_ops + 1);

    // Prepare orders in storage first.
    for (auto& op : ops) {
        if (op.action == Action::Add) {
            order_storage[op.order.id] = op.order;
        }
    }

    // Run 3 iterations, report best.
    double best_throughput = 0.0;

    for (int run = 0; run < 3; ++run) {
        // Reset: create fresh engine, reset order states.
        MatchingEngine engine;
        for (auto& op : ops) {
            if (op.action == Action::Add) {
                Order& o = order_storage[op.order.id];
                o.filled_quantity = 0;
                o.status = OrderStatus::New;
                o.prev = nullptr;
                o.next = nullptr;
            }
        }

        fence();
        auto start = Clock::now();

        for (auto& op : ops) {
            if (op.action == Action::Add) {
                engine.submit_order(&order_storage[op.order.id]);
            } else {
                engine.cancel_order(op.order.id);
            }
        }

        auto end = Clock::now();
        fence();
        do_not_optimize(engine);

        double elapsed_sec = std::chrono::duration<double>(end - start).count();
        double throughput = static_cast<double>(num_ops) / elapsed_sec;
        best_throughput = std::max(best_throughput, throughput);
    }

    std::cout << "\n┌─────────────────────────────────────────────┐\n";
    std::cout << "│ Bulk throughput (best of 3 runs)             │\n";
    std::cout << "├─────────────────────────────────────────────┤\n";
    std::cout << "│ Operations:    " << std::setw(28) << num_ops << " │\n";
    std::cout << "│ Throughput:    " << std::setw(28)
              << format_throughput(best_throughput) << " │\n";
    std::cout << "└─────────────────────────────────────────────┘\n";
}

// ══════════════════════════════════════════════════════════
// Main
// ══════════════════════════════════════════════════════════

int main(int argc, char* argv[]) {
    std::size_t num_ops = 500'000; // Default

    // Allow overriding from command line.
    if (argc > 1) {
        num_ops = static_cast<std::size_t>(std::atol(argv[1]));
    }

    std::cout << "═══════════════════════════════════════════════\n";
    std::cout << " Order Book Engine — Benchmark Suite\n";
    std::cout << " Operations per test: " << num_ops << "\n";
    std::cout << "═══════════════════════════════════════════════\n";

    // Warmup run — primes caches, triggers JIT-like effects.
    {
        std::cout << "\n[Warmup...]\n";
        auto warmup_ops = generate_uniform(GeneratorConfig{
            .num_orders = 10'000,
            .seed = 99
        });
        std::vector<Order> warmup_storage(10'001);
        MatchingEngine warmup_engine;
        for (auto& op : warmup_ops) {
            if (op.action == Action::Add) {
                warmup_storage[op.order.id] = op.order;
                warmup_engine.submit_order(&warmup_storage[op.order.id]);
            } else {
                warmup_engine.cancel_order(op.order.id);
            }
        }
        do_not_optimize(warmup_engine);
    }

    // Run benchmarks.
    std::cout << "\n── Per-operation latency benchmarks ──\n";

    auto stats1 = bench_orderbook_add_cancel(num_ops);
    print_stats(stats1);

    auto stats2 = bench_matching_engine(num_ops, "uniform");
    print_stats(stats2);

    auto stats3 = bench_matching_engine(num_ops, "clustered");
    print_stats(stats3);

    auto stats4 = bench_add_only(num_ops);
    print_stats(stats4);

    std::cout << "\n── Bulk throughput benchmark ──\n";
    bench_bulk_throughput(num_ops);

    // Summary table for easy copy-paste into docs.
    std::cout << "\n\n── Summary ──\n\n";
    std::cout << "| Benchmark | Throughput | p50 | p99 | p99.9 |\n";
    std::cout << "|-----------|-----------|-----|-----|-------|\n";

    auto print_row = [](const LatencyStats& s) {
        std::cout << "| " << std::left << std::setw(40) << s.name
                  << " | " << std::setw(18)
                  << format_throughput(s.throughput_ops_per_sec)
                  << " | " << std::setw(12) << format_duration(s.p50)
                  << " | " << std::setw(12) << format_duration(s.p99)
                  << " | " << std::setw(12) << format_duration(s.p999)
                  << " |\n";
    };

    print_row(stats1);
    print_row(stats2);
    print_row(stats3);
    print_row(stats4);

    std::cout << "\n";
    return 0;
}
