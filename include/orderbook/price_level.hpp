#pragma once

#include "orderbook/types.hpp"
#include <cstddef>

namespace orderbook {

/// A FIFO queue of all resting orders at a single price.
///
/// Implemented as an intrusive doubly-linked list using the prev/next
/// pointers embedded in each Order. This gives us:
///   - O(1) append to tail  (new orders go to the back of the queue)
///   - O(1) removal of any order (given a pointer to it)
///   - O(1) total quantity lookup (cached, updated incrementally)
///   - Zero heap allocations (no separate list nodes)
///
/// The FIFO ordering ensures price-time priority within a price level:
/// the order that arrived first is at the head and gets filled first.
class PriceLevel {
public:
    PriceLevel() = default;

    // Non-copyable — orders hold raw pointers into this level's list
    PriceLevel(const PriceLevel&) = delete;
    PriceLevel& operator=(const PriceLevel&) = delete;

    // Movable — needed for std::map emplacement
    PriceLevel(PriceLevel&& other) noexcept;
    PriceLevel& operator=(PriceLevel&& other) noexcept;

    ~PriceLevel() = default;

    /// Append an order to the back of the FIFO queue.
    /// The order's prev/next pointers will be modified.
    /// Precondition: order is not already in any list.
    void add_order(Order* order);

    /// Remove an order from the queue.
    /// The order's prev/next pointers will be reset to nullptr.
    /// Precondition: order is currently in THIS level's list.
    void remove_order(Order* order);

    /// The first order in the queue (earliest arrival). nullptr if empty.
    [[nodiscard]] Order* front() const noexcept { return head_; }

    /// The last order in the queue. nullptr if empty.
    [[nodiscard]] Order* back() const noexcept { return tail_; }

    /// Total remaining quantity across all orders at this price.
    [[nodiscard]] Quantity total_quantity() const noexcept { return total_quantity_; }

    /// Number of orders at this price level.
    [[nodiscard]] std::size_t order_count() const noexcept { return order_count_; }

    /// True if no orders are resting at this price.
    [[nodiscard]] bool empty() const noexcept { return head_ == nullptr; }

    /// Reduce the cached total quantity by a fill amount.
    /// Called by the matching engine when a resting order is partially filled,
    /// so we can update the level's aggregate without a costly remove+add.
    void reduce_total_quantity(Quantity filled) noexcept { total_quantity_ -= filled; }

private:
    Order*      head_{nullptr};
    Order*      tail_{nullptr};
    Quantity    total_quantity_{0};
    std::size_t order_count_{0};
};

} // namespace orderbook
