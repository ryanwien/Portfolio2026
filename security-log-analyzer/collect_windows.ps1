<#
collect_windows.ps1 — Collect REAL Windows logon events into the JSON Lines
format that analyze.py understands, so the analyzer runs against your own machine.

Sources (tried in order):
  1. Security log, Event IDs 4624 (logon) / 4625 (FAILED logon).
     The canonical SOC source — includes failed logons, so brute-force and
     password-spray detections light up. Requires an ELEVATED PowerShell
     (Run as administrator), because the Security log is protected.
  2. TerminalServices-LocalSessionManager/Operational session logons (21/24/25).
     Readable WITHOUT admin. Successful console/RDP sessions only (no failures).

Usage (from the security-log-analyzer folder):
    powershell -ExecutionPolicy Bypass -File collect_windows.ps1 -Hours 168
    python analyze.py data/windows_events.jsonl

Nothing is uploaded — events are written to a local file you then analyze locally.
#>
param(
    [int]$Hours = 168,                          # look-back window (default 7 days)
    [string]$Out = "data/windows_events.jsonl"
)

function New-Event($ts, $ip, $user, $service, $port, $action, $status) {
    [ordered]@{ ts = $ts; src_ip = $ip; user = $user; service = $service;
        port = $port; action = $action; status = $status }
}

$since  = (Get-Date).AddHours(-$Hours)
$events = New-Object System.Collections.Generic.List[object]
$source = ""

try {
    # ---- Source 1: Security log (needs admin) ----
    $sec = Get-WinEvent -FilterHashtable @{LogName = 'Security'; Id = 4624, 4625; StartTime = $since } -ErrorAction Stop
    foreach ($e in $sec) {
        $x = [xml]$e.ToXml()
        $d = @{}
        foreach ($n in $x.Event.EventData.Data) { $d[$n.Name] = $n.'#text' }
        $user = $d['TargetUserName']
        if (-not $user -or $user -in @('SYSTEM', 'ANONYMOUS LOGON', '-', 'DWM-1', 'UMFD-0')) { continue }
        $ip = $d['IpAddress']
        if (-not $ip -or $ip -in @('-', '::1', '127.0.0.1')) { $ip = 'local' }
        $status = if ($e.Id -eq 4624) { 'success' } else { 'fail' }
        $ts = $e.TimeCreated.ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
        $events.Add((New-Event $ts $ip $user "logon(type$($d['LogonType']))" 0 'login' $status))
    }
    $source = "Security log (Event IDs 4624/4625)"
}
catch {
    # ---- Source 2: TerminalServices session log (no admin) ----
    Write-Host "Security log not accessible ($($_.Exception.Message.Trim()))"
    Write-Host "Falling back to the session log (no admin needed; successful logons only).`n"
    $log = 'Microsoft-Windows-TerminalServices-LocalSessionManager/Operational'
    $ts_events = Get-WinEvent -FilterHashtable @{LogName = $log; Id = 21, 24, 25; StartTime = $since } -ErrorAction SilentlyContinue
    foreach ($e in $ts_events) {
        $x = [xml]$e.ToXml()
        $node = $x.Event.UserData.EventXML
        $user = $node.User
        $addr = $node.Address
        $ip = if (-not $addr -or $addr -eq 'LOCAL') { 'local' } else { $addr }
        $action = if ($e.Id -eq 24) { 'disconnect' } else { 'login' }
        $ts = $e.TimeCreated.ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
        $events.Add((New-Event $ts $ip $user 'rdp/console' 3389 $action 'success'))
    }
    $source = "TerminalServices session log (Event IDs 21/24/25)"
}

# Write JSON Lines (UTF-8 without BOM, so Python parses the first line cleanly)
$lines = $events | Sort-Object { $_.ts } | ForEach-Object { $_ | ConvertTo-Json -Compress }
$full = Join-Path (Get-Location) $Out
$dir = Split-Path -Parent $full
if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
[System.IO.File]::WriteAllLines($full, $lines, (New-Object System.Text.UTF8Encoding($false)))

Write-Host "Collected $($events.Count) event(s) from your $source"
Write-Host "Wrote $Out"
Write-Host "Now run:  python analyze.py $Out"
