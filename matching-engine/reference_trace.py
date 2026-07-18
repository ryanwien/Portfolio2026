"""reference_trace.py — replay reference_scenario.txt through the Python engine.

Prints every trade and the final book in a fixed format. The C++ engine has a
matching runner; diffing the two outputs is what proves the ports agree.

Usage:
    python reference_trace.py [scenario.txt]
"""

import os
import sys

from book import OrderBook
from order import Order, OrderType, Side


def parse_scenario(path):
    """Yield ('submit', side, type, price, qty) or ('cancel', order_id)."""
    with open(path, "r", encoding="utf-8") as fh:
        for raw in fh:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if parts[0] == "C":
                yield ("cancel", int(parts[1]))
            else:
                order_type = OrderType.LIMIT if parts[0] == "L" else OrderType.MARKET
                side = Side.BUY if parts[1] == "B" else Side.SELL
                price = None if parts[2] == "-" else float(parts[2])
                yield ("submit", side, order_type, price, int(parts[3]))


def main():
    scenario = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "reference_scenario.txt")

    book = OrderBook()
    next_id = 1

    for op in parse_scenario(scenario):
        if op[0] == "cancel":
            _, order_id = op
            removed = book.cancel(order_id)
            print(f"CANCEL {order_id} {'ok' if removed else 'miss'}")
            continue

        _, side, order_type, price, qty = op
        order = Order(side=side, quantity=qty, price=price,
                      order_type=order_type, order_id=next_id)
        next_id += 1

        price_text = "-" if price is None else f"{price:.2f}"
        print(f"SUBMIT {order.order_id} {side.value} {order_type.value} {price_text} {qty}")

        for trade in book.submit(order):
            print(f"  TRADE maker={trade.maker_order_id} taker={trade.taker_order_id} "
                  f"price={trade.price:.2f} qty={trade.quantity}")

    print("BOOK")
    depth = book.depth(levels=10)
    for price, quantity in depth["bids"]:
        print(f"  BID {price:.2f} {quantity}")
    for price, quantity in depth["asks"]:
        print(f"  ASK {price:.2f} {quantity}")

    bid, ask = book.best_bid(), book.best_ask()
    print(f"BEST_BID {'-' if bid is None else f'{bid:.2f}'}")
    print(f"BEST_ASK {'-' if ask is None else f'{ask:.2f}'}")
    spread = book.spread()
    print(f"SPREAD {'-' if spread is None else f'{spread:.2f}'}")
    print(f"RESTING {len(book.orders)}")


if __name__ == "__main__":
    main()
