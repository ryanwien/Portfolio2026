#!/usr/bin/env bash
# collect_linux.sh — Convert a Linux sshd auth log into the JSON Lines format
# that analyze.py reads. (The browser demo can also parse a raw auth.log directly,
# so this is mainly for the command-line workflow.)
#
# Usage:
#   bash collect_linux.sh                       > events.jsonl   # /var/log/auth.log
#   bash collect_linux.sh /var/log/auth.log     > events.jsonl
#   sudo journalctl -u ssh --since "7 days ago" | bash collect_linux.sh -  > events.jsonl
#   python3 analyze.py events.jsonl

SRC="${1:-/var/log/auth.log}"
YEAR="$(date +%Y)"

awk -v year="$YEAR" '
BEGIN { split("Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec", M, " ");
        for (i in M) mon[M[i]] = sprintf("%02d", i) }
/sshd\[[0-9]+\]:.*(Failed|Accepted) password for/ {
  ts = sprintf("%s-%s-%02dT%sZ", year, mon[$1], $2, $3);
  status = "fail"; if ($0 ~ /Accepted/) status = "success";
  user = "-"; ip = "-"; port = 0;
  for (i = 1; i <= NF; i++) {
    if ($i == "for")  { if ($(i+1) == "invalid") user = $(i+3); else user = $(i+1); }
    if ($i == "from") ip = $(i+1);
    if ($i == "port") port = $(i+1) + 0;
  }
  printf "{\"ts\":\"%s\",\"src_ip\":\"%s\",\"user\":\"%s\",\"service\":\"ssh\",\"port\":%d,\"action\":\"login\",\"status\":\"%s\"}\n", ts, ip, user, port, status;
}' "$SRC"
