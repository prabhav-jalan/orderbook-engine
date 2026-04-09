// src/core/order_book.cpp
//
// OrderBook: manages bid and ask sides for a single instrument.
//
// Phase 1 implementation uses std::map for price levels. This gives us
// O(log N) insert/lookup with correct ordering out of the box.
// Phase 4 will explore array-backed levels for O(1) access.

#include "orderbook/order_book.hpp"

namespace orderbook {

// ──────────────────────────────────────────────
// Order management
// ──────────────────────────────────────────────

ExecutionReport OrderBook::add_order(Order* order) {
    // Index the order for O(1) cancel/modify lookups.
    orders_[order->id] = order;

    // Route to the correct side and emplace into the price level.
    // std::map::operator[] default-constructs a PriceLevel if none exists
    // at this price, which is exactly what we want.
    if (order->side == Side::Buy) {
        bids_[order->price].add_order(order);
    } else {
        asks_[order->price].add_order(order);
    }

    order->status = OrderStatus::Accepted;

    return ExecutionReport{ExecutionReport::Type::Accepted, order->id};
}

std::optional<ExecutionReport> OrderBook::cancel_order(OrderId order_id) {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        return std::nullopt;
    }

    Order* order = it->second;
    remove_order_from_level(order);
    orders_.erase(it);

    order->status = OrderStatus::Cancelled;

    return ExecutionReport{ExecutionReport::Type::Cancelled, order->id};
}

std::optional<ExecutionReport> OrderBook::modify_order(OrderId order_id, Quantity new_quantity) {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        return std::nullopt;
    }

    Order* order = it->second;

    // If new quantity is at or below what's already filled, cancel instead.
    if (new_quantity <= order->filled_quantity) {
        return cancel_order(order_id);
    }

    // Reducing quantity preserves time priority — we modify in place.
    // Strategy: remove from level (with old quantity), update the order,
    // then re-add (with new quantity). This keeps the level's cached
    // total_quantity correct without needing to expose internals.
    if (order->side == Side::Buy) {
        auto level_it = bids_.find(order->price);
        if (level_it != bids_.end()) {
            level_it->second.remove_order(order);
            order->quantity = new_quantity;
            level_it->second.add_order(order);

            if (level_it->second.empty()) {
                bids_.erase(level_it);
            }
        }
    } else {
        auto level_it = asks_.find(order->price);
        if (level_it != asks_.end()) {
            level_it->second.remove_order(order);
            order->quantity = new_quantity;
            level_it->second.add_order(order);

            if (level_it->second.empty()) {
                asks_.erase(level_it);
            }
        }
    }

    return ExecutionReport{ExecutionReport::Type::Modified, order->id};
}

// ──────────────────────────────────────────────
// Queries
// ──────────────────────────────────────────────

std::optional<Price> OrderBook::best_bid() const {
    if (bids_.empty()) {
        return std::nullopt;
    }
    // bids_ uses std::greater, so begin() is the highest price.
    return bids_.begin()->first;
}

std::optional<Price> OrderBook::best_ask() const {
    if (asks_.empty()) {
        return std::nullopt;
    }
    // asks_ uses default std::less, so begin() is the lowest price.
    return asks_.begin()->first;
}

Quantity OrderBook::volume_at_price(Side side, Price price) const {
    if (side == Side::Buy) {
        auto it = bids_.find(price);
        if (it != bids_.end()) {
            return it->second.total_quantity();
        }
    } else {
        auto it = asks_.find(price);
        if (it != asks_.end()) {
            return it->second.total_quantity();
        }
    }
    return 0;
}

std::size_t OrderBook::level_count(Side side) const {
    if (side == Side::Buy) {
        return bids_.size();
    }
    return asks_.size();
}

bool OrderBook::contains(OrderId order_id) const {
    return orders_.find(order_id) != orders_.end();
}

Order* OrderBook::find_order(OrderId order_id) const {
    auto it = orders_.find(order_id);
    if (it != orders_.end()) {
        return it->second;
    }
    return nullptr;
}

// ──────────────────────────────────────────────
// Level access (used by MatchingEngine)
// ──────────────────────────────────────────────

PriceLevel* OrderBook::best_bid_level() {
    if (bids_.empty()) return nullptr;
    return &bids_.begin()->second;
}

PriceLevel* OrderBook::best_ask_level() {
    if (asks_.empty()) return nullptr;
    return &asks_.begin()->second;
}

void OrderBook::remove_filled_order(Order* order) {
    orders_.erase(order->id);
    remove_order_from_level(order);
}

void OrderBook::rest_order(Order* order) {
    orders_[order->id] = order;
    if (order->side == Side::Buy) {
        bids_[order->price].add_order(order);
    } else {
        asks_[order->price].add_order(order);
    }
    order->status = OrderStatus::Accepted;
}

Quantity OrderBook::matchable_ask_quantity(Price limit_price, bool is_market) const {
    Quantity total = 0;
    for (const auto& [price, level] : asks_) {
        // asks_ is sorted ascending, so once price > limit, we're done.
        if (!is_market && price > limit_price) break;
        total += level.total_quantity();
    }
    return total;
}

Quantity OrderBook::matchable_bid_quantity(Price limit_price, bool is_market) const {
    Quantity total = 0;
    for (const auto& [price, level] : bids_) {
        // bids_ is sorted descending (std::greater), so once price < limit, done.
        if (!is_market && price < limit_price) break;
        total += level.total_quantity();
    }
    return total;
}

// ──────────────────────────────────────────────
// Internal helpers
// ──────────────────────────────────────────────

void OrderBook::remove_order_from_level(Order* order) {
    if (order->side == Side::Buy) {
        auto it = bids_.find(order->price);
        if (it != bids_.end()) {
            it->second.remove_order(order);
            // Remove empty price levels to keep the map clean.
            if (it->second.empty()) {
                bids_.erase(it);
            }
        }
    } else {
        auto it = asks_.find(order->price);
        if (it != asks_.end()) {
            it->second.remove_order(order);
            if (it->second.empty()) {
                asks_.erase(it);
            }
        }
    }
}

} // namespace orderbook
