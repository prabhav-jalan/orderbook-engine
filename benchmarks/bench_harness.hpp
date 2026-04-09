#pragma once

// benchmarks/bench_harness.hpp
//
// Lightweight benchmark harness. We avoid Google Benchmark as a dependency
// to keep the build simple and self-contained. This harness provides:
//   - High-resolution timing (nanosecond precision)
//   - Percentile calculation (p50, p95, p99, p99.9)
//   - Warmup and iteration control
//   - Formatted result output

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace bench {

using Clock    = std::chrono::high_resolution_clock;
using Duration = std::chrono::nanoseconds;

/// Collected latency samples from a benchmark run.
struct LatencyStats {
    std::string name;
    std::size_t iterations{0};
    Duration    total{0};

    Duration min{Duration::max()};
    Duration max{Duration::zero()};
    Duration mean{0};
    Duration median{0};
    Duration p50{0};
    Duration p95{0};
    Duration p99{0};
    Duration p999{0};

    double throughput_ops_per_sec{0.0};
};

/// Compute percentile from a SORTED vector of durations.
inline Duration percentile(const std::vector<Duration>& sorted, double pct) {
    if (sorted.empty()) return Duration::zero();
    auto idx = static_cast<std::size_t>(
        std::ceil(pct / 100.0 * static_cast<double>(sorted.size())) - 1.0
    );
    idx = std::min(idx, sorted.size() - 1);
    return sorted[idx];
}

/// Format a duration as a human-readable string (ns, μs, ms, s).
inline std::string format_duration(Duration d) {
    auto ns = d.count();
    if (ns < 1'000) {
        return std::to_string(ns) + " ns";
    } else if (ns < 1'000'000) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f μs",
                      static_cast<double>(ns) / 1'000.0);
        return buf;
    } else if (ns < 1'000'000'000) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f ms",
                      static_cast<double>(ns) / 1'000'000.0);
        return buf;
    } else {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f s",
                      static_cast<double>(ns) / 1'000'000'000.0);
        return buf;
    }
}

/// Format a throughput number with comma separators.
inline std::string format_throughput(double ops) {
    // Convert to integer for formatting
    auto int_ops = static_cast<uint64_t>(ops);
    auto s = std::to_string(int_ops);
    // Insert commas from the right
    int n = static_cast<int>(s.length());
    for (int i = n - 3; i > 0; i -= 3) {
        s.insert(static_cast<std::size_t>(i), ",");
    }
    return s + " ops/sec";
}

/// Compute stats from a vector of individual operation latencies.
inline LatencyStats compute_stats(const std::string& name,
                                  std::vector<Duration>& samples) {
    LatencyStats stats;
    stats.name = name;
    stats.iterations = samples.size();

    if (samples.empty()) return stats;

    // Sort for percentile computation.
    std::sort(samples.begin(), samples.end());

    stats.min    = samples.front();
    stats.max    = samples.back();
    stats.median = percentile(samples, 50.0);
    stats.p50    = stats.median;
    stats.p95    = percentile(samples, 95.0);
    stats.p99    = percentile(samples, 99.0);
    stats.p999   = percentile(samples, 99.9);

    // Total and mean.
    Duration total{0};
    for (auto& s : samples) {
        total += s;
    }
    stats.total = total;
    stats.mean  = Duration(total.count() / static_cast<int64_t>(samples.size()));

    // Throughput.
    double total_sec = static_cast<double>(total.count()) / 1'000'000'000.0;
    if (total_sec > 0) {
        stats.throughput_ops_per_sec =
            static_cast<double>(samples.size()) / total_sec;
    }

    return stats;
}

/// Print a LatencyStats result in a clean table format.
inline void print_stats(const LatencyStats& stats) {
    std::cout << "\n┌─────────────────────────────────────────────┐\n";
    std::cout << "│ " << std::left << std::setw(44) << stats.name << "│\n";
    std::cout << "├─────────────────────────────────────────────┤\n";
    std::cout << "│ Iterations:  " << std::setw(30)
              << stats.iterations << " │\n";
    std::cout << "│ Throughput:  " << std::setw(30)
              << format_throughput(stats.throughput_ops_per_sec) << " │\n";
    std::cout << "├─────────────────────────────────────────────┤\n";
    std::cout << "│ Min:         " << std::setw(30)
              << format_duration(stats.min) << " │\n";
    std::cout << "│ Mean:        " << std::setw(30)
              << format_duration(stats.mean) << " │\n";
    std::cout << "│ p50:         " << std::setw(30)
              << format_duration(stats.p50) << " │\n";
    std::cout << "│ p95:         " << std::setw(30)
              << format_duration(stats.p95) << " │\n";
    std::cout << "│ p99:         " << std::setw(30)
              << format_duration(stats.p99) << " │\n";
    std::cout << "│ p99.9:       " << std::setw(30)
              << format_duration(stats.p999) << " │\n";
    std::cout << "│ Max:         " << std::setw(30)
              << format_duration(stats.max) << " │\n";
    std::cout << "└─────────────────────────────────────────────┘\n";
}

/// Prevent the compiler from optimizing away a value.
/// Essential for benchmarks — without this, the compiler can
/// eliminate the entire operation we're trying to measure.
template<typename T>
inline void do_not_optimize(T const& value) {
    asm volatile("" : : "r,m"(value) : "memory");
}

/// Compiler fence — prevents instruction reordering across this point.
inline void fence() {
    asm volatile("" ::: "memory");
}

} // namespace bench
