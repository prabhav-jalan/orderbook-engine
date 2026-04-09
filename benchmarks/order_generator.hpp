#pragma once

// benchmarks/order_generator.hpp
//
// Generates synthetic order streams for benchmarking.
// Supports multiple distribution profiles:
//   - Uniform:   prices spread evenly across a range
//   - Clustered: prices cluster around a mid-point (more realistic)
//   - Adversarial: designed to stress worst-case paths
//
// The generator pre-allocates all orders up front so that
// memory allocation doesn't contaminate benchmark timing.

#include "orderbook/types.hpp"
#include <cstdint>
#include <random>
#include <vector>

namespace bench {

/// What operation to perform with this order.
enum class Action : uint8_t {
    Add,
    Cancel,
};

/// A pre-generated benchmark operation.
struct BenchOp {
    Action         action;
    orderbook::Order order;
};

/// Configuration for the order generator.
struct GeneratorConfig {
    std::size_t num_orders{100'000};

    // Price range (in ticks). Mid-point is (min + max) / 2.
    orderbook::Price price_min{9'900};  // $99.00
    orderbook::Price price_max{10'100}; // $101.00

    // What fraction of operations should be cancels (0.0 to 1.0).
    // Realistic markets have ~60-80% cancel rate.
    double cancel_ratio{0.3};

    // Max quantity per order.
    orderbook::Quantity max_quantity{1000};

    // Random seed for reproducibility.
    uint64_t seed{42};
};

/// Generates a vector of benchmark operations.
inline std::vector<BenchOp> generate_uniform(const GeneratorConfig& cfg) {
    std::mt19937_64 rng(cfg.seed);
    std::uniform_int_distribution<orderbook::Price> price_dist(
        cfg.price_min, cfg.price_max
    );
    std::uniform_int_distribution<orderbook::Quantity> qty_dist(1, cfg.max_quantity);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_real_distribution<double> cancel_dist(0.0, 1.0);

    std::vector<BenchOp> ops;
    ops.reserve(cfg.num_orders);

    orderbook::OrderId next_id = 1;
    std::vector<orderbook::OrderId> active_ids; // IDs currently in the book
    active_ids.reserve(cfg.num_orders);

    for (std::size_t i = 0; i < cfg.num_orders; ++i) {
        // Decide: add or cancel?
        bool do_cancel = !active_ids.empty() && cancel_dist(rng) < cfg.cancel_ratio;

        if (do_cancel) {
            // Pick a random active order to cancel.
            std::uniform_int_distribution<std::size_t> idx_dist(
                0, active_ids.size() - 1
            );
            std::size_t idx = idx_dist(rng);
            orderbook::OrderId cancel_id = active_ids[idx];

            // Swap-remove from active list.
            active_ids[idx] = active_ids.back();
            active_ids.pop_back();

            BenchOp op{};
            op.action = Action::Cancel;
            op.order.id = cancel_id;
            ops.push_back(op);
        } else {
            // Add a new order.
            BenchOp op{};
            op.action = Action::Add;
            op.order.id = next_id++;
            op.order.price = price_dist(rng);
            op.order.quantity = qty_dist(rng);
            op.order.side = side_dist(rng) == 0
                ? orderbook::Side::Buy
                : orderbook::Side::Sell;
            op.order.type = orderbook::OrderType::Limit;
            op.order.timestamp = static_cast<orderbook::Timestamp>(i);
            ops.push_back(op);

            active_ids.push_back(op.order.id);
        }
    }

    return ops;
}

/// Generates orders clustered around a mid-price with normal distribution.
/// This is more realistic — most real order flow is near the current price.
inline std::vector<BenchOp> generate_clustered(const GeneratorConfig& cfg) {
    std::mt19937_64 rng(cfg.seed);

    orderbook::Price mid = (cfg.price_min + cfg.price_max) / 2;
    double stddev = static_cast<double>(cfg.price_max - cfg.price_min) / 6.0;
    std::normal_distribution<double> price_dist(
        static_cast<double>(mid), stddev
    );
    std::uniform_int_distribution<orderbook::Quantity> qty_dist(1, cfg.max_quantity);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_real_distribution<double> cancel_dist(0.0, 1.0);

    std::vector<BenchOp> ops;
    ops.reserve(cfg.num_orders);

    orderbook::OrderId next_id = 1;
    std::vector<orderbook::OrderId> active_ids;
    active_ids.reserve(cfg.num_orders);

    for (std::size_t i = 0; i < cfg.num_orders; ++i) {
        bool do_cancel = !active_ids.empty() && cancel_dist(rng) < cfg.cancel_ratio;

        if (do_cancel) {
            std::uniform_int_distribution<std::size_t> idx_dist(
                0, active_ids.size() - 1
            );
            std::size_t idx = idx_dist(rng);
            orderbook::OrderId cancel_id = active_ids[idx];
            active_ids[idx] = active_ids.back();
            active_ids.pop_back();

            BenchOp op{};
            op.action = Action::Cancel;
            op.order.id = cancel_id;
            ops.push_back(op);
        } else {
            BenchOp op{};
            op.action = Action::Add;
            op.order.id = next_id++;

            // Clamp to price range.
            auto raw_price = static_cast<orderbook::Price>(price_dist(rng));
            op.order.price = std::clamp(raw_price, cfg.price_min, cfg.price_max);

            op.order.quantity = qty_dist(rng);
            op.order.side = side_dist(rng) == 0
                ? orderbook::Side::Buy
                : orderbook::Side::Sell;
            op.order.type = orderbook::OrderType::Limit;
            op.order.timestamp = static_cast<orderbook::Timestamp>(i);
            ops.push_back(op);

            active_ids.push_back(op.order.id);
        }
    }

    return ops;
}

/// Generates add-only workload (no cancels). Useful for measuring
/// pure add+match throughput without cancel noise.
inline std::vector<BenchOp> generate_add_only(const GeneratorConfig& cfg) {
    GeneratorConfig no_cancel = cfg;
    no_cancel.cancel_ratio = 0.0;
    return generate_uniform(no_cancel);
}

} // namespace bench
