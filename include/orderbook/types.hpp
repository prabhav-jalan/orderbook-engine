#pragma once

#include <cstdint>
#include <string>

namespace orderbook {

// ──────────────────────────────────────────────
// Type aliases
// ──────────────────────────────────────────────

using OrderId   = uint64_t;
using Price     = int64_t;   // Fixed-point: price in ticks (e.g., $150.25 → 15025)
using Quantity  = uint32_t;
using Timestamp = uint64_t;  // Nanoseconds since epoch

// ──────────────────────────────────────────────
// Enums
// ──────────────────────────────────────────────

enum class Side : uint8_t {
    Buy,
    Sell
};

enum class OrderType : uint8_t {
    Limit,
    Market,
    IOC,    // Immediate or Cancel
    FOK     // Fill or Kill
};

enum class OrderStatus : uint8_t {
    New,
    Accepted,
    PartiallyFilled,
    Filled,
    Cancelled
};

// ──────────────────────────────────────────────
// Order
// ──────────────────────────────────────────────

struct Order {
    OrderId   id{0};
    Price     price{0};
    Quantity  quantity{0};
    Quantity  filled_quantity{0};
    Side      side{Side::Buy};
    OrderType type{OrderType::Limit};
    OrderStatus status{OrderStatus::New};
    Timestamp timestamp{0};

    // Intrusive list pointers (for PriceLevel queue)
    Order* prev{nullptr};
    Order* next{nullptr};

    [[nodiscard]] Quantity remaining_quantity() const noexcept {
        return quantity - filled_quantity;
    }

    [[nodiscard]] bool is_filled() const noexcept {
        return filled_quantity >= quantity;
    }
};

} // namespace orderbook
