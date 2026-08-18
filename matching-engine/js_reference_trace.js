/**
 * js_reference_trace.js — replay reference_scenario.txt through the engine
 * that ships inside orderbook_terminal.html.
 *
 * reference_trace.py and cpp/reference_trace.exe already prove the Python and
 * C++ engines agree. The terminal calls itself a client-side port of the same
 * design, which was an untested claim until this file existed: it pulls the
 * engine out of the page as shipped — not a copy of it — and prints the same
 * format, so the three implementations diff against each other.
 *
 * Usage:
 *     node js_reference_trace.js [scenario.txt]
 *
 *     python reference_trace.py > py.txt
 *     node js_reference_trace.js > js.txt
 *     diff py.txt js.txt        # no output
 */

const fs = require("fs");
const path = require("path");

const HERE = __dirname;
const PAGE = path.join(HERE, "orderbook_terminal.html");
const scenarioPath = process.argv[2] || path.join(HERE, "reference_scenario.txt");

/** Lift `class Levels` through the end of `class Book` out of the page. */
function loadEngine(pagePath) {
  const lines = fs.readFileSync(pagePath, "utf8").split(/\r?\n/);
  const start = lines.findIndex((l) => l.trim().startsWith("class Levels{"));
  const end = lines.findIndex(
    (l, i) => i > start && l.trim() === "}" && lines[i - 1].includes("bestAsk()"));
  if (start === -1 || end === -1) {
    throw new Error(`could not find the engine classes in ${pagePath}`);
  }
  const source = lines.slice(start, end + 1).join("\n");
  return new Function(`${source}\nreturn { Book };`)().Book;
}

/** Yield ["cancel", id] or ["submit", side, type, price, qty]. */
function* parseScenario(file) {
  for (const raw of fs.readFileSync(file, "utf8").split(/\r?\n/)) {
    const line = raw.trim();
    if (!line || line.startsWith("#")) continue;
    const parts = line.split(/\s+/);
    if (parts[0] === "C") {
      yield ["cancel", Number(parts[1])];
    } else {
      yield ["submit",
             parts[1] === "B" ? "buy" : "sell",
             parts[0] === "L" ? "limit" : "market",
             parts[2] === "-" ? null : Number(parts[2]),
             Number(parts[3])];
    }
  }
}

function main() {
  const Book = loadEngine(PAGE);
  const book = new Book();
  const out = [];
  const f2 = (n) => n.toFixed(2);
  let nextId = 1;

  for (const op of parseScenario(scenarioPath)) {
    if (op[0] === "cancel") {
      const id = op[1];
      out.push(`CANCEL ${id} ${book.cancel(id) ? "ok" : "miss"}`);
      continue;
    }

    const [, side, type, price, qty] = op;
    const id = nextId++;
    out.push(`SUBMIT ${id} ${side.toUpperCase()} ${type.toUpperCase()} ` +
             `${price === null ? "-" : f2(price)} ${qty}`);

    for (const t of book.submit({ id, side, type, price, qty })) {
      out.push(`  TRADE maker=${t.makerId} taker=${t.takerId} ` +
               `price=${f2(t.price)} qty=${t.qty}`);
    }
  }

  out.push("BOOK");
  for (const l of book.bids.topN(10)) out.push(`  BID ${f2(l.price)} ${l.totalQty}`);
  for (const l of book.asks.topN(10)) out.push(`  ASK ${f2(l.price)} ${l.totalQty}`);

  const bid = book.bestBid(), ask = book.bestAsk();
  out.push(`BEST_BID ${bid === null ? "-" : f2(bid)}`);
  out.push(`BEST_ASK ${ask === null ? "-" : f2(ask)}`);
  out.push(`SPREAD ${bid === null || ask === null ? "-" : f2(ask - bid)}`);
  out.push(`RESTING ${book.byId.size}`);

  console.log(out.join("\n"));
}

main();
