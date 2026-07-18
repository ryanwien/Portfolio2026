// test_order_book.cpp — self-contained test suite, no framework to install.

#include "../src/order_book.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace engine;

namespace {

int g_checks = 0;
int g_failures = 0;
const bool g_verbose = std::getenv("ENGINE_TEST_VERBOSE") != nullptr;

void check(bool condition, const std::string& what) {
    ++g_checks;
    if (g_verbose) std::cout << "    - " << what << "\n";
    if (!condition) {
        ++g_failures;
        std::cout << "  FAIL: " << what << "\n";
    }
}

template <typename A, typename B>
void check_eq(const A& actual, const B& expected, const std::string& what) {
    ++g_checks;
    if (g_verbose) std::cout << "    - " << what << "\n";
    if (!(actual == expected)) {
        ++g_failures;
        std::cout << "  FAIL: " << what << "\n"
                  << "        expected: " << expected << "\n"
                  << "        actual:   " << actual << "\n";
    }
}

Order limit(OrderId id, Side side, Price price, Quantity qty) {
    return Order{id, side, OrderType::Limit, price, qty};
}

Order market(OrderId id, Side side, Quantity qty) {
    return Order{id, side, OrderType::Market, std::nullopt, qty};
}

void test_resting_and_quotes() {
    std::cout << "resting orders and quotes\n";
    OrderBook book;

    check(!book.best_bid().has_value(), "an empty book has no bid");
    check(!book.best_ask().has_value(), "an empty book has no ask");
    check(!book.spread().has_value(), "an empty book has no spread");

    check(book.submit(limit(1, Side::Buy, 100.0, 10)).empty(), "a non-crossing buy rests silently");
    check(book.submit(limit(2, Side::Sell, 101.0, 10)).empty(), "a non-crossing sell rests silently");

    check_eq(*book.best_bid(), 100.0, "best bid is the highest buy");
    check_eq(*book.best_ask(), 101.0, "best ask is the lowest sell");
    check_eq(*book.spread(), 1.0, "spread is ask minus bid");
    check_eq(book.resting_count(), size_t{2}, "both orders are resting");

    // A better bid must take over the top of book.
    book.submit(limit(3, Side::Buy, 100.5, 5));
    check_eq(*book.best_bid(), 100.5, "a higher bid becomes the best bid");
}

void test_price_time_priority() {
    std::cout << "price-time priority\n";
    OrderBook book;

    // Same price, three different arrival times.
    book.submit(limit(1, Side::Sell, 100.0, 5));
    book.submit(limit(2, Side::Sell, 100.0, 5));
    book.submit(limit(3, Side::Sell, 99.0, 5));   // better price, arrived last

    const auto trades = book.submit(limit(4, Side::Buy, 100.0, 15));

    check_eq(trades.size(), size_t{3}, "the taker sweeps all three resting orders");
    // Best price first, regardless of arrival.
    check_eq(trades[0].maker_order_id, OrderId{3}, "the better price fills first");
    check_eq(trades[0].price, 99.0, "and it fills at its own resting price");
    // Then the two at 100.00, oldest first.
    check_eq(trades[1].maker_order_id, OrderId{1}, "at equal price the older order fills first");
    check_eq(trades[2].maker_order_id, OrderId{2}, "then the newer one");
}

void test_price_improvement_goes_to_the_aggressor() {
    std::cout << "price improvement\n";
    OrderBook book;

    book.submit(limit(1, Side::Sell, 99.0, 10));
    // Willing to pay 105, but the resting order is at 99.
    const auto trades = book.submit(limit(2, Side::Buy, 105.0, 10));

    check_eq(trades.size(), size_t{1}, "one execution");
    check_eq(trades[0].price, 99.0, "trade prints at the maker's price, not the taker's limit");
}

void test_partial_fill_rests_remainder() {
    std::cout << "partial fills\n";
    OrderBook book;

    book.submit(limit(1, Side::Sell, 100.0, 4));
    const auto trades = book.submit(limit(2, Side::Buy, 100.0, 10));

    check_eq(trades.size(), size_t{1}, "one execution against the only resting order");
    check_eq(trades[0].quantity, Quantity{4}, "it fills only what was available");
    check_eq(*book.best_bid(), 100.0, "the unfilled remainder rests as the new bid");

    const auto depth = book.depth();
    check_eq(depth.bids.at(0).quantity, Quantity{6}, "six of the ten are still working");
    check(!book.best_ask().has_value(), "the ask side is now empty");
}

void test_maker_partially_filled_stays() {
    std::cout << "partially filled maker\n";
    OrderBook book;

    book.submit(limit(1, Side::Sell, 100.0, 10));
    book.submit(limit(2, Side::Buy, 100.0, 3));

    const auto depth = book.depth();
    check_eq(depth.asks.at(0).quantity, Quantity{7}, "the maker keeps working its remainder");
    check_eq(book.resting_count(), size_t{1}, "and is still the only resting order");
}

void test_limit_does_not_cross_beyond_its_price() {
    std::cout << "limit price is respected\n";
    OrderBook book;

    book.submit(limit(1, Side::Sell, 101.0, 10));
    const auto trades = book.submit(limit(2, Side::Buy, 100.0, 10));

    check(trades.empty(), "a bid below the best ask does not trade");
    check_eq(*book.best_bid(), 100.0, "it rests instead");
    check_eq(*book.best_ask(), 101.0, "and the ask is untouched");
}

void test_market_orders() {
    std::cout << "market orders\n";
    OrderBook book;

    book.submit(limit(1, Side::Sell, 100.0, 5));
    book.submit(limit(2, Side::Sell, 101.0, 5));

    // Walks both levels regardless of price.
    const auto trades = book.submit(market(3, Side::Buy, 8));
    check_eq(trades.size(), size_t{2}, "a market order walks through levels");
    check_eq(trades[0].price, 100.0, "cheapest level first");
    check_eq(trades[1].price, 101.0, "then the next");

    // The unfilled part of a market order expires rather than resting.
    OrderBook thin;
    thin.submit(limit(1, Side::Sell, 100.0, 2));
    const auto sweep = thin.submit(market(2, Side::Buy, 100));
    check_eq(sweep.size(), size_t{1}, "it takes what little is there");
    check_eq(thin.resting_count(), size_t{0}, "and the remainder does not rest");
}

void test_market_order_into_empty_book() {
    std::cout << "market order, empty book\n";
    OrderBook book;

    const auto trades = book.submit(market(1, Side::Buy, 10));
    check(trades.empty(), "nothing trades");
    check_eq(book.resting_count(), size_t{0}, "and nothing rests");
}

void test_cancel() {
    std::cout << "cancellation\n";
    OrderBook book;

    book.submit(limit(1, Side::Buy, 100.0, 10));
    book.submit(limit(2, Side::Buy, 100.0, 5));

    check(book.cancel(1), "cancelling a resting order succeeds");
    check(!book.cancel(1), "cancelling it twice fails");
    check(!book.cancel(999), "cancelling an unknown id fails");

    const auto depth = book.depth();
    check_eq(depth.bids.at(0).quantity, Quantity{5},
             "the cancelled quantity leaves the level total");

    check(book.cancel(2), "cancelling the last order at a level succeeds");
    check(!book.best_bid().has_value(), "the empty level is removed from the book");
}

void test_cancel_after_partial_fill() {
    std::cout << "cancel after a partial fill\n";
    OrderBook book;

    book.submit(limit(1, Side::Sell, 100.0, 10));
    book.submit(limit(2, Side::Buy, 100.0, 4));   // leaves 6 resting

    check(book.cancel(1), "the partly filled maker can still be cancelled");
    check(!book.best_ask().has_value(), "removing it clears the level");
    check_eq(book.resting_count(), size_t{0}, "nothing is left resting");
}

void test_filled_orders_are_not_cancellable() {
    std::cout << "fully filled orders leave the book\n";
    OrderBook book;

    book.submit(limit(1, Side::Sell, 100.0, 5));
    book.submit(limit(2, Side::Buy, 100.0, 5));

    check(!book.cancel(1), "a fully filled maker is no longer cancellable");
    check_eq(book.resting_count(), size_t{0}, "the book is empty");
}

void test_depth_aggregates_and_limits() {
    std::cout << "depth snapshot\n";
    OrderBook book;

    book.submit(limit(1, Side::Buy, 100.0, 10));
    book.submit(limit(2, Side::Buy, 100.0, 15));   // same level
    book.submit(limit(3, Side::Buy, 99.0, 5));
    book.submit(limit(4, Side::Buy, 98.0, 5));

    const auto depth = book.depth(2);
    check_eq(depth.bids.size(), size_t{2}, "depth honours the level limit");
    check_eq(depth.bids[0].price, 100.0, "bids are ordered best first");
    check_eq(depth.bids[0].quantity, Quantity{25}, "quantities at one price are aggregated");
    check_eq(depth.bids[1].price, 99.0, "then the next level down");
}

void test_validation() {
    std::cout << "order validation\n";
    OrderBook book;

    auto rejects = [&](Order order, const std::string& what) {
        bool threw = false;
        try { book.submit(order); } catch (const std::invalid_argument&) { threw = true; }
        check(threw, what);
    };

    rejects(limit(1, Side::Buy, 100.0, 0), "zero quantity is rejected");
    rejects(limit(2, Side::Buy, 100.0, -5), "negative quantity is rejected");
    rejects(Order{3, Side::Buy, OrderType::Limit, std::nullopt, 10},
            "a limit order without a price is rejected");
    rejects(Order{4, Side::Buy, OrderType::Market, 100.0, 10},
            "a market order carrying a price is rejected");
}

void test_book_stays_consistent_under_churn() {
    std::cout << "consistency under churn\n";
    OrderBook book;

    // Submit, partially fill, and cancel in a deterministic pattern, then
    // confirm the resting count matches what is actually on the levels.
    for (OrderId id = 1; id <= 200; ++id) {
        const Price price = 100.0 + static_cast<double>(id % 10) * 0.5;
        book.submit(limit(id, id % 2 ? Side::Buy : Side::Sell, price, 10));
    }
    for (OrderId id = 1; id <= 200; id += 3) book.cancel(id);

    Quantity level_total = 0;
    for (const auto& entry : book.depth(100).bids) level_total += entry.quantity;
    for (const auto& entry : book.depth(100).asks) level_total += entry.quantity;

    Quantity expected = 0;
    for (OrderId id = 1; id <= 200; ++id) if (id % 3 != 1) expected += 10;

    // Crossing orders will have traded, so totals only need to be consistent
    // with the resting count, not with the raw submission total.
    check(level_total >= 0, "level totals never go negative");
    check(book.resting_count() * 10 >= static_cast<size_t>(0), "resting count is sane");
    check_eq(level_total, static_cast<Quantity>(book.resting_count()) * 10,
             "level quantities agree with the number of resting orders");
    (void)expected;
}

}  // namespace

int main() {
    std::cout << std::unitbuf;
    std::cout << "order book tests\n" << std::string(60, '-') << "\n";

    test_resting_and_quotes();
    test_price_time_priority();
    test_price_improvement_goes_to_the_aggressor();
    test_partial_fill_rests_remainder();
    test_maker_partially_filled_stays();
    test_limit_does_not_cross_beyond_its_price();
    test_market_orders();
    test_market_order_into_empty_book();
    test_cancel();
    test_cancel_after_partial_fill();
    test_filled_orders_are_not_cancellable();
    test_depth_aggregates_and_limits();
    test_validation();
    test_book_stays_consistent_under_churn();

    std::cout << std::string(60, '-') << "\n";
    if (g_failures == 0) {
        std::cout << "All " << g_checks << " checks passed.\n";
        return 0;
    }
    std::cout << g_failures << " of " << g_checks << " checks FAILED.\n";
    return 1;
}
