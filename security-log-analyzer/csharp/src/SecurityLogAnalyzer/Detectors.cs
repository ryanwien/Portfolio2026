using System.Globalization;

namespace SecurityLogAnalyzer;

/// <summary>
/// The detection rules. Each takes the full event stream plus the tuned
/// thresholds and returns the alerts it found. Ported rule-for-rule from
/// analyze.py — the two implementations must agree on any given log.
/// </summary>
public static class Detectors
{
    private static readonly string[] Privileged = { "admin", "root", "administrator", "svc_" };

    private static readonly Dictionary<string, int> SeverityRank = new()
    {
        ["CRITICAL"] = 0, ["HIGH"] = 1, ["MEDIUM"] = 2, ["LOW"] = 3,
    };

    /// <summary>Great-circle distance between two lat/lon points, in kilometres.</summary>
    public static double HaversineKm(double lat1, double lon1, double lat2, double lon2)
    {
        const double r = 6371.0;
        var p1 = double.DegreesToRadians(lat1);
        var p2 = double.DegreesToRadians(lat2);
        var dp = double.DegreesToRadians(lat2 - lat1);
        var dl = double.DegreesToRadians(lon2 - lon1);
        var a = Math.Pow(Math.Sin(dp / 2), 2) +
                Math.Cos(p1) * Math.Cos(p2) * Math.Pow(Math.Sin(dl / 2), 2);
        return 2 * r * Math.Asin(Math.Sqrt(a));
    }

    public static bool IsPrivileged(string? user)
    {
        var u = (user ?? "").ToLowerInvariant();
        return Privileged.Any(p => u == p || u.StartsWith(p, StringComparison.Ordinal));
    }

    /// <summary>Format a double the way Python's "{:.0f}" does (round-half-to-even).</summary>
    private static string Round0(double v) =>
        Math.Round(v, MidpointRounding.ToEven).ToString("0", CultureInfo.InvariantCulture);

    private static bool IsFailedLogin(AuthEvent e) => e.Action == "login" && e.Status == "fail";

    /// <summary>Failed logins clustering from one source IP inside a time window.</summary>
    public static List<Alert> BruteForce(List<AuthEvent> events, DetectionConfig cfg)
    {
        var alerts = new List<Alert>();

        foreach (var (ip, fails) in GroupInOrder(events.Where(IsFailedLogin), e => e.SrcIp))
        {
            // Sliding window over the failures: find the densest burst.
            var i = 0;
            var bestStart = 0;
            var bestLen = 0;
            for (var j = 0; j < fails.Count; j++)
            {
                while ((fails[j].When - fails[i].When).TotalSeconds > cfg.BruteForceWindowSeconds) i++;
                if (j - i + 1 > bestLen) { bestStart = i; bestLen = j - i + 1; }
            }
            if (bestLen < cfg.BruteForceThreshold) continue;

            var burst = fails.GetRange(bestStart, bestLen);
            var first = burst[0];
            var last = burst[^1];

            // Compromise check: did anything from this IP succeed once the burst began?
            var success = events.FirstOrDefault(
                e => e.SrcIp == ip && e.Status == "success" && e.When >= first.When);

            alerts.Add(success is not null
                ? new Alert("CRITICAL", "Brute-force success (likely compromise)", "T1110", ip,
                    $"{bestLen} failed logins then a SUCCESS as '{success.User}' " +
                    $"({success.Service}) from {ip} ({first.CountryOrUnknown})",
                    bestLen + 1, first.Ts, success.Ts)
                : new Alert("HIGH", "Brute force", "T1110", ip,
                    $"{bestLen} failed logins in {cfg.BruteForceWindowSeconds}s targeting " +
                    $"{string.Join(", ", burst.Select(e => e.User ?? "").Distinct().Order(StringComparer.Ordinal))} " +
                    $"from {ip} ({first.CountryOrUnknown})",
                    bestLen, first.Ts, last.Ts));
        }

        return alerts;
    }

    /// <summary>One source IP failing against many distinct usernames (low-and-slow).</summary>
    public static List<Alert> PasswordSpray(List<AuthEvent> events, DetectionConfig cfg)
    {
        var alerts = new List<Alert>();

        foreach (var (ip, evs) in GroupInOrder(events.Where(IsFailedLogin), e => e.SrcIp))
        {
            var users = evs.Select(e => e.User ?? "").Distinct().Order(StringComparer.Ordinal).ToList();
            if (users.Count < cfg.SprayUsers) continue;

            alerts.Add(new Alert("HIGH", "Password spraying", "T1110.003", ip,
                $"{users.Count} distinct users targeted from {ip} ({evs[0].CountryOrUnknown}): " +
                string.Join(", ", users),
                evs.Count, evs[0].Ts, evs[^1].Ts));
        }

        return alerts;
    }

