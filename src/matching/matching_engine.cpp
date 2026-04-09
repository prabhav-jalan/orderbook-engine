#include "orderbook/matching_engine.hpp"
#include <algorithm>

namespace orderbook {

// ──────────────────────────────────────────────
// Public interface
// ──────────────────────────────────────────────

MatchResult MatchingEngine::submit_order(Order* order) {
    MatchResult result;
    result.order_id = order->id;

    // FOK: verify the entire quantity can be filled before executing.
    if (order->type == OrderType::FOK) {
        if (!can_fill_completely(order)) {
            order->status = OrderStatus::Cancelled;
            result.is_cancelled = true;
            return result;
        }
    }

    // Attempt matching against the opposite side.
    if (order->side == Side::Buy) {
        match_buy(order, result);
    } else {
        match_sell(order, result);
    }

    result.filled_quantity = order->filled_quantity;
    result.is_fully_filled = order->is_filled();

    // Handle the unfilled remainder based on order type.
    if (!order->is_filled()) {
        if (order->type == OrderType::Limit) {
            book_.rest_order(order);
            result.is_resting = true;
        } else {
            // Market, IOC: cancel the unfilled remainder.
            order->status = OrderStatus::Cancelled;
            result.is_cancelled = true;
        }
    }

    return result;
}

bool MatchingEngine::cancel_order(OrderId order_id) {
    auto report = book_.cancel_order(order_id);
    return report.has_value();
}

// ──────────────────────────────────────────────
// Matching logic
// ──────────────────────────────────────────────

void MatchingEngine::match_buy(Order* incoming, MatchResult& result) {
    while (!incoming->is_filled()) {
        auto best_ask = book_.best_ask();
        if (!best_ask.has_value()) break;

        if (incoming->type != OrderType::Market && incoming->price < *best_ask) {
            break;
        }

        PriceLevel* level = book_.best_ask_level();
        if (level == nullptr || level->empty()) break;

        while (!incoming->is_filled() && !level->empty()) {
            Order* resting = level->front();

            Quantity fill_qty = std::min(
                incoming->remaining_quantity(),
                resting->remaining_quantity()
            );

            Price fill_price = resting->price;

            incoming->filled_quantity += fill_qty;
            resting->filled_quantity += fill_qty;

            incoming->status = incoming->is_filled()
                ? OrderStatus::Filled
                : OrderStatus::PartiallyFilled;

            result.trades.push_back(Trade{
                .buy_order_id  = incoming->id,
                .sell_order_id = resting->id,
                .price         = fill_price,
                .quantity      = fill_qty,
                .timestamp     = incoming->timestamp
            });

            if (resting->is_filled()) {
                resting->status = OrderStatus::Filled;
                book_.remove_filled_order(resting);
            } else {
                resting->status = OrderStatus::PartiallyFilled;
                level->reduce_total_quantity(fill_qty);
            }
        }
    }
}

void MatchingEngine::match_sell(Order* incoming, MatchResult& result) {
    while (!incoming->is_filled()) {
        auto best_bid = book_.best_bid();
        if (!best_bid.has_value()) break;

        if (incoming->type != OrderType::Market && incoming->price > *best_bid) {
            break;
        }

        PriceLevel* level = book_.best_bid_level();
        if (level == nullptr || level->empty()) break;

        while (!incoming->is_filled() && !level->empty()) {
            Order* resting = level->front();

            Quantity fill_qty = std::min(
                incoming->remaining_quantity(),
                resting->remaining_quantity()
            );

            Price fill_price = resting->price;

            incoming->filled_quantity += fill_qty;
            resting->filled_quantity += fill_qty;

            incoming->status = incoming->is_filled()
                ? OrderStatus::Filled
                : OrderStatus::PartiallyFilled;

            result.trades.push_back(Trade{
                .buy_order_id  = resting->id,
                .sell_order_id = incoming->id,
                .price         = fill_price,
                .quantity      = fill_qty,
                .timestamp     = incoming->timestamp
            });

            if (resting->is_filled()) {
                resting->status = OrderStatus::Filled;
                book_.remove_filled_order(resting);
            } else {
                resting->status = OrderStatus::PartiallyFilled;
                level->reduce_total_quantity(fill_qty);
            }
        }
    }
}

// ──────────────────────────────────────────────
// FOK pre-check
// ──────────────────────────────────────────────

bool MatchingEngine::can_fill_completely(const Order* order) const {
    Quantity needed = order->remaining_quantity();
    bool is_market = (order->type == OrderType::Market);

    Quantity available = 0;
    if (order->side == Side::Buy) {
        available = book_.matchable_ask_quantity(order->price, is_market);
    } else {
        available = book_.matchable_bid_quantity(order->price, is_market);
    }

    return available >= needed;
}

} // namespace orderbook
