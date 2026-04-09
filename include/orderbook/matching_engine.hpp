#pragma once

#include "orderbook/types.hpp"
#include "orderbook/order_book.hpp"

#include <vector>

namespace orderbook {

/// A trade that occurred when two orders matched.
struct Trade {
    OrderId   buy_order_id;
    OrderId   sell_order_id;
    Price     price;       // Execution price (resting order's price)
    Quantity  quantity;     // Quantity traded
    Timestamp timestamp;
};

/// Collection of all events produced by processing a single incoming order.
struct MatchResult {
    OrderId order_id{0};
    std::vector<Trade> trades;
    Quantity filled_quantity{0};
    bool is_resting{false};
    bool is_fully_filled{false};
    bool is_cancelled{false};
};

/// The matching engine processes incoming orders against an order book
/// using price-time priority.
///
/// Supported order types:
///   - Limit:  Match what you can, rest the remainder.
///   - Market: Match what you can, cancel the remainder (no resting).
///   - IOC:    Same as Market — match aggressively, cancel unfilled.
///   - FOK:    Only execute if the ENTIRE quantity can be filled.
class MatchingEngine {
public:
    MatchingEngine() = default;

    MatchingEngine(const MatchingEngine&) = delete;
    MatchingEngine& operator=(const MatchingEngine&) = delete;

    /// Submit an order to the matching engine.
    MatchResult submit_order(Order* order);

    /// Cancel a resting order by ID.
    bool cancel_order(OrderId order_id);

    [[nodiscard]] const OrderBook& book() const noexcept { return book_; }
    [[nodiscard]] OrderBook& book() noexcept { return book_; }

private:
    OrderBook book_;

    void match_buy(Order* order, MatchResult& result);
    void match_sell(Order* order, MatchResult& result);
    [[nodiscard]] bool can_fill_completely(const Order* order) const;
};

} // namespace orderbook