    /// <summary>One source IP probing many distinct ports in a short window.</summary>
    public static List<Alert> PortScan(List<AuthEvent> events, DetectionConfig cfg)
    {
        var alerts = new List<Alert>();

        foreach (var (ip, probes) in GroupInOrder(events.Where(e => e.Action == "port_probe"), e => e.SrcIp))
        {
            // Widen the window to whichever span covers the most distinct ports —
            // a scanner hammering one port repeatedly is not service discovery.
            var i = 0;
            var bestStart = 0;
            var bestLen = 0;
            var bestPorts = 0;
            for (var j = 0; j < probes.Count; j++)
            {
                while ((probes[j].When - probes[i].When).TotalSeconds > cfg.ScanWindowSeconds) i++;
                var distinct = probes.GetRange(i, j - i + 1).Select(p => p.Port).Distinct().Count();
                if (distinct > bestPorts) { bestStart = i; bestLen = j - i + 1; bestPorts = distinct; }
            }
            if (bestLen == 0) continue;

            var window = probes.GetRange(bestStart, bestLen);
            var ports = window.Select(p => p.Port).Distinct().Order().ToList();
            if (ports.Count < cfg.ScanPorts) continue;

            alerts.Add(new Alert("MEDIUM", "Port scan / service discovery", "T1046", ip,
                $"{ports.Count} ports probed from {ip} ({window[0].CountryOrUnknown}): " +
                string.Join(", ", ports.Select(p => p.ToString(CultureInfo.InvariantCulture))),
                window.Count, window[0].Ts, window[^1].Ts));
        }

        return alerts;
    }

    /// <summary>One user logging in from two places too far apart to be physically possible.</summary>
    public static List<Alert> ImpossibleTravel(List<AuthEvent> events, DetectionConfig cfg)
    {
        var alerts = new List<Alert>();

        var geoLogins = events.Where(e => e.Action == "login" && e.Status == "success"
                                          && e.Lat is not null && e.Lon is not null);

        foreach (var (user, logins) in GroupInOrder(geoLogins, e => e.User ?? ""))
        {
            // Keep only the single fastest hop per user, so one genuinely
            // travelling account doesn't spam a dozen near-identical alerts.
            (double Speed, AuthEvent A, AuthEvent B, double Km, double Hours)? worst = null;

            for (var k = 0; k + 1 < logins.Count; k++)
            {
                var a = logins[k];
                var b = logins[k + 1];
                var hours = (b.When - a.When).TotalSeconds / 3600.0;
                if (hours <= 0) continue;

                var km = HaversineKm(a.Lat!.Value, a.Lon!.Value, b.Lat!.Value, b.Lon!.Value);
                if (km < 500) continue;

                var speed = km / hours;
                if (speed > cfg.TravelKmh && (worst is null || speed > worst.Value.Speed))
                    worst = (speed, a, b, km, hours);
            }

            if (worst is null) continue;
            var (sp, ea, eb, dist, hrs) = worst.Value;

            alerts.Add(new Alert("HIGH", "Impossible travel", "T1078", user,
                $"'{user}' logged in from {ea.Country} then {eb.Country} " +
                $"{Round0(hrs * 60)} min apart ({Round0(dist)} km, {Round0(sp)} km/h)",
                2, ea.Ts, eb.Ts));
        }

        return alerts;
    }

    /// <summary>Successful privileged logins outside business hours.</summary>
    public static List<Alert> OffHoursPrivileged(List<AuthEvent> events, DetectionConfig cfg)
    {
        var alerts = new List<Alert>();

        foreach (var e in events)
        {
            if (e.Action != "login" || e.Status != "success" || !IsPrivileged(e.User)) continue;

            var hour = e.When.Hour;
            if (hour >= cfg.WorkStart && hour < cfg.WorkEnd) continue;

            alerts.Add(new Alert("LOW", "Off-hours privileged access", "T1078", e.User ?? "",
                $"'{e.User}' logged in at {e.When:HH:mm} UTC " +
                $"(outside {cfg.WorkStart:00}:00-{cfg.WorkEnd:00}:00) from {e.SrcIp}",
                1, e.Ts, e.Ts));
        }

        return alerts;
    }

    /// <summary>Every rule, in the order analyze.py runs them.</summary>
    public static readonly IReadOnlyList<Func<List<AuthEvent>, DetectionConfig, List<Alert>>> Rules =
        new Func<List<AuthEvent>, DetectionConfig, List<Alert>>[]
        {
            BruteForce, PasswordSpray, PortScan, ImpossibleTravel, OffHoursPrivileged,
        };

    /// <summary>Run every rule and rank the findings, most severe first.</summary>
    public static List<Alert> Analyze(List<AuthEvent> events, DetectionConfig cfg)
    {
        var alerts = new List<Alert>();
        foreach (var rule in Rules) alerts.AddRange(rule(events, cfg));

        // OrderBy is a stable sort, so within one severity the alerts stay in
        // rule order — same as Python's list.sort().
        return alerts.OrderBy(a => SeverityRank[a.Severity]).ToList();
    }

    /// <summary>
    /// Group while preserving first-seen key order and within-group event order.
    /// Python dicts iterate in insertion order, and the rules depend on it, so
    /// a hash-ordered GroupBy would reorder same-severity alerts.
    /// </summary>
    private static List<(string Key, List<AuthEvent> Items)> GroupInOrder(
        IEnumerable<AuthEvent> source, Func<AuthEvent, string> keySelector)
    {
        var order = new List<string>();
        var map = new Dictionary<string, List<AuthEvent>>(StringComparer.Ordinal);

        foreach (var e in source)
        {
            var key = keySelector(e);
            if (!map.TryGetValue(key, out var list))
            {
                list = new List<AuthEvent>();
                map[key] = list;
                order.Add(key);
            }
            list.Add(e);
        }

        return order.Select(k => (k, map[k])).ToList();
    }
}
