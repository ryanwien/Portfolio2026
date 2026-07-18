// order_book.hpp — a price-time-priority limit order book.
//
// Structure
// ---------
//   bids : price -> PriceLevel, descending, so the best bid is begin()
//   asks : price -> PriceLevel, ascending,  so the best ask is begin()
//
// Each PriceLevel is a FIFO queue of resting orders, which is what gives time
// priority: append at the tail, match from the head. A separate id -> location
// map makes cancellation O(1) — the location record carries a pointer straight
// to the owning level and an iterator to the order, so nothing is scanned.
//
// Why not two heaps? A heap gives O(1) access to the best price but makes
// cancelling an arbitrary resting order O(n), and can't express time priority
// within a level without extra bookkeeping. Cancels dominate real order flow —
// most orders are cancelled, not filled — so the ordered-map-of-queues design
// is the one exchanges actually use.
//
// Complexity, with P distinct price levels:
//   submit, per level touched : O(log P)
//   best bid / best ask       : O(1)
//   cancel                    : O(1), plus O(log P) only when a level empties

#pragma once

#include "order.hpp"

#include <cstddef>
#include <list>
#include <map>
#include <unordered_map>
#include <vector>

namespace engine {

/// Aggregated view of one price level, for display.
struct DepthEntry {
    Price price{};
    Quantity quantity{};

    friend bool operator==(const DepthEntry&, const DepthEntry&) = default;
};

struct Depth {
    std::vector<DepthEntry> bids;
    std::vector<DepthEntry> asks;
};

class OrderBook {
public:
    /// Match an incoming order against the book, resting any unfilled
    /// remainder if it is a limit order. Returns the trades generated.
    std::vector<Trade> submit(Order order);

    /// Remove a resting order. False if it isn't on the book.
    bool cancel(OrderId id);

    std::optional<Price> best_bid() const;
    std::optional<Price> best_ask() const;
    std::optional<Price> spread() const;

    /// Aggregated top-of-book snapshot.
    Depth depth(std::size_t levels = 5) const;

    /// Number of orders currently resting.
    std::size_t resting_count() const { return locations_.size(); }

private:
    /// FIFO queue of orders resting at one price.
    struct PriceLevel {
        Price price{};
        std::list<Order> queue;
        Quantity total_quantity{};
    };

    // std::greater for bids so begin() is the highest price, matching asks'
    // begin() being the lowest. Both sides then read "best first".
    using BidMap = std::map<Price, PriceLevel, std::greater<Price>>;
    using AskMap = std::map<Price, PriceLevel, std::less<Price>>;

    /// Where a resting order lives. The level pointer is what keeps cancel
    /// O(1) — std::map nodes are stable, so this stays valid until the level
    /// itself is erased, which only happens once it is empty.
    struct Location {
        Side side{};
        Price price{};
        PriceLevel* level{};
        std::list<Order>::iterator it{};
    };

    template <typename BookSide>
    std::vector<Trade> match(Order& taker, BookSide& opposing, bool taker_is_buy);

    void rest(const Order& order);

    BidMap bids_;
    AskMap asks_;
    std::unordered_map<OrderId, Location> locations_;
};

}  // namespace engine
