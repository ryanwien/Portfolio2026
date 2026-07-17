"""
analyze.py — SIEM-style security log analyzer

Ingests a stream of authentication/access events and detects common attack
patterns a SOC analyst triages every day, mapping each finding to its MITRE
ATT&CK technique. This is the kind of detection logic a SIEM (Splunk,
Microsoft Sentinel, Elastic) runs continuously against log data.

Detections:
    - Brute force            — many failed logins from one source     (T1110)
    - Brute-force success     — a success right after a failed burst   (T1110)
    - Password spraying       — one source, one try across many users  (T1110.003)
    - Port scan               — one source probing many ports/services (T1046)
    - Impossible travel       — one user, two far-apart logins, fast    (T1078)
    - Off-hours privileged    — admin/service login outside work hours  (T1078)

Every rule is thresholded (see DEFAULTS) so you can tune sensitivity, which is
the real day-to-day work of reducing false positives.

Usage:
    python analyze.py                          # analyze the bundled sample log
    python analyze.py path/to/events.jsonl     # analyze your own log
    python analyze.py --format json            # machine-readable alerts
    python analyze.py --bf-threshold 5         # tune brute-force sensitivity

Input format: JSON Lines (one JSON object per line) with the fields
    ts, src_ip, country, lat, lon, user, service, port, action, status

Built as a defensive security tool: log parsing, sliding-window detection,
geo-velocity analysis, and clear analyst-facing reporting. Standard library only.
"""

import argparse
import json
import math
import os
import sys
from collections import defaultdict
from datetime import datetime, timezone

DATA_PATH = os.path.join(os.path.dirname(__file__), "data", "auth_events.jsonl")

# ---------------------------------------------------------------------------
# Detection thresholds — the knobs a SOC analyst tunes to balance signal vs noise.
# ---------------------------------------------------------------------------
DEFAULTS = {
    "bf_threshold": 8,       # failed logins from one IP...
    "bf_window": 300,        # ...within this many seconds -> brute force
    "spray_users": 6,        # distinct users one IP fails against -> spraying
    "scan_ports": 10,        # distinct ports one IP probes...
    "scan_window": 120,      # ...within this many seconds -> port scan
    "travel_kmh": 800,       # implied speed above this between logins -> impossible travel
    "work_start": 7,         # business hours (local/UTC) start...
    "work_end": 19,          # ...and end, for off-hours detection
}
PRIVILEGED = ("admin", "root", "administrator", "svc_")

SEVERITY_RANK = {"CRITICAL": 0, "HIGH": 1, "MEDIUM": 2, "LOW": 3}


def parse_ts(s):
    """Parse an ISO-8601 timestamp (accepting a trailing 'Z') to a datetime."""
    return datetime.fromisoformat(s.replace("Z", "+00:00")).astimezone(timezone.utc)


def load_events(path):
    """Read a JSON Lines log file into a list of event dicts, sorted by time."""
    if not os.path.isfile(path):
        print(f"Error: log file '{path}' not found.")
        sys.exit(1)

    events = []
    with open(path, "r", encoding="utf-8") as fh:
        for lineno, line in enumerate(fh, 1):
            line = line.strip()
            if not line:
                continue
            try:
                e = json.loads(line)
                e["_dt"] = parse_ts(e["ts"])
                events.append(e)
            except (json.JSONDecodeError, KeyError, ValueError) as exc:
                print(f"  warning: skipping malformed line {lineno}: {exc}")

    events.sort(key=lambda e: e["_dt"])
    return events


