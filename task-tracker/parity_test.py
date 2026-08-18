"""Hold the two backends to the same answers.

The README claims both backends serve the same REST contract and the homepage
claims they share validation. This checks it instead of asserting it, because
that claim was wrong once: Flask used to accept a whitespace-only title, accept
a numeric title (with an AttributeError and a 500 on the way out), reject an
explicit null priority the C# side defaulted, and let a PUT blank out a title
entirely. Eight cases diverged.

Start both, then run this:

    python backend/app.py                                     # :5000
    ASPNETCORE_URLS=http://localhost:5058       dotnet run --project csharp/src/TaskTracker.Api         # :5058
    python parity_test.py http://localhost:5000 http://localhost:5058

Both default to port 5000, so the C# side needs an explicit port to sit
alongside the Flask one.

Status codes are always compared. Error messages are compared too whenever both
sides produced one of this API's own `{"error": ...}` bodies — a C# model-binding
failure answers with RFC 7807 problem details instead, and those are not meant to
match. Comparing the message matters because a body can be wrong in more than one
way at once: the two servers used to validate PUT in different orders, so
`{"title": "", "priority": "urgent"}` was a 400 on both while Flask blamed the
title and C# blamed the priority.
"""

import json
import sys
import urllib.error
import urllib.request

# The front end caps the title field at 120 characters; the server has to cap it
# at the same place, so the boundary is checked from both sides.
MAX = 120
LONG = "a" * (MAX + 1)
EXACT = "a" * MAX

# (label, body) — POST cases run against the collection, PUT cases against a
# task seeded for each backend.
POST_CASES = [
    ("valid task",              {"title": "real task"}),
    ("title missing",           {}),
    ("title empty string",      {"title": ""}),
    ("title whitespace only",   {"title": "   "}),
    ("title null",              {"title": None}),
    ("title is a number",       {"title": 123}),
    ("title is an object",      {"title": {"a": 1}}),
    ("title exactly 120",       {"title": EXACT}),
    ("title 121",               {"title": LONG}),
    ("title 120 plus padding",  {"title": "  " + EXACT + "  "}),
    ("title 121 and bad prio",  {"title": LONG, "priority": "urgent"}),
    ("priority invalid",        {"title": "x", "priority": "urgent"}),
    ("priority uppercase",      {"title": "x", "priority": "HIGH"}),
    ("priority explicit null",  {"title": "x", "priority": None}),
    ("description is a number", {"title": "x", "description": 7}),
    ("extra unknown field",     {"title": "x", "bogus": "y"}),
]

PUT_CASES = [
    ("valid update",            {"title": "updated"}),
    ("title whitespace only",   {"title": "   "}),
    ("title empty",             {"title": ""}),
    ("title null keeps it",     {"title": None}),
    ("title is a number",       {"title": 123}),
    ("title exactly 120",       {"title": EXACT}),
    ("title 121",               {"title": LONG}),
    ("title 120 plus padding",  {"title": "  " + EXACT + "  "}),
    ("title 121 and bad prio",  {"title": LONG, "priority": "urgent"}),
    ("empty title and bad prio", {"title": "", "priority": "urgent"}),
    ("priority invalid",        {"priority": "urgent"}),
    ("priority null keeps it",  {"priority": None}),
    ("completed as string",     {"completed": "yes"}),
    ("empty body",              {}),
]


def call(method, url, body):
    """Return (status, error message), treating 4xx/5xx as data not exceptions.

    The message is None unless the response is one of this API's own
    `{"error": ...}` bodies, so problem-details responses compare on status
    alone rather than on wording nobody promised to share.
    """
    data = json.dumps(body).encode()
    req = urllib.request.Request(
        url, data=data, method=method,
        headers={"Content-Type": "application/json"},
    )
    try:
        with urllib.request.urlopen(req) as resp:
            return resp.status, error_of(resp.read())
    except urllib.error.HTTPError as exc:
        return exc.code, error_of(exc.read())


def error_of(raw):
    try:
        body = json.loads(raw)
    except ValueError:
        return None
    return body.get("error") if isinstance(body, dict) else None


def seed(base):
    """Create one task and return its id, so PUT has something to aim at."""
    data = json.dumps({"title": "seed"}).encode()
    req = urllib.request.Request(
        base + "/api/tasks", data=data, method="POST",
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req) as resp:
        return json.loads(resp.read())["id"]


def compare(method, label, f, c, failures):
    """Statuses must always match; messages only when both sides sent one."""
    (fs, fe), (cs, ce) = f, c
    same = fs == cs and (fe is None or ce is None or fe == ce)
    if not same:
        failures.append((method, label, f, c))
    note = "" if fe is None or ce is None else (" msg ok" if fe == ce else " MSG DIFFERS")
    print("  %-5s %-24s flask=%s csharp=%s  %s%s"
          % (method, label, fs, cs, "ok" if same else "DIVERGES", note))


def main(flask_base, csharp_base):
    failures = []
    checked = 0

    for label, body in POST_CASES:
        compare("POST", label,
                call("POST", flask_base + "/api/tasks", body),
                call("POST", csharp_base + "/api/tasks", body), failures)
        checked += 1

    fid, cid = seed(flask_base), seed(csharp_base)
    for label, body in PUT_CASES:
        compare("PUT", label,
                call("PUT", "%s/api/tasks/%d" % (flask_base, fid), body),
                call("PUT", "%s/api/tasks/%d" % (csharp_base, cid), body), failures)
        checked += 1

    print()
    if failures:
        print("%d of %d cases diverge:" % (len(failures), checked))
        for method, label, f, c in failures:
            print("  %s %s: flask %s %r, csharp %s %r"
                  % (method, label, f[0], f[1], c[0], c[1]))
        return 1
    print("All %d cases agree." % checked)
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(2)
    sys.exit(main(sys.argv[1].rstrip("/"), sys.argv[2].rstrip("/")))
