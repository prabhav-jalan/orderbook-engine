#include <gtest/gtest.h>
#include "orderbook/order_book.hpp"
#include <vector>

using namespace orderbook;

// ──────────────────────────────────────────────
// Helper: create orders conveniently
// ──────────────────────────────────────────────
static Order make_buy(OrderId id, Price price, Quantity qty) {
    Order o{};
    o.id = id;
    o.price = price;
    o.quantity = qty;
    o.side = Side::Buy;
    return o;
}

static Order make_sell(OrderId id, Price price, Quantity qty) {
    Order o{};
    o.id = id;
    o.price = price;
    o.quantity = qty;
    o.side = Side::Sell;
    return o;
}

// ──────────────────────────────────────────────
// Empty book
// ──────────────────────────────────────────────

TEST(OrderBookTest, EmptyBook) {
    OrderBook book;
    EXPECT_EQ(book.order_count(), 0);
    EXPECT_EQ(book.best_bid(), std::nullopt);
    EXPECT_EQ(book.best_ask(), std::nullopt);
    EXPECT_EQ(book.level_count(Side::Buy), 0);
    EXPECT_EQ(book.level_count(Side::Sell), 0);
}

// ──────────────────────────────────────────────
// Adding orders
// ──────────────────────────────────────────────

TEST(OrderBookTest, AddSingleBid) {
    OrderBook book;
    Order bid = make_buy(1, 10000, 100);

    auto report = book.add_order(&bid);

    EXPECT_EQ(report.type, ExecutionReport::Type::Accepted);
    EXPECT_EQ(report.order_id, 1);
    EXPECT_EQ(book.order_count(), 1);
    EXPECT_EQ(book.best_bid(), 10000);
    EXPECT_EQ(book.best_ask(), std::nullopt);
    EXPECT_EQ(book.volume_at_price(Side::Buy, 10000), 100);
}

TEST(OrderBookTest, AddSingleAsk) {
    OrderBook book;
    Order ask = make_sell(1, 10050, 200);

    book.add_order(&ask);

    EXPECT_EQ(book.best_ask(), 10050);
    EXPECT_EQ(book.best_bid(), std::nullopt);
    EXPECT_EQ(book.volume_at_price(Side::Sell, 10050), 200);
}

TEST(OrderBookTest, BestBidIsHighestPrice) {
    OrderBook book;
    Order b1 = make_buy(1, 10000, 100);
    Order b2 = make_buy(2, 10050, 100);
    Order b3 = make_buy(3, 9950, 100);

    book.add_order(&b1);
    book.add_order(&b2);
    book.add_order(&b3);

    EXPECT_EQ(book.best_bid(), 10050); // Highest bid wins
    EXPECT_EQ(book.level_count(Side::Buy), 3);
}

TEST(OrderBookTest, BestAskIsLowestPrice) {
    OrderBook book;
    Order a1 = make_sell(1, 10100, 100);
    Order a2 = make_sell(2, 10050, 100);
    Order a3 = make_sell(3, 10200, 100);

    book.add_order(&a1);
    book.add_order(&a2);
    book.add_order(&a3);

    EXPECT_EQ(book.best_ask(), 10050); // Lowest ask wins
    EXPECT_EQ(book.level_count(Side::Sell), 3);
}

TEST(OrderBookTest, MultipleOrdersSamePrice) {
    OrderBook book;
    Order b1 = make_buy(1, 10000, 100);
    Order b2 = make_buy(2, 10000, 200);
    Order b3 = make_buy(3, 10000, 50);

    book.add_order(&b1);
    book.add_order(&b2);
    book.add_order(&b3);

    EXPECT_EQ(book.level_count(Side::Buy), 1); // All at same price level
    EXPECT_EQ(book.volume_at_price(Side::Buy, 10000), 350);
    EXPECT_EQ(book.order_count(), 3);
}

// ──────────────────────────────────────────────
// Cancelling orders
// ──────────────────────────────────────────────

TEST(OrderBookTest, CancelOrder) {
    OrderBook book;
    Order bid = make_buy(1, 10000, 100);
    book.add_order(&bid);

    auto report = book.cancel_order(1);

    ASSERT_TRUE(report.has_value());
    EXPECT_EQ(report->type, ExecutionReport::Type::Cancelled);
    EXPECT_EQ(book.order_count(), 0);
    EXPECT_EQ(book.best_bid(), std::nullopt);
    EXPECT_EQ(bid.status, OrderStatus::Cancelled);
}

TEST(OrderBookTest, CancelNonexistentOrder) {
    OrderBook book;
    auto report = book.cancel_order(999);
    EXPECT_FALSE(report.has_value());
}

TEST(OrderBookTest, CancelRemovesEmptyPriceLevel) {
    OrderBook book;
    Order bid = make_buy(1, 10000, 100);
    book.add_order(&bid);

    book.cancel_order(1);

    EXPECT_EQ(book.level_count(Side::Buy), 0);
    EXPECT_EQ(book.volume_at_price(Side::Buy, 10000), 0);
}

