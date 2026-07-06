"""Runnable walkthrough: python demo.py

Builds up a book, then sends a marketable order and prints the trades and
resulting depth so you can see price-time priority in action.
"""

from matching_engine import Order, OrderBook, OrderType, Side


def show(book: OrderBook) -> None:
    d = book.depth()
    print(f"  asks: {list(reversed(d['asks']))}")
    print(f"  bids: {d['bids']}")
    print(f"  spread: {book.spread()}\n")


def main() -> None:
    book = OrderBook()

    print("1) Rest some resting liquidity on both sides:")
    for side, qty, px in [
        (Side.SELL, 5, 101.0),
        (Side.SELL, 8, 102.0),
        (Side.SELL, 3, 101.0),   # joins the 101 queue behind the first order
        (Side.BUY, 4, 99.0),
        (Side.BUY, 6, 98.0),
    ]:
        book.submit(Order(side=side, quantity=qty, price=px))
    show(book)

    print("2) Aggressive BUY 10 @ 101 — should sweep both 101 orders (FIFO), rest 2:")
    trades = book.submit(Order(side=Side.BUY, quantity=10, price=101.0))
    for t in trades:
        print(f"   TRADE {t.quantity} @ {t.price} "
              f"(maker #{t.maker_order_id} / taker #{t.taker_order_id})")
    print()
    show(book)

    print("3) MARKET SELL 7 — hits best bids top-down:")
    trades = book.submit(Order(side=Side.SELL, quantity=7, order_type=OrderType.MARKET))
    for t in trades:
        print(f"   TRADE {t.quantity} @ {t.price} "
              f"(maker #{t.maker_order_id} / taker #{t.taker_order_id})")
    print()
    show(book)


if __name__ == "__main__":
    main()
