#include <gtest/gtest.h>
#include "orderbook/matching_engine.hpp"
#include <vector>
#include <memory>

using namespace orderbook;

// ──────────────────────────────────────────────
// Helpers: manage Order lifetimes for tests
// ──────────────────────────────────────────────

class MatchingEngineTest : public ::testing::Test {
protected:
    MatchingEngine engine;

    // Storage for orders — engine holds raw pointers, so we need
    // the Orders to outlive the engine operations.
    std::vector<std::unique_ptr<Order>> order_storage;

    Order* make_order(OrderId id, Side side, Price price, Quantity qty,
                      OrderType type = OrderType::Limit) {
        auto o = std::make_unique<Order>();
        o->id = id;
        o->side = side;
        o->price = price;
        o->quantity = qty;
        o->type = type;
        o->timestamp = id; // Use id as timestamp for simplicity
        Order* ptr = o.get();
        order_storage.push_back(std::move(o));
        return ptr;
    }

    Order* make_buy(OrderId id, Price price, Quantity qty,
                    OrderType type = OrderType::Limit) {
        return make_order(id, Side::Buy, price, qty, type);
    }

    Order* make_sell(OrderId id, Price price, Quantity qty,
                     OrderType type = OrderType::Limit) {
        return make_order(id, Side::Sell, price, qty, type);
    }
};

// ══════════════════════════════════════════════
// Limit orders — basic matching
// ══════════════════════════════════════════════

TEST_F(MatchingEngineTest, LimitOrderRestsWhenNoMatch) {
    auto* buy = make_buy(1, 10000, 100);
    auto result = engine.submit_order(buy);

    EXPECT_EQ(result.trades.size(), 0);
    EXPECT_TRUE(result.is_resting);
    EXPECT_FALSE(result.is_fully_filled);
    EXPECT_EQ(engine.book().order_count(), 1);
    EXPECT_EQ(engine.book().best_bid(), 10000);
}

TEST_F(MatchingEngineTest, ExactMatchBuyIntoSell) {
    // Resting sell at $100
    auto* sell = make_sell(1, 10000, 100);
    engine.submit_order(sell);

    // Incoming buy at $100 for exact quantity
    auto* buy = make_buy(2, 10000, 100);
    auto result = engine.submit_order(buy);

    EXPECT_EQ(result.trades.size(), 1);
    EXPECT_TRUE(result.is_fully_filled);
    EXPECT_FALSE(result.is_resting);

    auto& trade = result.trades[0];
    EXPECT_EQ(trade.buy_order_id, 2);
    EXPECT_EQ(trade.sell_order_id, 1);
    EXPECT_EQ(trade.price, 10000);
    EXPECT_EQ(trade.quantity, 100);

    // Book should be empty.
    EXPECT_EQ(engine.book().order_count(), 0);
    EXPECT_EQ(engine.book().best_bid(), std::nullopt);
    EXPECT_EQ(engine.book().best_ask(), std::nullopt);
}

TEST_F(MatchingEngineTest, ExactMatchSellIntoBuy) {
    auto* buy = make_buy(1, 10000, 100);
    engine.submit_order(buy);

    auto* sell = make_sell(2, 10000, 100);
    auto result = engine.submit_order(sell);

    EXPECT_EQ(result.trades.size(), 1);
    EXPECT_TRUE(result.is_fully_filled);
    EXPECT_EQ(result.trades[0].buy_order_id, 1);
    EXPECT_EQ(result.trades[0].sell_order_id, 2);
    EXPECT_EQ(engine.book().order_count(), 0);
}

TEST_F(MatchingEngineTest, PartialFillRestingOrderRemains) {
    // Resting sell for 200
    auto* sell = make_sell(1, 10000, 200);
    engine.submit_order(sell);

    // Buy only 60
    auto* buy = make_buy(2, 10000, 60);
    auto result = engine.submit_order(buy);

    EXPECT_EQ(result.trades.size(), 1);
    EXPECT_TRUE(result.is_fully_filled); // Buy is fully filled
    EXPECT_EQ(result.trades[0].quantity, 60);

    // Resting sell should have 140 remaining.
    EXPECT_EQ(sell->filled_quantity, 60);
    EXPECT_EQ(sell->remaining_quantity(), 140);
    EXPECT_EQ(engine.book().order_count(), 1);
    EXPECT_EQ(engine.book().volume_at_price(Side::Sell, 10000), 140);
}