TEST(OrderBookTest, CancelOneOfMultipleAtSamePrice) {
    OrderBook book;
    Order b1 = make_buy(1, 10000, 100);
    Order b2 = make_buy(2, 10000, 200);

    book.add_order(&b1);
    book.add_order(&b2);

    book.cancel_order(1);

    EXPECT_EQ(book.level_count(Side::Buy), 1);  // Level still exists
    EXPECT_EQ(book.volume_at_price(Side::Buy, 10000), 200);
    EXPECT_EQ(book.order_count(), 1);
    EXPECT_FALSE(book.contains(1));
    EXPECT_TRUE(book.contains(2));
}

TEST(OrderBookTest, CancelBestBidRevealNextBest) {
    OrderBook book;
    Order b1 = make_buy(1, 10050, 100); // Best bid
    Order b2 = make_buy(2, 10000, 100); // Second best

    book.add_order(&b1);
    book.add_order(&b2);

    EXPECT_EQ(book.best_bid(), 10050);

    book.cancel_order(1);

    EXPECT_EQ(book.best_bid(), 10000); // Second best is now the best
}

// ──────────────────────────────────────────────
// Modifying orders
// ──────────────────────────────────────────────

TEST(OrderBookTest, ModifyReduceQuantity) {
    OrderBook book;
    Order bid = make_buy(1, 10000, 100);
    book.add_order(&bid);

    auto report = book.modify_order(1, 60);

    ASSERT_TRUE(report.has_value());
    EXPECT_EQ(report->type, ExecutionReport::Type::Modified);
    EXPECT_EQ(book.volume_at_price(Side::Buy, 10000), 60);
    EXPECT_EQ(bid.quantity, 60);
}

TEST(OrderBookTest, ModifyToZeroCancels) {
    OrderBook book;
    Order bid = make_buy(1, 10000, 100);
    book.add_order(&bid);

    auto report = book.modify_order(1, 0);

    ASSERT_TRUE(report.has_value());
    EXPECT_EQ(report->type, ExecutionReport::Type::Cancelled);
    EXPECT_EQ(book.order_count(), 0);
}

TEST(OrderBookTest, ModifyNonexistentOrder) {
    OrderBook book;
    auto report = book.modify_order(999, 50);
    EXPECT_FALSE(report.has_value());
}

// ──────────────────────────────────────────────
// Queries
// ──────────────────────────────────────────────

TEST(OrderBookTest, VolumeAtPriceNoOrders) {
    OrderBook book;
    EXPECT_EQ(book.volume_at_price(Side::Buy, 10000), 0);
    EXPECT_EQ(book.volume_at_price(Side::Sell, 10000), 0);
}

TEST(OrderBookTest, ContainsAndFindOrder) {
    OrderBook book;
    Order bid = make_buy(1, 10000, 100);
    book.add_order(&bid);

    EXPECT_TRUE(book.contains(1));
    EXPECT_FALSE(book.contains(2));
    EXPECT_EQ(book.find_order(1), &bid);
    EXPECT_EQ(book.find_order(2), nullptr);
}

// ──────────────────────────────────────────────
// Full book scenario
// ──────────────────────────────────────────────

TEST(OrderBookTest, RealisticBookBuildup) {
    OrderBook book;

    // Build a realistic book:
    // Bids: 100.00, 99.95, 99.90
    // Asks: 100.05, 100.10, 100.15
    Order b1 = make_buy(1, 10000, 500);
    Order b2 = make_buy(2, 9995,  300);
    Order b3 = make_buy(3, 9990,  200);
    Order b4 = make_buy(4, 10000, 100); // Second order at best bid

    Order a1 = make_sell(5, 10005, 400);
    Order a2 = make_sell(6, 10010, 250);
    Order a3 = make_sell(7, 10015, 150);
    Order a4 = make_sell(8, 10005, 200); // Second order at best ask

    book.add_order(&b1);
    book.add_order(&b2);
    book.add_order(&b3);
    book.add_order(&b4);
    book.add_order(&a1);
    book.add_order(&a2);
    book.add_order(&a3);
    book.add_order(&a4);

    // Verify the book state.
    EXPECT_EQ(book.order_count(), 8);
    EXPECT_EQ(book.best_bid(), 10000);
    EXPECT_EQ(book.best_ask(), 10005);
    EXPECT_EQ(book.level_count(Side::Buy), 3);
    EXPECT_EQ(book.level_count(Side::Sell), 3);
    EXPECT_EQ(book.volume_at_price(Side::Buy, 10000), 600);  // 500 + 100
    EXPECT_EQ(book.volume_at_price(Side::Sell, 10005), 600);  // 400 + 200

    // Spread = best ask - best bid = 100.05 - 100.00 = 0.05 = 5 ticks
    EXPECT_EQ(*book.best_ask() - *book.best_bid(), 5);

    // Cancel the best bid entirely.
    book.cancel_order(1); // Remove 500 from 100.00
    book.cancel_order(4); // Remove 100 from 100.00

    EXPECT_EQ(book.best_bid(), 9995); // Next best bid
    EXPECT_EQ(book.level_count(Side::Buy), 2); // 100.00 level gone
}
