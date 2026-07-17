#!/usr/bin/env bash
# collect_macos.sh — Collect sshd authentication events from the macOS unified
# log into the JSON Lines format that analyze.py (and the browser demo) read.
#
# macOS has no /var/log/auth.log; auth events live in the unified log and are
# queried with `log show`. This targets sshd, so it needs Remote Login (SSH)
# to have been enabled and used. Best-effort — message formats can vary by
# macOS version.
#
# Usage:
#   bash collect_macos.sh            > ~/mac_events.jsonl     # last 7 days
#   bash collect_macos.sh 30         > ~/mac_events.jsonl     # last 30 days
#   python3 analyze.py ~/mac_events.jsonl
#   # ...or load ~/mac_events.jsonl in the browser demo.

DAYS="${1:-7}"

log show --last "${DAYS}d" --predicate 'process == "sshd"' --style syslog 2>/dev/null | awk '
/sshd(-session)?\[[0-9]+\]:.*(Failed|Accepted) password for/ {
  date=$1; time=$2; sub(/\..*/, "", time);          # 14:02:11.123-0700 -> 14:02:11
  ts=date "T" time "Z";                              # treat local time as UTC (demo simplification)
  status="fail"; if ($0 ~ /Accepted/) status="success";
  user="-"; ip="-"; port=0;
  for (i=1; i<=NF; i++) {
    if ($i=="for")  { if ($(i+1)=="invalid") user=$(i+3); else user=$(i+1); }
    if ($i=="from") ip=$(i+1);
    if ($i=="port") port=$(i+1)+0;
  }
  gsub(/"/, "", user);
  printf "{\"ts\":\"%s\",\"src_ip\":\"%s\",\"user\":\"%s\",\"service\":\"ssh\",\"port\":%d,\"action\":\"login\",\"status\":\"%s\"}\n", ts, ip, user, port, status;
}'