TEST_F(MatchingEngineTest, PartialFillIncomingOrderRests) {
    // Resting sell for 50
    auto* sell = make_sell(1, 10000, 50);
    engine.submit_order(sell);

    // Buy 200 — only 50 can be filled, 150 rests.
    auto* buy = make_buy(2, 10000, 200);
    auto result = engine.submit_order(buy);

    EXPECT_EQ(result.trades.size(), 1);
    EXPECT_EQ(result.trades[0].quantity, 50);
    EXPECT_TRUE(result.is_resting); // Remainder rests
    EXPECT_FALSE(result.is_fully_filled);
    EXPECT_EQ(result.filled_quantity, 50);

    EXPECT_EQ(engine.book().order_count(), 1); // Only the buy remains
    EXPECT_EQ(engine.book().best_bid(), 10000);
    EXPECT_EQ(engine.book().volume_at_price(Side::Buy, 10000), 150);
}

// ══════════════════════════════════════════════
// Price improvement
// ══════════════════════════════════════════════

TEST_F(MatchingEngineTest, PriceImprovementForBuyer) {
    // Sell resting at $100
    auto* sell = make_sell(1, 10000, 100);
    engine.submit_order(sell);

    // Buy willing to pay $105 — should fill at $100 (resting price).
    auto* buy = make_buy(2, 10500, 100);
    auto result = engine.submit_order(buy);

    EXPECT_EQ(result.trades[0].price, 10000); // Not 10500!
}

TEST_F(MatchingEngineTest, PriceImprovementForSeller) {
    // Buy resting at $105
    auto* buy = make_buy(1, 10500, 100);
    engine.submit_order(buy);

    // Sell willing to accept $100 — fills at $105 (resting price).
    auto* sell = make_sell(2, 10000, 100);
    auto result = engine.submit_order(sell);

    EXPECT_EQ(result.trades[0].price, 10500); // Not 10000!
}

// ══════════════════════════════════════════════
// Multi-level sweep
// ══════════════════════════════════════════════

TEST_F(MatchingEngineTest, BuySweepsMultipleAskLevels) {
    // Three ask levels:
    // $100: 50 shares
    // $101: 30 shares
    // $102: 40 shares
    auto* a1 = make_sell(1, 10000, 50);
    auto* a2 = make_sell(2, 10100, 30);
    auto* a3 = make_sell(3, 10200, 40);
    engine.submit_order(a1);
    engine.submit_order(a2);
    engine.submit_order(a3);

    // Buy 100 at $102 — sweeps through all three levels.
    auto* buy = make_buy(4, 10200, 100);
    auto result = engine.submit_order(buy);

    EXPECT_EQ(result.trades.size(), 3);
    EXPECT_TRUE(result.is_fully_filled);

    // Trade 1: 50 @ $100
    EXPECT_EQ(result.trades[0].price, 10000);
    EXPECT_EQ(result.trades[0].quantity, 50);

    // Trade 2: 30 @ $101
    EXPECT_EQ(result.trades[1].price, 10100);
    EXPECT_EQ(result.trades[1].quantity, 30);

    // Trade 3: 20 @ $102 (only 20 more needed)
    EXPECT_EQ(result.trades[2].price, 10200);
    EXPECT_EQ(result.trades[2].quantity, 20);

    // 20 shares should remain at $102.
    EXPECT_EQ(engine.book().order_count(), 1);
    EXPECT_EQ(engine.book().volume_at_price(Side::Sell, 10200), 20);
}

