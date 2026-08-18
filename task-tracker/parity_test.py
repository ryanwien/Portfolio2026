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

Only status codes are compared. The bodies differ by design (the C# side is
typed and phrases some binding failures differently) but the accept/reject
decision has to be identical, and that is what a caller actually depends on.
"""

import json
import sys
import urllib.error
import urllib.request

# (label, method, body) — POST cases run against the collection, PUT cases
# against a task seeded for each backend.
POST_CASES = [
    ("valid task",              {"title": "real task"}),
    ("title missing",           {}),
    ("title empty string",      {"title": ""}),
    ("title whitespace only",   {"title": "   "}),
    ("title null",              {"title": None}),
    ("title is a number",       {"title": 123}),
    ("title is an object",      {"title": {"a": 1}}),
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
    ("priority invalid",        {"priority": "urgent"}),
    ("priority null keeps it",  {"priority": None}),
    ("completed as string",     {"completed": "yes"}),
    ("empty body",              {}),
]


def call(method, url, body):
    """Return the status code, treating 4xx/5xx as data rather than errors."""
    data = json.dumps(body).encode()
    req = urllib.request.Request(
        url, data=data, method=method,
        headers={"Content-Type": "application/json"},
    )
    try:
        with urllib.request.urlopen(req) as resp:
            return resp.status
    except urllib.error.HTTPError as exc:
        return exc.code


def seed(base):
    """Create one task and return its id, so PUT has something to aim at."""
    data = json.dumps({"title": "seed"}).encode()
    req = urllib.request.Request(
        base + "/api/tasks", data=data, method="POST",
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req) as resp:
        return json.loads(resp.read())["id"]


def main(flask_base, csharp_base):
    failures = []
    checked = 0

    for label, body in POST_CASES:
        f = call("POST", flask_base + "/api/tasks", body)
        c = call("POST", csharp_base + "/api/tasks", body)
        checked += 1
        status = "ok" if f == c else "DIVERGES"
        if f != c:
            failures.append(("POST", label, f, c))
        print("  POST  %-24s flask=%s csharp=%s  %s" % (label, f, c, status))

    fid, cid = seed(flask_base), seed(csharp_base)
    for label, body in PUT_CASES:
        f = call("PUT", "%s/api/tasks/%d" % (flask_base, fid), body)
        c = call("PUT", "%s/api/tasks/%d" % (csharp_base, cid), body)
        checked += 1
        status = "ok" if f == c else "DIVERGES"
        if f != c:
            failures.append(("PUT", label, f, c))
        print("  PUT   %-24s flask=%s csharp=%s  %s" % (label, f, c, status))

    print()
    if failures:
        print("%d of %d cases diverge:" % (len(failures), checked))
        for method, label, f, c in failures:
            print("  %s %s: flask %s, csharp %s" % (method, label, f, c))
        return 1
    print("All %d cases agree." % checked)
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(2)
    sys.exit(main(sys.argv[1].rstrip("/"), sys.argv[2].rstrip("/")))
