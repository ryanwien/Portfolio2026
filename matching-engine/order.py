"""Domain models for the matching engine.

An ``Order`` doubles as a node in the intrusive doubly-linked list that
backs each price level. Storing the ``prev``/``next`` pointers on the order
itself (rather than in a separate node object) is what lets cancellation run
in O(1): the order-id lookup map hands us the order, and the order already
knows its neighbours, so we can unlink it without scanning the queue.
"""

from __future__ import annotations

import itertools
from dataclasses import dataclass, field
from enum import Enum
from typing import Optional


class Side(Enum):
    BUY = "BUY"
    SELL = "SELL"


class OrderType(Enum):
    LIMIT = "LIMIT"
    MARKET = "MARKET"


_order_ids = itertools.count(1)


@dataclass
class Order:
    side: Side
    quantity: int
    price: Optional[float] = None          # None is only valid for MARKET orders
    order_type: OrderType = OrderType.LIMIT
    order_id: int = field(default_factory=lambda: next(_order_ids))

    # Intrusive linked-list pointers, set while the order rests on the book.
    # Excluded from equality/repr so two logically-equal orders still compare.
    prev: Optional["Order"] = field(default=None, repr=False, compare=False)
    next: Optional["Order"] = field(default=None, repr=False, compare=False)

    def __post_init__(self) -> None:
        if self.quantity <= 0:
            raise ValueError("quantity must be positive")
        if self.order_type is OrderType.LIMIT and self.price is None:
            raise ValueError("limit orders require a price")
        if self.order_type is OrderType.MARKET and self.price is not None:
            raise ValueError("market orders must not carry a price")


@dataclass(frozen=True)
class Trade:
    """A single execution. Price is the resting (maker) order's price, so any
    price improvement accrues to the incoming (taker) order — standard
    exchange convention."""

    maker_order_id: int
    taker_order_id: int
    price: float
    quantity: int