TEST_F(MatchingEngineTest, SellSweepsMultipleBidLevels) {
    auto* b1 = make_buy(1, 10200, 30);
    auto* b2 = make_buy(2, 10100, 40);
    auto* b3 = make_buy(3, 10000, 50);
    engine.submit_order(b1);
    engine.submit_order(b2);
    engine.submit_order(b3);

    // Sell 60 at $100 — sweeps highest bids first.
    auto* sell = make_sell(4, 10000, 60);
    auto result = engine.submit_order(sell);

    EXPECT_EQ(result.trades.size(), 2);
    EXPECT_TRUE(result.is_fully_filled);

    // Trade 1: 30 @ $102 (highest bid first)
    EXPECT_EQ(result.trades[0].price, 10200);
    EXPECT_EQ(result.trades[0].quantity, 30);

    // Trade 2: 30 @ $101
    EXPECT_EQ(result.trades[1].price, 10100);
    EXPECT_EQ(result.trades[1].quantity, 30);

    // $101 should have 10 remaining, $100 untouched.
    EXPECT_EQ(engine.book().volume_at_price(Side::Buy, 10100), 10);
    EXPECT_EQ(engine.book().volume_at_price(Side::Buy, 10000), 50);
}

// ══════════════════════════════════════════════
// FIFO ordering within a price level
// ══════════════════════════════════════════════

TEST_F(MatchingEngineTest, FIFOWithinPriceLevel) {
    // Three sells at the same price.
    auto* s1 = make_sell(1, 10000, 30);
    auto* s2 = make_sell(2, 10000, 40);
    auto* s3 = make_sell(3, 10000, 50);
    engine.submit_order(s1);
    engine.submit_order(s2);
    engine.submit_order(s3);

    // Buy 50 — should fill s1 (30) then partial s2 (20).
    auto* buy = make_buy(4, 10000, 50);
    auto result = engine.submit_order(buy);

    EXPECT_EQ(result.trades.size(), 2);

    // First fill: s1 completely (30)
    EXPECT_EQ(result.trades[0].sell_order_id, 1);
    EXPECT_EQ(result.trades[0].quantity, 30);

    // Second fill: s2 partially (20)
    EXPECT_EQ(result.trades[1].sell_order_id, 2);
    EXPECT_EQ(result.trades[1].quantity, 20);

    // s1 is gone, s2 has 20 remaining, s3 is untouched.
    EXPECT_FALSE(engine.book().contains(1));
    EXPECT_EQ(s2->remaining_quantity(), 20);
    EXPECT_EQ(s3->remaining_quantity(), 50);
}

// ══════════════════════════════════════════════
// No match — price doesn't cross
// ══════════════════════════════════════════════

TEST_F(MatchingEngineTest, NoMatchWhenPriceDoesNotCross) {
    auto* sell = make_sell(1, 10100, 100); // Ask at $101
    engine.submit_order(sell);

    auto* buy = make_buy(2, 10000, 100); // Bid at $100
    auto result = engine.submit_order(buy);

    // No trade — spread is $1.
    EXPECT_EQ(result.trades.size(), 0);
    EXPECT_TRUE(result.is_resting);
    EXPECT_EQ(engine.book().order_count(), 2);
    EXPECT_EQ(engine.book().best_bid(), 10000);
    EXPECT_EQ(engine.book().best_ask(), 10100);
}

// ══════════════════════════════════════════════
// Market orders
// ══════════════════════════════════════════════

TEST_F(MatchingEngineTest, MarketBuyMatchesAtAnyPrice) {
    auto* sell = make_sell(1, 99999, 100); // Very expensive
    engine.submit_order(sell);

    auto* buy = make_buy(2, 0, 100, OrderType::Market);
    auto result = engine.submit_order(buy);

    EXPECT_EQ(result.trades.size(), 1);
    EXPECT_TRUE(result.is_fully_filled);
    EXPECT_EQ(result.trades[0].price, 99999);
}

TEST_F(MatchingEngineTest, MarketOrderCancelsUnfilledRemainder) {
    auto* sell = make_sell(1, 10000, 30);
    engine.submit_order(sell);

    // Market buy for 100, only 30 available.
    auto* buy = make_buy(2, 0, 100, OrderType::Market);
    auto result = engine.submit_order(buy);

    EXPECT_EQ(result.trades.size(), 1);
    EXPECT_EQ(result.filled_quantity, 30);
    EXPECT_TRUE(result.is_cancelled); // Remainder cancelled
    EXPECT_FALSE(result.is_resting);  // Market orders never rest
    EXPECT_EQ(engine.book().order_count(), 0);
}

