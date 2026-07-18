// reference_trace.cpp — replay reference_scenario.txt through the C++ engine.
//
// Prints trades and the final book in exactly the format reference_trace.py
// uses, so diffing the two outputs proves the ports agree.

#include "order_book.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;
using namespace engine;

namespace {

std::string fixed2(Price p) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << p;
    return out.str();
}

fs::path default_scenario() {
    // Walk up from the binary to find the scenario next to the Python sources.
    for (fs::path dir = fs::current_path(); !dir.empty(); dir = dir.parent_path()) {
        const auto candidate = dir / "reference_scenario.txt";
        if (fs::exists(candidate)) return candidate;
        if (!dir.has_relative_path()) break;
    }
    return "reference_scenario.txt";
}

}  // namespace

int main(int argc, char** argv) {
    const fs::path scenario = argc > 1 ? fs::path(argv[1]) : default_scenario();

    std::ifstream in(scenario);
    if (!in) {
        std::cerr << "Error: could not open scenario '" << scenario.string() << "'.\n";
        return 1;
    }

    OrderBook book;
    OrderId next_id = 1;
    std::string line;

    while (std::getline(in, line)) {
        // Strip comments and blanks.
        const auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        std::istringstream fields(line);
        std::string kind;
        if (!(fields >> kind)) continue;

        if (kind == "C") {
            OrderId id{};
            fields >> id;
            std::cout << "CANCEL " << id << (book.cancel(id) ? " ok" : " miss") << "\n";
            continue;
        }

        std::string side_text, price_text;
        Quantity qty{};
        fields >> side_text >> price_text >> qty;

        Order order;
        order.id = next_id++;
        order.side = side_text == "B" ? Side::Buy : Side::Sell;
        order.type = kind == "L" ? OrderType::Limit : OrderType::Market;
        order.quantity = qty;
        if (price_text != "-") order.price = std::stod(price_text);

        std::cout << "SUBMIT " << order.id << " "
                  << (order.side == Side::Buy ? "BUY" : "SELL") << " "
                  << (order.type == OrderType::Limit ? "LIMIT" : "MARKET") << " "
                  << (order.price ? fixed2(*order.price) : "-") << " " << qty << "\n";

        for (const auto& trade : book.submit(order)) {
            std::cout << "  TRADE maker=" << trade.maker_order_id
                      << " taker=" << trade.taker_order_id
                      << " price=" << fixed2(trade.price)
                      << " qty=" << trade.quantity << "\n";
        }
    }

    std::cout << "BOOK\n";
    const auto depth = book.depth(10);
    for (const auto& entry : depth.bids)
        std::cout << "  BID " << fixed2(entry.price) << " " << entry.quantity << "\n";
    for (const auto& entry : depth.asks)
        std::cout << "  ASK " << fixed2(entry.price) << " " << entry.quantity << "\n";

    const auto bid = book.best_bid();
    const auto ask = book.best_ask();
    const auto spread = book.spread();
    std::cout << "BEST_BID " << (bid ? fixed2(*bid) : "-") << "\n";
    std::cout << "BEST_ASK " << (ask ? fixed2(*ask) : "-") << "\n";
    std::cout << "SPREAD " << (spread ? fixed2(*spread) : "-") << "\n";
    std::cout << "RESTING " << book.resting_count() << "\n";

    return 0;
}
