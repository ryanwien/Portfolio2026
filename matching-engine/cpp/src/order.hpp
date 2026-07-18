// order.hpp — domain types for the matching engine.

#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>

namespace engine {

using OrderId = std::uint64_t;
using Quantity = std::int64_t;
using Price = double;

enum class Side { Buy, Sell };
enum class OrderType { Limit, Market };

/// An order as submitted. Once it rests, the book owns it and tracks the
/// remaining quantity here.
struct Order {
    OrderId id{};
    Side side{};
    OrderType type{OrderType::Limit};
    /// Unset for market orders, which take whatever the book offers.
    std::optional<Price> price{};
    Quantity quantity{};

    /// Validate the same invariants Order.__post_init__ enforces in Python.
    void validate() const {
        if (quantity <= 0) throw std::invalid_argument("quantity must be positive");
        if (type == OrderType::Limit && !price.has_value())
            throw std::invalid_argument("limit orders require a price");
        if (type == OrderType::Market && price.has_value())
            throw std::invalid_argument("market orders must not carry a price");
    }
};

/// A single execution. The price is the resting (maker) order's, so price
/// improvement accrues to the aggressor — standard exchange convention.
struct Trade {
    OrderId maker_order_id{};
    OrderId taker_order_id{};
    Price price{};
    Quantity quantity{};

    friend bool operator==(const Trade&, const Trade&) = default;
};

}  // namespace engine
