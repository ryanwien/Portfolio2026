using System.Text.Json.Serialization;

namespace SecurityLogAnalyzer;

/// <summary>
/// One authentication or access event, as read from a JSON Lines log.
/// Fields mirror the Python implementation's schema exactly; real host logs
/// often omit geo data, so country/lat/lon are optional.
/// </summary>
public sealed class AuthEvent
{
    [JsonPropertyName("ts")] public string Ts { get; set; } = "";
    [JsonPropertyName("src_ip")] public string SrcIp { get; set; } = "";
    [JsonPropertyName("country")] public string? Country { get; set; }
    [JsonPropertyName("lat")] public double? Lat { get; set; }
    [JsonPropertyName("lon")] public double? Lon { get; set; }
    [JsonPropertyName("user")] public string? User { get; set; }
    [JsonPropertyName("service")] public string? Service { get; set; }
    [JsonPropertyName("port")] public int Port { get; set; }
    [JsonPropertyName("action")] public string? Action { get; set; }
    [JsonPropertyName("status")] public string? Status { get; set; }

    /// <summary>Parsed <see cref="Ts"/>, normalized to UTC. Set by the loader.</summary>
    [JsonIgnore] public DateTimeOffset When { get; set; }

    /// <summary>Country, or "?" when the log carries no geo data.</summary>
    public string CountryOrUnknown => string.IsNullOrEmpty(Country) ? "?" : Country;
}

/// <summary>One normalized finding, mapped to a MITRE ATT&amp;CK technique.</summary>
public sealed record Alert(
    string Severity,
    string Technique,
    string AttackId,
    string Entity,
    string Detail,
    int Events,
    string FirstSeen,
    string LastSeen);

/// <summary>
/// Detection thresholds — the knobs a SOC analyst tunes to trade signal
/// against noise. Defaults match DEFAULTS in analyze.py.
/// </summary>
public sealed class DetectionConfig
{
    /// <summary>Failed logins from one IP...</summary>
    public int BruteForceThreshold { get; init; } = 8;

    /// <summary>...within this many seconds, to count as brute force.</summary>
    public int BruteForceWindowSeconds { get; init; } = 300;

    /// <summary>Distinct users one IP fails against before it reads as spraying.</summary>
    public int SprayUsers { get; init; } = 6;

    /// <summary>Distinct ports one IP probes...</summary>
    public int ScanPorts { get; init; } = 10;

    /// <summary>...within this many seconds, to count as a port scan.</summary>
    public int ScanWindowSeconds { get; init; } = 120;

    /// <summary>Implied travel speed above which a login pair is impossible.</summary>
    public double TravelKmh { get; init; } = 800;

    /// <summary>Business hours start (UTC), for off-hours detection.</summary>
    public int WorkStart { get; init; } = 7;

    /// <summary>Business hours end (UTC), for off-hours detection.</summary>
    public int WorkEnd { get; init; } = 19;
}
