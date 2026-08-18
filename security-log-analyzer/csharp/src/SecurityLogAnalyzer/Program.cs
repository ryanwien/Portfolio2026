using System.Globalization;
using System.Text.Encodings.Web;
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
// Every threshold the engine has is reachable from the command line, matching
// the flags analyze.py exposes so the two stay comparable under tuning.
var defaults = new DetectionConfig();
var bfThreshold = defaults.BruteForceThreshold;
var bfWindow = defaults.BruteForceWindowSeconds;
var sprayUsers = defaults.SprayUsers;
var scanPorts = defaults.ScanPorts;
var scanWindow = defaults.ScanWindowSeconds;
var travelKmh = defaults.TravelKmh;
var workStart = defaults.WorkStart;
var workEnd = defaults.WorkEnd;

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

        case "--bf-window" when i + 1 < args.Length:
            if (!int.TryParse(args[++i], out bfWindow))
            {
                Console.Error.WriteLine("Error: --bf-window must be an integer.");
                return 2;
            }
            break;

        case "--spray-users" when i + 1 < args.Length:
            if (!int.TryParse(args[++i], out sprayUsers))
            {
                Console.Error.WriteLine("Error: --spray-users must be an integer.");
                return 2;
            }
            break;

        case "--scan-ports" when i + 1 < args.Length:
            if (!int.TryParse(args[++i], out scanPorts))
            {
                Console.Error.WriteLine("Error: --scan-ports must be an integer.");
                return 2;
            }
            break;

        case "--scan-window" when i + 1 < args.Length:
            if (!int.TryParse(args[++i], out scanWindow))
            {
                Console.Error.WriteLine("Error: --scan-window must be an integer.");
                return 2;
            }
            break;

        case "--travel-kmh" when i + 1 < args.Length:
            if (!double.TryParse(args[++i], NumberStyles.Float, CultureInfo.InvariantCulture, out travelKmh))
            {
                Console.Error.WriteLine("Error: --travel-kmh must be a number.");
                return 2;
            }
            break;

        case "--work-start" when i + 1 < args.Length:
            if (!int.TryParse(args[++i], out workStart))
            {
                Console.Error.WriteLine("Error: --work-start must be an integer.");
                return 2;
            }
            break;

        case "--work-end" when i + 1 < args.Length:
            if (!int.TryParse(args[++i], out workEnd))
            {
                Console.Error.WriteLine("Error: --work-end must be an integer.");
                return 2;
            }
            break;

        case "-h" or "--help":
            Console.WriteLine("usage: analyze [logfile] [--format text|json]");
            Console.WriteLine("               [--bf-threshold N] [--bf-window SECONDS]");
            Console.WriteLine("               [--spray-users N]");
            Console.WriteLine("               [--scan-ports N] [--scan-window SECONDS]");
            Console.WriteLine("               [--travel-kmh KMH]");
            Console.WriteLine("               [--work-start HOUR] [--work-end HOUR]");
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

var config = new DetectionConfig
{
    BruteForceThreshold = bfThreshold,
    BruteForceWindowSeconds = bfWindow,
    SprayUsers = sprayUsers,
    ScanPorts = scanPorts,
    ScanWindowSeconds = scanWindow,
    TravelKmh = travelKmh,
    WorkStart = workStart,
    WorkEnd = workEnd,
};
var events = LogLoader.Load(logFile, Console.Out);
var alerts = Detectors.Analyze(events, config);

if (format == "json")
{
    Console.WriteLine(JsonSerializer.Serialize(alerts, new JsonSerializerOptions
    {
        WriteIndented = true,
        PropertyNamingPolicy = JsonNamingPolicy.SnakeCaseLower,
        DefaultIgnoreCondition = JsonIgnoreCondition.Never,
        // The default encoder escapes ' as \u0027 in case the output is pasted
        // into HTML. This goes to stdout and is compared against the Python
        // engine byte for byte, and a detail like "'admin' logged in" appears in
        // most alerts, so that escaping alone broke the identical-output claim.
        Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping,
    }));
}
else
{
    Report.Print(events, alerts, Console.Out);
}

return 0;
