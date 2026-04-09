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
///
/// Design decisions (Phase 1):
///   - std::map for price levels: O(log N) insert/lookup, ordered iteration.
///     Good enough for correctness. We'll optimize in Phase 4.
///   - std::unordered_map for order ID index: O(1) cancel/modify by ID.
///   - Orders are owned externally (raw pointers). In Phase 4 we'll add
///     an arena allocator. For now the caller manages Order lifetimes.
class OrderBook {
public:
    OrderBook() = default;

    // Non-copyable (contains PriceLevels which are non-copyable)
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;

    /// Add an order to the book. The order will be placed at the correct
    /// side and price level. Returns Accepted report on success.
    ///
    /// Precondition: order->id must be unique (not already in the book).
    /// The caller retains ownership of the Order memory.
    ExecutionReport add_order(Order* order);

    /// Cancel an order by its ID. Removes it from its price level.
    /// Returns Cancelled report, or std::nullopt if the order wasn't found.
    std::optional<ExecutionReport> cancel_order(OrderId order_id);

    /// Modify an order's quantity. If the new quantity is less than the
    /// filled quantity, the order is cancelled instead.
    /// Reducing quantity preserves time priority.
    /// Returns Modified or Cancelled report, or std::nullopt if not found.
    std::optional<ExecutionReport> modify_order(OrderId order_id, Quantity new_quantity);

    // ──────────────────────────────────────────
    // Queries
    // ──────────────────────────────────────────

    /// Best bid price, or std::nullopt if no bids.
    [[nodiscard]] std::optional<Price> best_bid() const;

    /// Best ask price, or std::nullopt if no asks.
    [[nodiscard]] std::optional<Price> best_ask() const;

    /// Total resting quantity at a given price and side.
    /// Returns 0 if no orders exist at that price.
    [[nodiscard]] Quantity volume_at_price(Side side, Price price) const;

    /// Number of distinct price levels on a given side.
    [[nodiscard]] std::size_t level_count(Side side) const;

    /// Total number of orders in the entire book (both sides).
    [[nodiscard]] std::size_t order_count() const { return orders_.size(); }

    /// Check if a specific order ID exists in the book.
    [[nodiscard]] bool contains(OrderId order_id) const;

    /// Get a pointer to an order by ID, or nullptr if not found.
    [[nodiscard]] Order* find_order(OrderId order_id) const;

    // ──────────────────────────────────────────
    // Level access (used by MatchingEngine)
    // ──────────────────────────────────────────

    /// Get the best (first) bid price level. Returns nullptr if no bids.
    [[nodiscard]] PriceLevel* best_bid_level();

    /// Get the best (first) ask price level. Returns nullptr if no asks.
    [[nodiscard]] PriceLevel* best_ask_level();

    /// Remove a fully filled order from the book's index and its level.
    /// Called by the matching engine after filling an order.
    void remove_filled_order(Order* order);

    /// Add an order directly to the book without generating reports.
    /// Used by the matching engine to rest unfilled remainder.
    void rest_order(Order* order);

    /// Calculate total matchable quantity on the ask side for a buy order
    /// at the given limit price (or all prices if is_market is true).
    [[nodiscard]] Quantity matchable_ask_quantity(Price limit_price, bool is_market) const;

    /// Calculate total matchable quantity on the bid side for a sell order
    /// at the given limit price (or all prices if is_market is true).
    [[nodiscard]] Quantity matchable_bid_quantity(Price limit_price, bool is_market) const;

private:
    /// Bids: std::map with std::greater so begin() = highest price (best bid).
    std::map<Price, PriceLevel, std::greater<Price>> bids_;

    /// Asks: std::map with default std::less so begin() = lowest price (best ask).
    std::map<Price, PriceLevel> asks_;

    /// Order ID → Order pointer for O(1) lookups on cancel/modify.
    std::unordered_map<OrderId, Order*> orders_;

    /// Internal helpers
    void remove_order_from_level(Order* order);
};

} // namespace orderbook