TEST_F(MatchingEngineTest, MarketOrderNoLiquidity) {
    // Empty book.
    auto* buy = make_buy(1, 0, 100, OrderType::Market);
    auto result = engine.submit_order(buy);

    EXPECT_EQ(result.trades.size(), 0);
    EXPECT_TRUE(result.is_cancelled);
    EXPECT_EQ(engine.book().order_count(), 0);
}

// ══════════════════════════════════════════════
// IOC (Immediate or Cancel)
// ══════════════════════════════════════════════

TEST_F(MatchingEngineTest, IOCFullFill) {
    auto* sell = make_sell(1, 10000, 100);
    engine.submit_order(sell);

    auto* buy = make_buy(2, 10000, 100, OrderType::IOC);
    auto result = engine.submit_order(buy);

    EXPECT_EQ(result.trades.size(), 1);
    EXPECT_TRUE(result.is_fully_filled);
    EXPECT_FALSE(result.is_cancelled);
}

TEST_F(MatchingEngineTest, IOCPartialFillCancelsRemainder) {
    auto* sell = make_sell(1, 10000, 40);
    engine.submit_order(sell);

    auto* buy = make_buy(2, 10000, 100, OrderType::IOC);
    auto result = engine.submit_order(buy);

    EXPECT_EQ(result.trades.size(), 1);
    EXPECT_EQ(result.filled_quantity, 40);
    EXPECT_TRUE(result.is_cancelled); // 60 unfilled → cancelled
    EXPECT_FALSE(result.is_resting);  // IOC never rests
}

TEST_F(MatchingEngineTest, IOCNoLiquidity) {
    auto* buy = make_buy(1, 10000, 100, OrderType::IOC);
    auto result = engine.submit_order(buy);

    EXPECT_EQ(result.trades.size(), 0);
    EXPECT_TRUE(result.is_cancelled);
    EXPECT_EQ(engine.book().order_count(), 0);
}

// ══════════════════════════════════════════════
// FOK (Fill or Kill)
// ══════════════════════════════════════════════

TEST_F(MatchingEngineTest, FOKFullFill) {
    auto* s1 = make_sell(1, 10000, 60);
    auto* s2 = make_sell(2, 10000, 40);
    engine.submit_order(s1);
    engine.submit_order(s2);

    // FOK for 100 — exactly 100 available at $100.
    auto* buy = make_buy(3, 10000, 100, OrderType::FOK);
    auto result = engine.submit_order(buy);

    EXPECT_EQ(result.trades.size(), 2);
    EXPECT_TRUE(result.is_fully_filled);
    EXPECT_FALSE(result.is_cancelled);
    EXPECT_EQ(engine.book().order_count(), 0);
}

TEST_F(MatchingEngineTest, FOKRejectedInsufficientQuantity) {
    auto* sell = make_sell(1, 10000, 80);
    engine.submit_order(sell);

    // FOK for 100 — only 80 available → rejected, no trades.
    auto* buy = make_buy(2, 10000, 100, OrderType::FOK);
    auto result = engine.submit_order(buy);

    EXPECT_EQ(result.trades.size(), 0);
    EXPECT_TRUE(result.is_cancelled);
    EXPECT_FALSE(result.is_fully_filled);

    // The resting sell should be untouched.
    EXPECT_EQ(engine.book().order_count(), 1);
    EXPECT_EQ(sell->filled_quantity, 0);
}

TEST_F(MatchingEngineTest, FOKRejectedPriceTooHigh) {
    auto* sell = make_sell(1, 10100, 100);
    engine.submit_order(sell);

    // FOK buy at $100, ask is at $101 — price doesn't cross.
    auto* buy = make_buy(2, 10000, 100, OrderType::FOK);
    auto result = engine.submit_order(buy);

    EXPECT_EQ(result.trades.size(), 0);
    EXPECT_TRUE(result.is_cancelled);
    EXPECT_EQ(engine.book().order_count(), 1); // Sell untouched
}

