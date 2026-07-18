namespace SecurityLogAnalyzer;

/// <summary>Analyst-facing text report. Byte-identical to analyze.py's output.</summary>
public static class Report
{
    public static void Print(List<AuthEvent> events, List<Alert> alerts, TextWriter output)
    {
        var counts = alerts.GroupBy(a => a.Severity)
                           .ToDictionary(g => g.Key, g => g.Count(), StringComparer.Ordinal);
        int Count(string sev) => counts.TryGetValue(sev, out var n) ? n : 0;

        output.WriteLine(new string('=', 68));
        output.WriteLine("SECURITY LOG ANALYSIS");
        output.WriteLine(new string('=', 68));
        output.WriteLine($"  Events analyzed: {events.Count}");
        if (events.Count > 0)
            output.WriteLine($"  Time range:      {events[0].Ts}  ->  {events[^1].Ts}");
        output.WriteLine($"  Alerts:          {alerts.Count}  " +
                         $"(CRIT {Count("CRITICAL")} | HIGH {Count("HIGH")} | " +
                         $"MED {Count("MEDIUM")} | LOW {Count("LOW")})");
        output.WriteLine(new string('-', 68));

        if (alerts.Count == 0)
        {
            output.WriteLine("  No suspicious activity detected.");
            return;
        }

        foreach (var a in alerts)
        {
            output.WriteLine();
            output.WriteLine($"  [{a.Severity}] {a.Technique}  ({a.AttackId})");
            output.WriteLine($"    entity : {a.Entity}");
            output.WriteLine($"    detail : {a.Detail}");
            output.WriteLine($"    window : {a.FirstSeen} -> {a.LastSeen}  ({a.Events} events)");
        }
        output.WriteLine();
    }
}
