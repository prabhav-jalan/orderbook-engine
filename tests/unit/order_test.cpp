#include <gtest/gtest.h>
#include "orderbook/types.hpp"

using namespace orderbook;

// ──────────────────────────────────────────────
// Order basics
// ──────────────────────────────────────────────

TEST(OrderTest, DefaultConstruction) {
    Order order{};
    EXPECT_EQ(order.id, 0);
    EXPECT_EQ(order.price, 0);
    EXPECT_EQ(order.quantity, 0);
    EXPECT_EQ(order.filled_quantity, 0);
    EXPECT_EQ(order.side, Side::Buy);
    EXPECT_EQ(order.type, OrderType::Limit);
    EXPECT_EQ(order.status, OrderStatus::New);
    EXPECT_EQ(order.prev, nullptr);
    EXPECT_EQ(order.next, nullptr);
}

TEST(OrderTest, RemainingQuantity) {
    Order order{};
    order.quantity = 100;
    order.filled_quantity = 35;
    EXPECT_EQ(order.remaining_quantity(), 65);
}

TEST(OrderTest, RemainingQuantityWhenUnfilled) {
    Order order{};
    order.quantity = 500;
    order.filled_quantity = 0;
    EXPECT_EQ(order.remaining_quantity(), 500);
}

TEST(OrderTest, IsFilledExact) {
    Order order{};
    order.quantity = 100;
    order.filled_quantity = 100;
    EXPECT_TRUE(order.is_filled());
}

TEST(OrderTest, IsFilledPartial) {
    Order order{};
    order.quantity = 100;
    order.filled_quantity = 50;
    EXPECT_FALSE(order.is_filled());
}

TEST(OrderTest, FixedPointPricing) {
    // $150.25 represented as 15025 ticks (tick size = $0.01)
    Order order{};
    order.price = 15025;
    EXPECT_EQ(order.price, 15025);

    // Verify arithmetic works correctly with fixed-point
    Price bid = 15025;
    Price ask = 15030;
    EXPECT_EQ(ask - bid, 5); // Spread = $0.05 = 5 ticks
}
