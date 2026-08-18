"""export_sample.py — regenerate the sample log the browser demo analyses.

demo.html carries the event log inline so it can run with no backend and no
fetch. That copy was originally pasted in by hand, and it drifted: 160 of the
221 events had picked up different usernames than the committed log, so the
README's claim that the demo and the CLI run "over the exact same sample log"
had quietly stopped being true. The rules matched; the data did not.

This script regenerates the whole `var SAMPLE = [...]` line from
`data/auth_events.jsonl`, so there is one copy of the log and one place to
edit it.

Usage:
    python export_sample.py            # rewrite the `var SAMPLE = [...];` line
    python export_sample.py --check    # exit 1 if demo.html has drifted
"""

import io
import json
import os
import re
import sys

BASE = os.path.dirname(os.path.abspath(__file__))
LOG_PATH = os.path.join(BASE, "data", "auth_events.jsonl")
DEMO_PATH = os.path.join(BASE, "demo.html")

SAMPLE_RE = re.compile(r"^  var SAMPLE = \[.*\];$", re.M)

# Every field the demo's detectors read. Missing one fails here rather than
# silently producing a demo that finds nothing.
REQUIRED = ("ts", "src_ip", "country", "lat", "lon", "user", "service",
            "port", "action", "status")


def load_events(path):
    events = []
    for n, line in enumerate(io.open(path, encoding="utf-8"), 1):
        line = line.strip()
        if not line:
            continue
        try:
            event = json.loads(line)
        except ValueError as exc:
            raise SystemExit(f"{path}:{n}: not valid JSON: {exc}")
        missing = [f for f in REQUIRED if f not in event]
        if missing:
            raise SystemExit(f"{path}:{n}: missing {', '.join(missing)}")
        events.append(event)
    return events


def main():
    check = "--check" in sys.argv[1:]

    events = load_events(LOG_PATH)
    line = "  var SAMPLE = " + json.dumps(events, separators=(",", ":")) + ";"

    html = io.open(DEMO_PATH, encoding="utf-8").read()
    if not SAMPLE_RE.search(html):
        raise SystemExit("could not find the `  var SAMPLE = [...];` line in demo.html")

    updated = SAMPLE_RE.sub(lambda _: line, html, count=1)

    if check:
        if updated == html:
            print(f"in sync: {len(events)} events")
            return 0
        print("OUT OF SYNC: demo.html does not match data/auth_events.jsonl")
        print("run `python export_sample.py` to regenerate it")
        return 1

    if updated == html:
        print(f"Already up to date ({len(events)} events)")
        return 0

    io.open(DEMO_PATH, "w", encoding="utf-8", newline="").write(updated)
    print(f"Updated {DEMO_PATH}")
    print(f"  events    : {len(events)}")
    print(f"  span      : {events[0]['ts']} .. {events[-1]['ts']}")
    print(f"  users     : {len(set(e['user'] for e in events))}")
    print(f"  source IPs: {len(set(e['src_ip'] for e in events))}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
