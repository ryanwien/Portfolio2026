using System.Text.Json;
using System.Text.Json.Serialization;
using SecurityLogAnalyzer;

// ---------------------------------------------------------------------------
// CLI entry point. Mirrors analyze.py's arguments and output byte for byte, so
// the two implementations can be diffed directly against the same log.
//
//   analyze                            analyze the bundled sample log
//   analyze path/to/events.jsonl       analyze your own log
//   analyze --format json              machine-readable alerts
//   analyze --bf-threshold 5           tune brute-force sensitivity
// ---------------------------------------------------------------------------

string? logFile = null;
var format = "text";
var bfThreshold = new DetectionConfig().BruteForceThreshold;

for (var i = 0; i < args.Length; i++)
{
    switch (args[i])
    {
        case "--format" when i + 1 < args.Length:
            format = args[++i];
            if (format is not ("text" or "json"))
            {
                Console.Error.WriteLine("Error: --format must be 'text' or 'json'.");
                return 2;
            }
            break;

        case "--bf-threshold" when i + 1 < args.Length:
            if (!int.TryParse(args[++i], out bfThreshold))
            {
                Console.Error.WriteLine("Error: --bf-threshold must be an integer.");
                return 2;
            }
            break;

        case "-h" or "--help":
            Console.WriteLine("usage: analyze [logfile] [--format text|json] [--bf-threshold N]");
            return 0;

        default:
            if (args[i].StartsWith('-'))
            {
                Console.Error.WriteLine($"Error: unknown option '{args[i]}'.");
                return 2;
            }
            logFile = args[i];
            break;
    }
}

// Default to the bundled sample, resolved relative to this project rather than
// the working directory, so `dotnet run` works from anywhere.
logFile ??= Path.Combine(AppContext.BaseDirectory,
    "..", "..", "..", "..", "..", "..", "data", "auth_events.jsonl");

if (!File.Exists(logFile))
{
    Console.WriteLine($"Error: log file '{logFile}' not found.");
    return 1;
}

var config = new DetectionConfig { BruteForceThreshold = bfThreshold };
var events = LogLoader.Load(logFile, Console.Out);
var alerts = Detectors.Analyze(events, config);

if (format == "json")
{
    Console.WriteLine(JsonSerializer.Serialize(alerts, new JsonSerializerOptions
    {
        WriteIndented = true,
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower,
        DefaultIgnoreCondition = JsonIgnoreCondition.Never,
    }));
}
else
{
    Report.Print(events, alerts, Console.Out);
}

return 0;
