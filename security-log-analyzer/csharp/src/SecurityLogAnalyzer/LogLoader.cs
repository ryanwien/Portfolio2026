using System.Globalization;
using System.Text.Json;

namespace SecurityLogAnalyzer;

/// <summary>Reads a JSON Lines authentication log into time-ordered events.</summary>
public static class LogLoader
{
    private static readonly JsonSerializerOptions Options = new()
    {
        PropertyNameCaseInsensitive = true,
        ReadCommentHandling = JsonCommentHandling.Skip,
    };

    /// <summary>
    /// Parse an ISO-8601 timestamp, accepting a trailing 'Z', and normalize to UTC.
    /// </summary>
    public static DateTimeOffset ParseTimestamp(string s) =>
        DateTimeOffset.Parse(s, CultureInfo.InvariantCulture, DateTimeStyles.AssumeUniversal)
                      .ToUniversalTime();

    /// <summary>
    /// Read every line of a JSON Lines log. Malformed lines are reported and
    /// skipped rather than aborting the run, so one bad record can't blind the
    /// analyst to the rest of the log.
    /// </summary>
    public static List<AuthEvent> Load(string path, TextWriter? warnings = null)
    {
        var events = new List<AuthEvent>();
        var lineNo = 0;

        foreach (var raw in File.ReadLines(path))
        {
            lineNo++;
            var line = raw.Trim();
            if (line.Length == 0) continue;

            try
            {
                var e = JsonSerializer.Deserialize<AuthEvent>(line, Options);
                if (e is null || string.IsNullOrEmpty(e.Ts)) throw new FormatException("missing ts");
                e.When = ParseTimestamp(e.Ts);
                events.Add(e);
            }
            catch (Exception ex) when (ex is JsonException or FormatException)
            {
                warnings?.WriteLine($"  warning: skipping malformed line {lineNo}: {ex.Message}");
            }
        }

        // Detections assume chronological order; sort once here rather than in
        // each rule. OrderBy is stable, matching Python's list.sort().
        return events.OrderBy(e => e.When).ToList();
    }
}
