// src/core/price_level.cpp
//
// PriceLevel: FIFO queue of orders at a single price.
//
// This is the most performance-critical data structure in the order book.
// Every add, cancel, and match operation touches a PriceLevel.
// The intrusive list avoids heap allocations and keeps operations O(1).

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
    // Wire the new order to the end of the list.
    order->prev = tail_;
    order->next = nullptr;

    if (tail_ != nullptr) {
        // List is non-empty: link current tail to the new order.
        tail_->next = order;
    } else {
        // List was empty: new order is also the head.
        head_ = order;
    }

    tail_ = order;

    // Update cached aggregates.
    total_quantity_ += order->remaining_quantity();
    ++order_count_;
}

void PriceLevel::remove_order(Order* order) {
    // Unlink from the doubly-linked list.
    if (order->prev != nullptr) {
        order->prev->next = order->next;
    } else {
        // Removing the head — advance head to the next order.
        head_ = order->next;
    }

    if (order->next != nullptr) {
        order->next->prev = order->prev;
    } else {
        // Removing the tail — retreat tail to the previous order.
        tail_ = order->prev;
    }

    // Clean up the order's list pointers.
    order->prev = nullptr;
    order->next = nullptr;

    // Update cached aggregates.
    total_quantity_ -= order->remaining_quantity();
    --order_count_;
}

} // namespace orderbook
