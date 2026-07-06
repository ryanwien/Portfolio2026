"""Tests for the matching engine. Run with: pytest -q"""

import pytest

from matching_engine import Order, OrderBook, OrderType, Side


def limit(side, qty, price):
    return Order(side=side, quantity=qty, price=price)


def market(side, qty):
    return Order(side=side, quantity=qty, order_type=OrderType.MARKET)


def test_resting_order_no_cross():
    book = OrderBook()
    trades = book.submit(limit(Side.BUY, 10, 100.0))
    assert trades == []
    assert book.best_bid() == 100.0
    assert book.best_ask() is None


def test_full_fill():
    book = OrderBook()
    book.submit(limit(Side.SELL, 10, 100.0))
    trades = book.submit(limit(Side.BUY, 10, 100.0))
    assert len(trades) == 1
    assert (trades[0].price, trades[0].quantity) == (100.0, 10)
    assert book.best_bid() is None and book.best_ask() is None  # book cleared


def test_partial_fill_taker_larger_rests_remainder():
    book = OrderBook()
    book.submit(limit(Side.SELL, 4, 100.0))
    trades = book.submit(limit(Side.BUY, 10, 100.0))
    assert sum(t.quantity for t in trades) == 4
    assert book.best_bid() == 100.0        # 6 remaining rest as a bid
    assert book.depth()["bids"] == [(100.0, 6)]


def test_partial_fill_maker_larger_stays_on_book():
    book = OrderBook()
    book.submit(limit(Side.SELL, 10, 100.0))
    trades = book.submit(limit(Side.BUY, 4, 100.0))
    assert sum(t.quantity for t in trades) == 4
    assert book.best_ask() == 100.0
    assert book.depth()["asks"] == [(100.0, 6)]


def test_price_time_priority():
    """Same price: the earlier resting order must fill first (FIFO)."""
    book = OrderBook()
    first = limit(Side.SELL, 5, 100.0)
    second = limit(Side.SELL, 5, 100.0)
    book.submit(first)
    book.submit(second)
    trades = book.submit(limit(Side.BUY, 5, 100.0))
    assert trades[0].maker_order_id == first.order_id  # not `second`


def test_price_priority_sweeps_best_first():
    """A buy sweeps the cheapest asks first and executes at maker prices."""
    book = OrderBook()
    book.submit(limit(Side.SELL, 5, 101.0))
    book.submit(limit(Side.SELL, 5, 100.0))
    trades = book.submit(limit(Side.BUY, 8, 101.0))
    assert trades[0].price == 100.0 and trades[0].quantity == 5
    assert trades[1].price == 101.0 and trades[1].quantity == 3
    assert book.depth()["asks"] == [(101.0, 2)]


def test_price_improvement_goes_to_taker():
    """Buy limit @105 hits a resting ask @100 -> trade prints at 100."""
    book = OrderBook()
    book.submit(limit(Side.SELL, 5, 100.0))
    trades = book.submit(limit(Side.BUY, 5, 105.0))
    assert trades[0].price == 100.0


def test_cancel_removes_order_and_level():
    book = OrderBook()
    o = limit(Side.BUY, 10, 100.0)
    book.submit(o)
    assert book.cancel(o.order_id) is True
    assert book.best_bid() is None
    assert book.cancel(o.order_id) is False  # already gone


def test_cancel_preserves_other_orders_at_level():
    book = OrderBook()
    a = limit(Side.BUY, 5, 100.0)
    b = limit(Side.BUY, 7, 100.0)
    book.submit(a)
    book.submit(b)
    book.cancel(a.order_id)
    assert book.depth()["bids"] == [(100.0, 7)]
    # b should now be first in line
    trades = book.submit(limit(Side.SELL, 7, 100.0))
    assert trades[0].maker_order_id == b.order_id


def test_market_order_sweeps_until_filled():
    book = OrderBook()
    book.submit(limit(Side.SELL, 3, 100.0))
    book.submit(limit(Side.SELL, 3, 101.0))
    trades = book.submit(market(Side.BUY, 5))
    assert sum(t.quantity for t in trades) == 5
    assert book.depth()["asks"] == [(101.0, 1)]


def test_market_order_discards_unfillable_remainder():
    book = OrderBook()
    book.submit(limit(Side.SELL, 2, 100.0))
    trades = book.submit(market(Side.BUY, 5))
    assert sum(t.quantity for t in trades) == 2
    assert book.best_ask() is None  # nothing left, remainder not rested


def test_no_cross_when_prices_dont_meet():
    book = OrderBook()
    book.submit(limit(Side.SELL, 5, 101.0))
    trades = book.submit(limit(Side.BUY, 5, 100.0))
    assert trades == []
    assert book.best_bid() == 100.0 and book.best_ask() == 101.0
    assert book.spread() == 1.0


def test_invalid_orders_rejected():
    with pytest.raises(ValueError):
        Order(side=Side.BUY, quantity=0, price=100.0)
    with pytest.raises(ValueError):
        Order(side=Side.BUY, quantity=5)  # limit with no price
