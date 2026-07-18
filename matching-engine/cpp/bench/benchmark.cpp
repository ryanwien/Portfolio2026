// benchmark.cpp — per-operation latency and throughput for the order book.
//
// Two measurement problems shape how this is written.
//
// Timer resolution. A single submit or cancel costs well under a microsecond,
// but steady_clock on Windows ticks at ~100ns. Timing one operation therefore
// reports 0ns, 100ns or 200ns and nothing in between, which looks precise and
// is not. Operations are timed in batches instead, so each sample is an
// average over enough work to clear the clock's granularity by a wide margin.
//
// Book depth. Order flow that cancels as often as it adds drains the book, and
// an almost-empty book makes every operation look fast for the wrong reason.
// Cancels are gated on a depth floor so the measured book stays realistically
// populated throughout.

#include "../src/order_book.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace engine;
using Clock = std::chrono::steady_clock;

namespace {

/// Operations per timed batch. At ~100ns per op this is ~25us of work per
/// sample, roughly 250x the clock granularity.
constexpr int kBatch = 256;

/// Keep at least this many orders resting, so the book under test has depth.
constexpr std::size_t kDepthFloor = 5000;

struct Stats {
    double p50{}, p99{}, p999{}, mean{}, min{};
    std::size_t batches{};
};

Stats summarize(std::vector<double>& per_op_ns) {
    Stats s;
    if (per_op_ns.empty()) return s;
    std::sort(per_op_ns.begin(), per_op_ns.end());
    const auto at = [&](double q) {
        return per_op_ns[static_cast<std::size_t>(q * static_cast<double>(per_op_ns.size() - 1))];
    };
    s.batches = per_op_ns.size();
    s.min = per_op_ns.front();
    s.p50 = at(0.50);
    s.p99 = at(0.99);
    s.p999 = at(0.999);
    double total = 0;
    for (const double v : per_op_ns) total += v;
    s.mean = total / static_cast<double>(per_op_ns.size());
    return s;
}

void report(const std::string& label, Stats s) {
    std::cout << "  " << std::left << std::setw(20) << label << std::right
              << std::fixed << std::setprecision(1)
              << std::setw(9) << s.min
              << std::setw(9) << s.mean
              << std::setw(9) << s.p50
              << std::setw(9) << s.p99
              << std::setw(10) << s.p999
              << std::setw(11) << s.batches << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    const int batches = argc > 1 ? std::stoi(argv[1]) : 2000;

    // Fixed seed: this compares builds, it does not sample new flow.
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<int> tick(-40, 40);
    std::uniform_int_distribution<Quantity> size(1, 100);
    std::uniform_int_distribution<int> aggressive(0, 99);

    constexpr Price mid = 100.0;
    OrderBook book;
    std::vector<OrderId> resting;
    OrderId next_id = 1;
    std::uint64_t trades_generated = 0;

    auto submit_passive = [&] {
        const bool buy = (next_id % 2) == 0;
        const Price price = mid + 0.01 * tick(rng);
        Order o{next_id++, buy ? Side::Buy : Side::Sell, OrderType::Limit, price, size(rng)};
        const Quantity wanted = o.quantity;
        Quantity filled = 0;
        for (const auto& t : book.submit(o)) filled += t.quantity;
        trades_generated += (filled > 0 ? 1u : 0u);
        // A limit that didn't fully fill is now resting and can be cancelled.
        if (filled < wanted) resting.push_back(o.id);
    };

    // Warm the book up to the depth floor before measuring anything.
    while (resting.size() < kDepthFloor * 2) submit_passive();

    std::vector<double> submit_ns, cancel_ns, quote_ns;
    submit_ns.reserve(static_cast<std::size_t>(batches));
    cancel_ns.reserve(static_cast<std::size_t>(batches));
    quote_ns.reserve(static_cast<std::size_t>(batches));

    for (int b = 0; b < batches; ++b) {
        // ---- submits (mostly passive, ~5% crossing market orders) ----
        {
            const auto start = Clock::now();
            for (int i = 0; i < kBatch; ++i) {
                if (aggressive(rng) < 5) {
                    Order o{next_id++, (i % 2) ? Side::Buy : Side::Sell,
                            OrderType::Market, std::nullopt, size(rng)};
                    for (const auto& t : book.submit(o)) { (void)t; ++trades_generated; }
                } else {
                    submit_passive();
                }
            }
            const auto end = Clock::now();
            submit_ns.push_back(
                std::chrono::duration<double, std::nano>(end - start).count() / kBatch);
        }

        // ---- cancels, only while the book keeps its depth ----
        if (resting.size() > kDepthFloor + kBatch) {
            std::vector<OrderId> victims;
            victims.reserve(kBatch);
            for (int i = 0; i < kBatch; ++i) {
                std::uniform_int_distribution<std::size_t> pick(0, resting.size() - 1);
                const auto idx = pick(rng);
                victims.push_back(resting[idx]);
                resting[idx] = resting.back();
                resting.pop_back();
            }

            const auto start = Clock::now();
            for (const OrderId id : victims) book.cancel(id);
            const auto end = Clock::now();
            cancel_ns.push_back(
                std::chrono::duration<double, std::nano>(end - start).count() / kBatch);
        }

        // ---- top-of-book reads, which a strategy does constantly ----
        {
            Price sink = 0;
            const auto start = Clock::now();
            for (int i = 0; i < kBatch; ++i) {
                const auto bid = book.best_bid();
                const auto ask = book.best_ask();
                sink += (bid ? *bid : 0) + (ask ? *ask : 0);
            }
            const auto end = Clock::now();
            quote_ns.push_back(
                std::chrono::duration<double, std::nano>(end - start).count() / kBatch);
            if (sink < 0) std::cout << "";  // keep the reads from being elided
        }
    }

    const auto submit_stats = summarize(submit_ns);

    std::cout << "order book latency  (nanoseconds per operation, averaged over "
              << kBatch << "-op batches)\n"
              << std::string(86, '-') << "\n"
              << "  " << std::left << std::setw(20) << "operation" << std::right
              << std::setw(9) << "min" << std::setw(9) << "mean" << std::setw(9) << "p50"
              << std::setw(9) << "p99" << std::setw(10) << "p99.9"
              << std::setw(11) << "batches" << "\n";

    report("submit", submit_stats);
    report("cancel", summarize(cancel_ns));
    report("best bid + ask", summarize(quote_ns));

    std::cout << std::string(86, '-') << "\n"
              << "  resting orders at end : " << book.resting_count() << "\n"
              << "  price levels          : " << book.depth(1'000'000).bids.size() +
                                                  book.depth(1'000'000).asks.size() << "\n"
              << "  trades generated      : " << trades_generated << "\n";

    if (submit_stats.mean > 0) {
        std::cout << "  submit throughput     : "
                  << std::fixed << std::setprecision(2)
                  << (1e9 / submit_stats.mean) / 1e6 << " M ops/sec\n";
    }

    return 0;
}
