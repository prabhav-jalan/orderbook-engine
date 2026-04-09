#include "orderbook/price_level.hpp"

namespace orderbook {

// ──────────────────────────────────────────────
// Move operations
// ──────────────────────────────────────────────

PriceLevel::PriceLevel(PriceLevel&& other) noexcept
    : head_{other.head_}
    , tail_{other.tail_}
    , total_quantity_{other.total_quantity_}
    , order_count_{other.order_count_}
{
    other.head_ = nullptr;
    other.tail_ = nullptr;
    other.total_quantity_ = 0;
    other.order_count_ = 0;
}

PriceLevel& PriceLevel::operator=(PriceLevel&& other) noexcept {
    if (this != &other) {
        head_           = other.head_;
        tail_           = other.tail_;
        total_quantity_ = other.total_quantity_;
        order_count_    = other.order_count_;

        other.head_ = nullptr;
        other.tail_ = nullptr;
        other.total_quantity_ = 0;
        other.order_count_ = 0;
    }
    return *this;
}

// ──────────────────────────────────────────────
// Core operations
// ──────────────────────────────────────────────

void PriceLevel::add_order(Order* order) {
    order->prev = tail_;
    order->next = nullptr;

    if (tail_ != nullptr) {
        tail_->next = order;
    } else {
        head_ = order;
    }

    tail_ = order;
    total_quantity_ += order->remaining_quantity();
    ++order_count_;
}

void PriceLevel::remove_order(Order* order) {
    if (order->prev != nullptr) {
        order->prev->next = order->next;
    } else {
        head_ = order->next;
    }

    if (order->next != nullptr) {
        order->next->prev = order->prev;
    } else {
        tail_ = order->prev;
    }

    order->prev = nullptr;
    order->next = nullptr;
    total_quantity_ -= order->remaining_quantity();
    --order_count_;
}

} // namespace orderbook