TEST_F(MatchingEngineTest, FOKAcrossMultipleLevels) {
    auto* s1 = make_sell(1, 10000, 50);
    auto* s2 = make_sell(2, 10100, 50);
    engine.submit_order(s1);
    engine.submit_order(s2);

    // FOK buy for 100 at $101 — 50 at $100 + 50 at $101 = 100, OK.
    auto* buy = make_buy(3, 10100, 100, OrderType::FOK);
    auto result = engine.submit_order(buy);

    EXPECT_EQ(result.trades.size(), 2);
    EXPECT_TRUE(result.is_fully_filled);
}

// ══════════════════════════════════════════════
// Cancel via matching engine
// ══════════════════════════════════════════════

TEST_F(MatchingEngineTest, CancelRestingOrder) {
    auto* buy = make_buy(1, 10000, 100);
    engine.submit_order(buy);

    EXPECT_TRUE(engine.cancel_order(1));
    EXPECT_EQ(engine.book().order_count(), 0);
}

TEST_F(MatchingEngineTest, CancelNonexistent) {
    EXPECT_FALSE(engine.cancel_order(999));
}

// ══════════════════════════════════════════════
// Complex scenarios
// ══════════════════════════════════════════════

TEST_F(MatchingEngineTest, MultipleTradesAndResidual) {
    // Build an ask book:
    // $100: 100 (orders 1, 2 — 50 each)
    // $101: 80  (order 3)
    // $102: 200 (order 4)
    auto* s1 = make_sell(1, 10000, 50);
    auto* s2 = make_sell(2, 10000, 50);
    auto* s3 = make_sell(3, 10100, 80);
    auto* s4 = make_sell(4, 10200, 200);
    engine.submit_order(s1);
    engine.submit_order(s2);
    engine.submit_order(s3);
    engine.submit_order(s4);

    // Buy 250 at $102 → sweeps $100 (100), $101 (80), $102 (70 of 200)
    auto* buy = make_buy(5, 10200, 250);
    auto result = engine.submit_order(buy);

    EXPECT_EQ(result.trades.size(), 4); // s1, s2, s3, partial s4
    EXPECT_TRUE(result.is_fully_filled);
    EXPECT_EQ(result.filled_quantity, 250);

    // Verify trade prices follow level order.
    EXPECT_EQ(result.trades[0].price, 10000);
    EXPECT_EQ(result.trades[0].quantity, 50);
    EXPECT_EQ(result.trades[1].price, 10000);
    EXPECT_EQ(result.trades[1].quantity, 50);
    EXPECT_EQ(result.trades[2].price, 10100);
    EXPECT_EQ(result.trades[2].quantity, 80);
    EXPECT_EQ(result.trades[3].price, 10200);
    EXPECT_EQ(result.trades[3].quantity, 70);

    // s4 should have 130 remaining.
    EXPECT_EQ(s4->remaining_quantity(), 130);
    EXPECT_EQ(engine.book().order_count(), 1);
    EXPECT_EQ(engine.book().best_ask(), 10200);
}

TEST_F(MatchingEngineTest, InterleavedBuysAndSells) {
    // Simulate a real trading sequence.
    auto* b1 = make_buy(1, 10000, 100);
    engine.submit_order(b1); // Rests

    auto* b2 = make_buy(2, 9950, 200);
    engine.submit_order(b2); // Rests

    auto* s1 = make_sell(3, 10050, 150);
    engine.submit_order(s1); // Rests (no cross)

    // Now a sell comes in at $100 — matches the buy at $100.
    auto* s2 = make_sell(4, 10000, 60);
    auto result = engine.submit_order(s2);

    EXPECT_EQ(result.trades.size(), 1);
    EXPECT_EQ(result.trades[0].price, 10000);
    EXPECT_EQ(result.trades[0].quantity, 60);
    EXPECT_TRUE(result.is_fully_filled);

    // b1 partially filled (40 remaining).
    EXPECT_EQ(b1->remaining_quantity(), 40);

    // Book state: bid at $100 (40), bid at $99.50 (200), ask at $100.50 (150)
    EXPECT_EQ(engine.book().best_bid(), 10000);
    EXPECT_EQ(engine.book().best_ask(), 10050);
    EXPECT_EQ(engine.book().order_count(), 3);
}
