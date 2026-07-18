#include "order_book.hpp"

#include <algorithm>

namespace engine {

std::vector<Trade> OrderBook::submit(Order order) {
    order.validate();

    auto trades = order.side == Side::Buy ? match(order, asks_, /*taker_is_buy=*/true)
                                          : match(order, bids_, /*taker_is_buy=*/false);

    // Whatever the aggressor couldn't fill rests, becoming the new quote.
    // A market order never rests — it takes what's there or expires.
    if (order.quantity > 0 && order.type == OrderType::Limit) rest(order);

    return trades;
}

template <typename BookSide>
std::vector<Trade> OrderBook::match(Order& taker, BookSide& opposing, bool taker_is_buy) {
    std::vector<Trade> trades;

    while (taker.quantity > 0 && !opposing.empty()) {
        // Both maps are ordered best-first, so begin() is the touchable level.
        auto best = opposing.begin();
        const Price best_price = best->first;
        PriceLevel& level = best->second;

        if (taker.type == OrderType::Limit) {
            // Stop once the best resting price no longer crosses the limit.
            if (taker_is_buy && best_price > *taker.price) break;
            if (!taker_is_buy && best_price < *taker.price) break;
        }

        // Walk the queue from the head: oldest resting order fills first.
        while (!level.queue.empty() && taker.quantity > 0) {
            Order& maker = level.queue.front();
            const Quantity fill = std::min(taker.quantity, maker.quantity);

            trades.push_back(Trade{maker.id, taker.id, best_price, fill});
            taker.quantity -= fill;
            maker.quantity -= fill;
            level.total_quantity -= fill;

            if (maker.quantity == 0) {
                locations_.erase(maker.id);
                level.queue.pop_front();
            }
            // Otherwise the maker is only partly filled, which means the taker
            // is exhausted, so the loop condition ends it.
        }

        if (level.queue.empty()) opposing.erase(best);
    }

    return trades;
}

void OrderBook::rest(const Order& order) {
    const Price price = *order.price;

    PriceLevel* level = nullptr;
    if (order.side == Side::Buy) {
        auto [it, inserted] = bids_.try_emplace(price);
        if (inserted) it->second.price = price;
        level = &it->second;
    } else {
        auto [it, inserted] = asks_.try_emplace(price);
        if (inserted) it->second.price = price;
        level = &it->second;
    }

    level->queue.push_back(order);
    level->total_quantity += order.quantity;

    locations_[order.id] = Location{order.side, price, level, std::prev(level->queue.end())};
}

bool OrderBook::cancel(OrderId id) {
    const auto found = locations_.find(id);
    if (found == locations_.end()) return false;

    const Location location = found->second;
    PriceLevel* level = location.level;

    level->total_quantity -= location.it->quantity;
    level->queue.erase(location.it);
    locations_.erase(found);

    // Only pay the ordered-map erase when the level is actually gone.
    if (level->queue.empty()) {
        if (location.side == Side::Buy) bids_.erase(location.price);
        else asks_.erase(location.price);
    }

    return true;
}

std::optional<Price> OrderBook::best_bid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<Price> OrderBook::best_ask() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

std::optional<Price> OrderBook::spread() const {
    const auto bid = best_bid();
    const auto ask = best_ask();
    if (!bid || !ask) return std::nullopt;
    return *ask - *bid;
}

Depth OrderBook::depth(std::size_t levels) const {
    Depth out;

    for (const auto& [price, level] : bids_) {
        if (out.bids.size() >= levels) break;
        out.bids.push_back({price, level.total_quantity});
    }
    for (const auto& [price, level] : asks_) {
        if (out.asks.size() >= levels) break;
        out.asks.push_back({price, level.total_quantity});
    }

    return out;
}

}  // namespace engine
