#include <gtest/gtest.h>
#include "orderbook/price_level.hpp"
#include <vector>

using namespace orderbook;

// ──────────────────────────────────────────────
// Helper: create an order with given id and quantity
// ──────────────────────────────────────────────
static Order make_order(OrderId id, Quantity qty, Price price = 10000) {
    Order o{};
    o.id = id;
    o.price = price;
    o.quantity = qty;
    o.side = Side::Buy;
    return o;
}

// ──────────────────────────────────────────────
// Basic operations
// ──────────────────────────────────────────────

TEST(PriceLevelTest, EmptyByDefault) {
    PriceLevel level;
    EXPECT_TRUE(level.empty());
    EXPECT_EQ(level.order_count(), 0);
    EXPECT_EQ(level.total_quantity(), 0);
    EXPECT_EQ(level.front(), nullptr);
    EXPECT_EQ(level.back(), nullptr);
}

TEST(PriceLevelTest, AddSingleOrder) {
    PriceLevel level;
    Order order = make_order(1, 100);

    level.add_order(&order);

    EXPECT_FALSE(level.empty());
    EXPECT_EQ(level.order_count(), 1);
    EXPECT_EQ(level.total_quantity(), 100);
    EXPECT_EQ(level.front(), &order);
    EXPECT_EQ(level.back(), &order);
}

TEST(PriceLevelTest, AddMultipleOrdersFIFO) {
    PriceLevel level;
    Order o1 = make_order(1, 100);
    Order o2 = make_order(2, 200);
    Order o3 = make_order(3, 50);

    level.add_order(&o1);
    level.add_order(&o2);
    level.add_order(&o3);

    // Head should be the first order added (FIFO).
    EXPECT_EQ(level.front(), &o1);
    EXPECT_EQ(level.back(), &o3);
    EXPECT_EQ(level.order_count(), 3);
    EXPECT_EQ(level.total_quantity(), 350);

    // Walk the list and verify FIFO ordering.
    EXPECT_EQ(o1.next, &o2);
    EXPECT_EQ(o2.next, &o3);
    EXPECT_EQ(o3.next, nullptr);
    EXPECT_EQ(o3.prev, &o2);
    EXPECT_EQ(o2.prev, &o1);
    EXPECT_EQ(o1.prev, nullptr);
}

// ──────────────────────────────────────────────
// Removal
// ──────────────────────────────────────────────

TEST(PriceLevelTest, RemoveOnlyOrder) {
    PriceLevel level;
    Order order = make_order(1, 100);

    level.add_order(&order);
    level.remove_order(&order);

    EXPECT_TRUE(level.empty());
    EXPECT_EQ(level.order_count(), 0);
    EXPECT_EQ(level.total_quantity(), 0);
    EXPECT_EQ(level.front(), nullptr);
    EXPECT_EQ(level.back(), nullptr);
    EXPECT_EQ(order.prev, nullptr);
    EXPECT_EQ(order.next, nullptr);
}

TEST(PriceLevelTest, RemoveHead) {
    PriceLevel level;
    Order o1 = make_order(1, 100);
    Order o2 = make_order(2, 200);
    Order o3 = make_order(3, 50);

    level.add_order(&o1);
    level.add_order(&o2);
    level.add_order(&o3);

    level.remove_order(&o1);

    EXPECT_EQ(level.front(), &o2);
    EXPECT_EQ(level.back(), &o3);
    EXPECT_EQ(level.order_count(), 2);
    EXPECT_EQ(level.total_quantity(), 250);
    EXPECT_EQ(o2.prev, nullptr); // o2 is now the head
}

TEST(PriceLevelTest, RemoveTail) {
    PriceLevel level;
    Order o1 = make_order(1, 100);
    Order o2 = make_order(2, 200);
    Order o3 = make_order(3, 50);

    level.add_order(&o1);
    level.add_order(&o2);
    level.add_order(&o3);

    level.remove_order(&o3);

    EXPECT_EQ(level.front(), &o1);
    EXPECT_EQ(level.back(), &o2);
    EXPECT_EQ(level.order_count(), 2);
    EXPECT_EQ(level.total_quantity(), 300);
    EXPECT_EQ(o2.next, nullptr); // o2 is now the tail
}

TEST(PriceLevelTest, RemoveMiddle) {
    PriceLevel level;
    Order o1 = make_order(1, 100);
    Order o2 = make_order(2, 200);
    Order o3 = make_order(3, 50);

    level.add_order(&o1);
    level.add_order(&o2);
    level.add_order(&o3);

    level.remove_order(&o2);

    // o1 and o3 should now be directly linked.
    EXPECT_EQ(level.front(), &o1);
    EXPECT_EQ(level.back(), &o3);
    EXPECT_EQ(o1.next, &o3);
    EXPECT_EQ(o3.prev, &o1);
    EXPECT_EQ(level.order_count(), 2);
    EXPECT_EQ(level.total_quantity(), 150);
}

// ──────────────────────────────────────────────
// Quantity tracking with partial fills
// ──────────────────────────────────────────────

TEST(PriceLevelTest, QuantityReflectsRemainingNotTotal) {
    PriceLevel level;
    Order order = make_order(1, 100);
    order.filled_quantity = 30; // Only 70 remaining

    level.add_order(&order);

    EXPECT_EQ(level.total_quantity(), 70);
}

// ──────────────────────────────────────────────
// Stress: add and remove many orders
// ──────────────────────────────────────────────

TEST(PriceLevelTest, AddAndRemoveMany) {
    PriceLevel level;
    constexpr int N = 1000;
    std::vector<Order> orders(N);

    for (int i = 0; i < N; ++i) {
        orders[static_cast<std::size_t>(i)] = make_order(
            static_cast<OrderId>(i + 1), 10
        );
        level.add_order(&orders[static_cast<std::size_t>(i)]);
    }

    EXPECT_EQ(level.order_count(), N);
    EXPECT_EQ(level.total_quantity(), static_cast<Quantity>(N * 10));

    // Remove all orders from the front (simulating fills in FIFO order).
    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(level.front()->id, static_cast<OrderId>(i + 1));
        level.remove_order(level.front());
    }

    EXPECT_TRUE(level.empty());
    EXPECT_EQ(level.total_quantity(), 0);
}

// ──────────────────────────────────────────────
// Move semantics
// ──────────────────────────────────────────────

TEST(PriceLevelTest, MoveConstruction) {
    PriceLevel level;
    Order o1 = make_order(1, 100);
    Order o2 = make_order(2, 200);
    level.add_order(&o1);
    level.add_order(&o2);

    PriceLevel moved(std::move(level));

    EXPECT_EQ(moved.order_count(), 2);
    EXPECT_EQ(moved.total_quantity(), 300);
    EXPECT_EQ(moved.front(), &o1);

    // Source should be empty after move.
    EXPECT_TRUE(level.empty()); // NOLINT(bugprone-use-after-move)
}
