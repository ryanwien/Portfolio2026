# Limit Order Book & Matching Engine

A price-time-priority matching engine is the core of any exchange. Incoming
orders match against the opposite side of the book best-price-first, and ties
at a price are broken by arrival time (FIFO). Written to be read: the design
choices below are the point.

## Quick start

```bash
pip install -r requirements.txt
python demo.py         # annotated walkthrough
```

## What it does

- **Limit & market orders** on both sides
- **Matching** with price priority across levels and time priority within a level
- **Partial fills** — remainder of a limit order rests on the book; an unfilled
  market remainder is discarded
- **Price improvement** accrues to the incoming order (trades print at the
  resting order's price — standard convention)
- **O(1) cancellation** of any resting order
- **Top-of-book / depth** snapshots

## Design: why not two heaps?

The tempting answer to "best bid / best ask" is a max-heap for bids and a
min-heap for asks. It's the wrong tool, and explaining why is the interesting
part:

| Operation            | Two heaps      | This design            |
|----------------------|----------------|------------------------|
| Best bid / ask       | O(1)           | O(1)                   |
| Insert resting order | O(log N)       | O(log P)               |
| **Cancel an order**  | **O(N)**       | **O(1)**               |
| Time priority        | manual seq nos | free (FIFO queue)      |

`N` = total orders, `P` = distinct price levels (`P ≪ N` in practice). Real
order flow is cancel-heavy — most orders never trade — so O(n) cancels are
disqualifying. The structure exchanges actually use:

```
bids: SortedDict  price -> PriceLevel   (best bid = highest price)
asks: SortedDict  price -> PriceLevel   (best ask = lowest  price)
                              │
                              └── FIFO doubly-linked list of orders (time priority)

orders: dict  order_id -> Order          (O(1) lookup for cancel)
```

Three ideas working together:

1. **Ordered map of price levels** (`sortedcontainers.SortedDict`, a B-tree)
   → O(log P) to find/create a level, O(1) to read the best price.
2. **FIFO doubly-linked list per level** → time priority is automatic; append
   at the tail, match from the head.
3. **Intrusive linked list + id→order map** → the `prev`/`next` pointers live
   on the `Order` itself, so a cancel looks the order up in the dict and
   unlinks it in O(1) with no queue scan.

## Complexity summary

| Operation      | Cost      |
|----------------|-----------|
| submit (per level touched) | O(log P) |
| best bid / ask | O(1)      |
| cancel         | O(1)      |
| depth(k)       | O(k)      |

## Files

- `order.py` — `Order` (also a list node), `Trade`, enums
- `book.py` — `PriceLevel` and the `OrderBook` engine
- `demo.py` — annotated end-to-end scenario
- `orderbook_terminal.html` — live browser front end (see below)

## Front end: live order book terminal

`orderbook_terminal.html` is a single self-contained file — no build, no
backend. Open it (or deploy it as `index.html` on GitHub Pages) and it boots
into a running market:

- **Depth ladder** with size bars and a live spread band
- **Order entry** (buy/sell, limit/market); click any level to load its price
- **Trade tape** of executions, timestamped to the millisecond
- **Order-flow simulator** generating limits, markets, and cancels around the mid
- **Symbol basket** (AAPL, MSFT, NVDA, AMZN, GOOGL, META, TSLA, JPM) with
  realistic seed prices — illustrative starting points, not a live feed

The matching logic is the same price-time-priority design as the Python
engine, ported to JavaScript, so one design is shown in two languages.

## Natural extensions (good talking points)

- **More order types**: IOC (immediate-or-cancel), FOK (fill-or-kill),
  stop / stop-limit, post-only.
- **Self-trade prevention**: cancel or decrement when a participant would
  cross their own resting order.
- **Thread safety / throughput**: a single-writer event loop fed by a lock-free
  queue is the usual answer — one thread owns the book, so no per-order locking.
- **Market-data feed**: emit L2 depth deltas and a trade tape to subscribers.
- **FIX protocol** front end for order entry.
- **Persistence / replay**: append-only event log so the book can be rebuilt
  deterministically after a crash.
- **C++ port**: swap `SortedDict` for `std::map<Price, PriceLevel>` and the
  id map for `std::unordered_map` — same design, and more idiomatic for the HFT
  domain this models.
```
