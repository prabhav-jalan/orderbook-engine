#pragma once

#include "orderbook/types.hpp"
#include "orderbook/price_level.hpp"

#include <map>
#include <unordered_map>
#include <optional>
#include <vector>

namespace orderbook {

/// Execution report generated when an order is added, cancelled, or modified.
struct ExecutionReport {
    enum class Type : uint8_t {
        Accepted,
        Cancelled,
        Modified
    };

    Type    type;
    OrderId order_id;
};

/// A complete order book for a single instrument (e.g., AAPL).
///
/// Contains two sides:
///   - Bids: buy orders, sorted by price DESCENDING (best bid = highest price)
///   - Asks: sell orders, sorted by price ASCENDING  (best ask = lowest price)
class OrderBook {
public:
    OrderBook() = default;

    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;

    /// Add an order to the book.
    ExecutionReport add_order(Order* order);

    /// Cancel an order by its ID.
    std::optional<ExecutionReport> cancel_order(OrderId order_id);

    /// Modify an order's quantity.
    std::optional<ExecutionReport> modify_order(OrderId order_id, Quantity new_quantity);

    // ──────────────────────────────────────────
    // Queries
    // ──────────────────────────────────────────

    [[nodiscard]] std::optional<Price> best_bid() const;
    [[nodiscard]] std::optional<Price> best_ask() const;
    [[nodiscard]] Quantity volume_at_price(Side side, Price price) const;
    [[nodiscard]] std::size_t level_count(Side side) const;
    [[nodiscard]] std::size_t order_count() const { return orders_.size(); }
    [[nodiscard]] bool contains(OrderId order_id) const;
    [[nodiscard]] Order* find_order(OrderId order_id) const;

    // ──────────────────────────────────────────
    // Level access (used by MatchingEngine)
    // ──────────────────────────────────────────

    [[nodiscard]] PriceLevel* best_bid_level();
    [[nodiscard]] PriceLevel* best_ask_level();
    void remove_filled_order(Order* order);
    void rest_order(Order* order);

    [[nodiscard]] Quantity matchable_ask_quantity(Price limit_price, bool is_market) const;
    [[nodiscard]] Quantity matchable_bid_quantity(Price limit_price, bool is_market) const;

private:
    /// Bids: std::map with std::greater so begin() = highest price (best bid).
    std::map<Price, PriceLevel, std::greater<Price>> bids_;

    /// Asks: std::map with default std::less so begin() = lowest price (best ask).
    std::map<Price, PriceLevel> asks_;

    /// Order ID → Order pointer for O(1) lookups on cancel/modify.
    std::unordered_map<OrderId, Order*> orders_;

    void remove_order_from_level(Order* order);
};

} // namespace orderbook