def haversine_km(lat1, lon1, lat2, lon2):
    """Great-circle distance between two lat/lon points, in kilometers."""
    r = 6371.0
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dp = math.radians(lat2 - lat1)
    dl = math.radians(lon2 - lon1)
    a = math.sin(dp / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2
    return 2 * r * math.asin(math.sqrt(a))


def is_privileged(user):
    u = (user or "").lower()
    return any(u == p or u.startswith(p) for p in PRIVILEGED)


def alert(severity, technique, tid, entity, detail, count, first, last):
    """Build one normalized alert record."""
    return {
        "severity": severity,
        "technique": technique,
        "attack_id": tid,
        "entity": entity,
        "detail": detail,
        "events": count,
        "first_seen": first,
        "last_seen": last,
    }


# ---------------------------------------------------------------------------
# Detection rules. Each takes the full event list + config and returns alerts.
# ---------------------------------------------------------------------------
def detect_brute_force(events, cfg):
    """Failed logins clustering from one source IP within a time window."""
    alerts = []
    by_ip = defaultdict(list)
    for e in events:
        if e.get("action") == "login" and e.get("status") == "fail":
            by_ip[e["src_ip"]].append(e)

    for ip, fails in by_ip.items():
        fails.sort(key=lambda e: e["_dt"])
        # sliding window: find the densest burst
        i, best = 0, []
        for j in range(len(fails)):
            while (fails[j]["_dt"] - fails[i]["_dt"]).total_seconds() > cfg["bf_window"]:
                i += 1
            if j - i + 1 > len(best):
                best = fails[i:j + 1]
        if len(best) >= cfg["bf_threshold"]:
            users = sorted({e["user"] for e in best})
            # Compromise check: a success from this IP after the burst.
            burst_end = best[-1]["_dt"]
            success = next((e for e in events
                            if e["src_ip"] == ip and e.get("status") == "success"
                            and e["_dt"] >= best[0]["_dt"]), None)
            if success:
                alerts.append(alert(
                    "CRITICAL", "Brute-force success (likely compromise)", "T1110",
                    ip, f"{len(best)} failed logins then a SUCCESS as '{success['user']}' "
                        f"({success['service']}) from {ip} ({best[0]['country']})",
                    len(best) + 1, best[0]["ts"], success["ts"]))
            else:
                alerts.append(alert(
                    "HIGH", "Brute force", "T1110", ip,
                    f"{len(best)} failed logins in {cfg['bf_window']}s targeting "
                    f"{', '.join(users)} from {ip} ({best[0]['country']})",
                    len(best), best[0]["ts"], best[-1]["ts"]))
    return alerts


def detect_password_spray(events, cfg):
    """One source IP failing against many distinct usernames (low-and-slow)."""
    alerts = []
    users_by_ip = defaultdict(set)
    evs_by_ip = defaultdict(list)
    for e in events:
        if e.get("action") == "login" and e.get("status") == "fail":
            users_by_ip[e["src_ip"]].add(e["user"])
            evs_by_ip[e["src_ip"]].append(e)

    for ip, users in users_by_ip.items():
        if len(users) >= cfg["spray_users"]:
            evs = sorted(evs_by_ip[ip], key=lambda e: e["_dt"])
            alerts.append(alert(
                "HIGH", "Password spraying", "T1110.003", ip,
                f"{len(users)} distinct users targeted from {ip} ({evs[0]['country']}): "
                f"{', '.join(sorted(users))}",
                len(evs), evs[0]["ts"], evs[-1]["ts"]))
    return alerts


def detect_port_scan(events, cfg):
    """One source IP probing many distinct ports in a short window."""
    alerts = []
    by_ip = defaultdict(list)
    for e in events:
        if e.get("action") == "port_probe":
            by_ip[e["src_ip"]].append(e)

    for ip, probes in by_ip.items():
        probes.sort(key=lambda e: e["_dt"])
        i, best = 0, []
        for j in range(len(probes)):
            while (probes[j]["_dt"] - probes[i]["_dt"]).total_seconds() > cfg["scan_window"]:
                i += 1
            window = probes[i:j + 1]
            if len({p["port"] for p in window}) > len({p["port"] for p in best}):
                best = window
        ports = sorted({p["port"] for p in best})
        if len(ports) >= cfg["scan_ports"]:
            alerts.append(alert(
                "MEDIUM", "Port scan / service discovery", "T1046", ip,
                f"{len(ports)} ports probed from {ip} ({best[0]['country']}): "
                f"{', '.join(str(p) for p in ports)}",
                len(best), best[0]["ts"], best[-1]["ts"]))
    return alerts


def detect_impossible_travel(events, cfg):
    """One user with two successful logins too far apart to be physically possible."""
    alerts = []
    by_user = defaultdict(list)
    for e in events:
        if e.get("action") == "login" and e.get("status") == "success":
            by_user[e["user"]].append(e)

    for user, logins in by_user.items():
        logins.sort(key=lambda e: e["_dt"])
        # Keep only the single most-suspicious (fastest) hop per user, so one
        # traveling account doesn't spam a dozen near-identical alerts.
        worst = None
        for a, b in zip(logins, logins[1:]):
            hours = (b["_dt"] - a["_dt"]).total_seconds() / 3600
            if hours <= 0:
                continue
            km = haversine_km(a["lat"], a["lon"], b["lat"], b["lon"])
            if km < 500:
                continue
            speed = km / hours
            if speed > cfg["travel_kmh"] and (worst is None or speed > worst[0]):
                worst = (speed, a, b, km, hours)
        if worst:
            speed, a, b, km, hours = worst
            alerts.append(alert(
                "HIGH", "Impossible travel", "T1078", user,
                f"'{user}' logged in from {a['country']} then {b['country']} "
                f"{hours * 60:.0f} min apart ({km:.0f} km, {speed:.0f} km/h)",
                2, a["ts"], b["ts"]))
    return alerts


def detect_offhours_privileged(events, cfg):
    """Successful privileged logins outside business hours."""
    alerts = []
    for e in events:
        if (e.get("action") == "login" and e.get("status") == "success"
                and is_privileged(e.get("user"))):
            hour = e["_dt"].hour
            if hour < cfg["work_start"] or hour >= cfg["work_end"]:
                alerts.append(alert(
                    "LOW", "Off-hours privileged access", "T1078", e["user"],
                    f"'{e['user']}' logged in at {e['_dt']:%H:%M} UTC "
                    f"(outside {cfg['work_start']:02d}:00-{cfg['work_end']:02d}:00) "
                    f"from {e['src_ip']}",
                    1, e["ts"], e["ts"]))
    return alerts


RULES = [
    detect_brute_force,
    detect_password_spray,
    detect_port_scan,
    detect_impossible_travel,
    detect_offhours_privileged,
]


def analyze(events, cfg):
    """Run every rule and return alerts sorted by severity."""
    alerts = []
    for rule in RULES:
        alerts.extend(rule(events, cfg))
    alerts.sort(key=lambda a: SEVERITY_RANK[a["severity"]])
    return alerts


def print_report(events, alerts, cfg):
    counts = defaultdict(int)
    for a in alerts:
        counts[a["severity"]] += 1

    print("=" * 68)
    print("SECURITY LOG ANALYSIS")
    print("=" * 68)
    print(f"  Events analyzed: {len(events)}")
    if events:
        print(f"  Time range:      {events[0]['ts']}  ->  {events[-1]['ts']}")
    print(f"  Alerts:          {len(alerts)}  "
          f"(CRIT {counts['CRITICAL']} | HIGH {counts['HIGH']} | "
          f"MED {counts['MEDIUM']} | LOW {counts['LOW']})")
    print("-" * 68)

    if not alerts:
        print("  No suspicious activity detected.")
        return

    for a in alerts:
        print(f"\n  [{a['severity']}] {a['technique']}  ({a['attack_id']})")
        print(f"    entity : {a['entity']}")
        print(f"    detail : {a['detail']}")
        print(f"    window : {a['first_seen']} -> {a['last_seen']}  ({a['events']} events)")
    print()


def main():
    parser = argparse.ArgumentParser(description="SIEM-style security log analyzer.")
    parser.add_argument("logfile", nargs="?", default=DATA_PATH,
                        help="JSON Lines log to analyze (defaults to bundled sample)")
    parser.add_argument("--format", choices=["text", "json"], default="text",
                        help="output format")
    parser.add_argument("--bf-threshold", type=int, default=DEFAULTS["bf_threshold"],
                        help="failed logins from one IP to flag brute force")
    args = parser.parse_args()

    cfg = dict(DEFAULTS, bf_threshold=args.bf_threshold)
    events = load_events(args.logfile)
    alerts = analyze(events, cfg)

    if args.format == "json":
        print(json.dumps(alerts, indent=2))
    else:
        print_report(events, alerts, cfg)


if __name__ == "__main__":
    main()
