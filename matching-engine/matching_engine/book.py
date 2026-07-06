"""A price-time-priority limit order book.

Data-structure design
---------------------
Two ordered maps (``SortedDict``) hold the price levels:

    bids : price -> PriceLevel   (best bid  = highest price)
    asks : price -> PriceLevel   (best ask  = lowest  price)

Each ``PriceLevel`` is a FIFO doubly-linked list of resting orders, giving
**time priority** for free (append at tail, match from head). A separate
``orders`` dict maps order_id -> Order for O(1) cancellation.

Why not two heaps? A binary heap gives O(1) access to the best price but
makes cancelling an arbitrary resting order O(n), and it can't express
time priority within a price level without extra bookkeeping. Cancels
dominate real order flow (most orders are cancelled, not filled), so the
ordered-map-of-queues design is the one exchanges actually use.

Complexity (P = distinct price levels)
    submit (per level touched) : O(log P)
    best bid / ask             : O(1)   amortised via SortedDict.peekitem
    cancel                     : O(1)
"""

from __future__ import annotations

from typing import List, Optional

from sortedcontainers import SortedDict

from .order import Order, OrderType, Side, Trade


class PriceLevel:
    """FIFO queue of orders resting at one price, as an intrusive DLL."""

    __slots__ = ("price", "head", "tail", "total_quantity")

    def __init__(self, price: float) -> None:
        self.price = price
        self.head: Optional[Order] = None
        self.tail: Optional[Order] = None
        self.total_quantity = 0

    def append(self, order: Order) -> None:
        order.prev, order.next = self.tail, None
        if self.tail is not None:
            self.tail.next = order
        else:
            self.head = order
        self.tail = order
        self.total_quantity += order.quantity

    def remove(self, order: Order) -> None:
        if order.prev is not None:
            order.prev.next = order.next
        else:
            self.head = order.next
        if order.next is not None:
            order.next.prev = order.prev
        else:
            self.tail = order.prev
        self.total_quantity -= order.quantity
        order.prev = order.next = None

    def is_empty(self) -> bool:
        return self.head is None


class OrderBook:
    def __init__(self) -> None:
        self.bids: SortedDict = SortedDict()   # ascending keys; best bid at index -1
        self.asks: SortedDict = SortedDict()   # ascending keys; best ask at index  0
        self.orders: dict[int, Order] = {}     # order_id -> resting Order

    # ---- public API ---------------------------------------------------

    def submit(self, order: Order) -> List[Trade]:
        """Match an incoming order against the book; rest any unfilled
        remainder (limit orders only). Returns the trades generated."""
        if order.side is Side.BUY:
            trades = self._match(order, self.asks, taker_is_buy=True)
        else:
            trades = self._match(order, self.bids, taker_is_buy=False)

        if order.quantity > 0 and order.order_type is OrderType.LIMIT:
            self._rest(order)
        return trades

    def cancel(self, order_id: int) -> bool:
        """Remove a resting order. Returns False if it isn't on the book."""
        order = self.orders.get(order_id)
        if order is None:
            return False
        book = self.bids if order.side is Side.BUY else self.asks
        level: PriceLevel = book[order.price]
        level.remove(order)
        if level.is_empty():
            del book[order.price]
        del self.orders[order_id]
        return True

    def best_bid(self) -> Optional[float]:
        return self.bids.peekitem(-1)[0] if self.bids else None

    def best_ask(self) -> Optional[float]:
        return self.asks.peekitem(0)[0] if self.asks else None

    def spread(self) -> Optional[float]:
        bid, ask = self.best_bid(), self.best_ask()
        return None if bid is None or ask is None else ask - bid

    def depth(self, levels: int = 5) -> dict:
        """Aggregated top-of-book snapshot for display/inspection."""
        bids = [self.bids.peekitem(-1 - i) for i in range(min(levels, len(self.bids)))]
        asks = [self.asks.peekitem(i) for i in range(min(levels, len(self.asks)))]
        return {
            "bids": [(p, lvl.total_quantity) for p, lvl in bids],
            "asks": [(p, lvl.total_quantity) for p, lvl in asks],
        }

    # ---- internals ----------------------------------------------------

    def _match(self, taker: Order, book: SortedDict, taker_is_buy: bool) -> List[Trade]:
        trades: List[Trade] = []
        while taker.quantity > 0 and book:
            best_price, level = book.peekitem(0) if taker_is_buy else book.peekitem(-1)

            if taker.order_type is OrderType.LIMIT:
                # Stop once the best resting price no longer crosses the limit.
                if taker_is_buy and best_price > taker.price:
                    break
                if not taker_is_buy and best_price < taker.price:
                    break

            maker = level.head
            while maker is not None and taker.quantity > 0:
                fill = min(taker.quantity, maker.quantity)
                trades.append(Trade(maker.order_id, taker.order_id, best_price, fill))
                taker.quantity -= fill
                maker.quantity -= fill
                level.total_quantity -= fill

                if maker.quantity == 0:              # maker fully filled -> remove
                    nxt = maker.next
                    level.remove(maker)              # remove() subtracts 0 now
                    del self.orders[maker.order_id]
                    maker = nxt
                # else: maker partially filled, meaning taker is exhausted -> loop ends

            if level.is_empty():
                del book[best_price]
        return trades

    def _rest(self, order: Order) -> None:
        book = self.bids if order.side is Side.BUY else self.asks
        level = book.get(order.price)
        if level is None:
            level = PriceLevel(order.price)
            book[order.price] = level
        level.append(order)
        self.orders[order.order_id] = order
