using SecurityLogAnalyzer;
using Xunit;

namespace SecurityLogAnalyzerTests;

/// <summary>
/// The bundled synthetic log is the shared fixture between this suite and the
/// Python implementation. Any divergence in the rules shows up as a failure
/// here, which is the point: the two engines must agree.
/// </summary>
public sealed class SampleLogFixture
{
    public List<AuthEvent> Events { get; }

    public SampleLogFixture()
    {
        Events = LogLoader.Load(FindSampleLog());
    }

    private static string FindSampleLog()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null)
        {
            var candidate = Path.Combine(dir.FullName, "data", "auth_events.jsonl");
            if (File.Exists(candidate)) return candidate;
            dir = dir.Parent;
        }
        throw new FileNotFoundException("Could not locate data/auth_events.jsonl above the test binary.");
    }
}

public sealed class GoldenLogTests : IClassFixture<SampleLogFixture>
{
    private readonly SampleLogFixture _fixture;
    private readonly List<Alert> _alerts;

    public GoldenLogTests(SampleLogFixture fixture)
    {
        _fixture = fixture;
        _alerts = Detectors.Analyze(_fixture.Events, new DetectionConfig());
    }

    [Fact]
    public void LoadsEveryEventInChronologicalOrder()
    {
        Assert.Equal(221, _fixture.Events.Count);
        for (var i = 1; i < _fixture.Events.Count; i++)
            Assert.True(_fixture.Events[i - 1].When <= _fixture.Events[i].When);
    }

    [Fact]
    public void ProducesTheSameSixAlertsAsThePythonEngine()
    {
        Assert.Equal(6, _alerts.Count);
        Assert.Equal(1, _alerts.Count(a => a.Severity == "CRITICAL"));
        Assert.Equal(2, _alerts.Count(a => a.Severity == "HIGH"));
        Assert.Equal(1, _alerts.Count(a => a.Severity == "MEDIUM"));
        Assert.Equal(2, _alerts.Count(a => a.Severity == "LOW"));
    }

    [Fact]
    public void RanksMostSevereFirst()
    {
        var expected = new[] { "CRITICAL", "HIGH", "HIGH", "MEDIUM", "LOW", "LOW" };
        Assert.Equal(expected, _alerts.Select(a => a.Severity).ToArray());
    }

    [Theory]
    // Every field of every alert, exactly as analyze.py emits it.
    [InlineData(0, "CRITICAL", "Brute-force success (likely compromise)", "T1110", "198.51.100.7", 44,
        "43 failed logins then a SUCCESS as 'admin' (ssh) from 198.51.100.7 (RU)")]
    [InlineData(1, "HIGH", "Password spraying", "T1110.003", "203.0.113.44", 8,
        "8 distinct users targeted from 203.0.113.44 (CN): a.chen, d.nguyen, j.martinez, k.obrien, m.rossi, r.kowalski, s.patel, t.johnson")]
    [InlineData(2, "HIGH", "Impossible travel", "T1078", "j.martinez", 2,
        "'j.martinez' logged in from US then RU 100 min apart (7511 km, 4507 km/h)")]
    [InlineData(3, "MEDIUM", "Port scan / service discovery", "T1046", "203.0.113.88", 16,
        "16 ports probed from 203.0.113.88 (CN): 21, 22, 23, 25, 53, 80, 110, 143, 443, 445, 993, 1433, 3306, 3389, 5432, 8080")]
    [InlineData(4, "LOW", "Off-hours privileged access", "T1078", "admin", 1,
        "'admin' logged in at 02:04 UTC (outside 07:00-19:00) from 198.51.100.7")]
    [InlineData(5, "LOW", "Off-hours privileged access", "T1078", "svc_backup", 1,
        "'svc_backup' logged in at 04:15 UTC (outside 07:00-19:00) from 10.0.1.20")]
    public void AlertMatchesPythonOutputExactly(
        int index, string severity, string technique, string attackId,
        string entity, int events, string detail)
    {
        var a = _alerts[index];
        Assert.Equal(severity, a.Severity);
        Assert.Equal(technique, a.Technique);
        Assert.Equal(attackId, a.AttackId);
        Assert.Equal(entity, a.Entity);
        Assert.Equal(events, a.Events);
        Assert.Equal(detail, a.Detail);
    }

    [Fact]
    public void LoweringTheBruteForceThresholdSurfacesMoreAlerts()
    {
        // Tuning sensitivity is the real day-to-day work; a looser threshold
        // must catch strictly more, never fewer.
        var sensitive = Detectors.Analyze(_fixture.Events, new DetectionConfig { BruteForceThreshold = 4 });
        Assert.True(sensitive.Count > _alerts.Count);
    }

    [Fact]
    public void RaisingTheBruteForceThresholdSuppressesTheBurst()
    {
        var strict = Detectors.Analyze(_fixture.Events, new DetectionConfig { BruteForceThreshold = 99 });
        Assert.DoesNotContain(strict, a => a.AttackId == "T1110" && a.Technique.StartsWith("Brute"));
    }
}

public sealed class RuleUnitTests
{
    [Fact]
    public void HaversineMatchesAKnownGreatCircleDistance()
    {
        // London Heathrow -> JFK is about 5555 km.
        var km = Detectors.HaversineKm(51.4700, -0.4543, 40.6413, -73.7781);
        Assert.InRange(km, 5540, 5570);
    }

    [Fact]
    public void HaversineIsZeroForIdenticalPoints()
    {
        Assert.Equal(0, Detectors.HaversineKm(10, 20, 10, 20), 9);
    }

    [Theory]
    [InlineData("admin", true)]
    [InlineData("ADMIN", true)]
    [InlineData("root", true)]
    [InlineData("administrator", true)]
    [InlineData("svc_backup", true)]
    [InlineData("j.martinez", false)]
    [InlineData("", false)]
    [InlineData(null, false)]
    public void PrivilegedAccountsAreRecognizedCaseInsensitively(string? user, bool expected)
        => Assert.Equal(expected, Detectors.IsPrivileged(user));

    [Fact]
    public void ShortHopsAreNotImpossibleTravel()
    {
        // 100 km in 10 minutes is 600 km/h — fast, but under the 500 km floor,
        // so it must not fire. Guards against flagging normal commutes.
        var events = new List<AuthEvent>
        {
            Login("u", "2026-01-01T00:00:00Z", 51.5, -0.12),
            Login("u", "2026-01-01T00:10:00Z", 52.4, -0.12),
        };
        Assert.Empty(Detectors.ImpossibleTravel(events, new DetectionConfig()));
    }

    [Fact]
    public void SlowLongHaulTravelIsNotFlagged()
    {
        // London -> New York over 12 hours is roughly 463 km/h. Plausible.
        var events = new List<AuthEvent>
        {
            Login("u", "2026-01-01T00:00:00Z", 51.4700, -0.4543),
            Login("u", "2026-01-01T12:00:00Z", 40.6413, -73.7781),
        };
        Assert.Empty(Detectors.ImpossibleTravel(events, new DetectionConfig()));
    }

    [Fact]
    public void OnlyTheFastestHopIsReportedPerUser()
    {
        // Three implausible hops for one account should still yield one alert,
        // so a single roaming user can't drown the queue.
        var events = new List<AuthEvent>
        {
            Login("u", "2026-01-01T00:00:00Z", 51.4700, -0.4543),
            Login("u", "2026-01-01T01:00:00Z", 40.6413, -73.7781),
            Login("u", "2026-01-01T02:00:00Z", 51.4700, -0.4543),
        };
        var alerts = Detectors.ImpossibleTravel(events, new DetectionConfig());
        Assert.Single(alerts);
    }

    [Theory]
    [InlineData("2026-01-01T06:59:00Z", true)]   // just before business hours
    [InlineData("2026-01-01T07:00:00Z", false)]  // boundary: work starts
    [InlineData("2026-01-01T18:59:00Z", false)]  // last business minute
    [InlineData("2026-01-01T19:00:00Z", true)]   // boundary: work ends
    public void OffHoursBoundariesAreInclusiveOfTheWorkingDay(string ts, bool shouldAlert)
    {
        var events = new List<AuthEvent>
        {
            new() { Ts = ts, When = LogLoader.ParseTimestamp(ts), SrcIp = "10.0.0.1",
                    User = "admin", Action = "login", Status = "success" },
        };
        var alerts = Detectors.OffHoursPrivileged(events, new DetectionConfig());
        Assert.Equal(shouldAlert, alerts.Count == 1);
    }

    [Fact]
    public void UnprivilegedOffHoursLoginsAreIgnored()
    {
        var events = new List<AuthEvent>
        {
            new() { Ts = "2026-01-01T03:00:00Z", When = LogLoader.ParseTimestamp("2026-01-01T03:00:00Z"),
                    SrcIp = "10.0.0.1", User = "j.martinez", Action = "login", Status = "success" },
        };
        Assert.Empty(Detectors.OffHoursPrivileged(events, new DetectionConfig()));
    }

    [Fact]
    public void MalformedLinesAreSkippedRatherThanAbortingTheRun()
    {
        var path = Path.GetTempFileName();
        try
        {
            File.WriteAllLines(path, new[]
            {
                """{"ts":"2026-01-01T00:00:00Z","src_ip":"1.2.3.4","user":"a","action":"login","status":"fail"}""",
                "{ this is not json",
                "",
                """{"ts":"2026-01-01T00:01:00Z","src_ip":"1.2.3.4","user":"b","action":"login","status":"fail"}""",
            });

            var events = LogLoader.Load(path, TextWriter.Null);
            Assert.Equal(2, events.Count);
        }
        finally { File.Delete(path); }
    }

    private static AuthEvent Login(string user, string ts, double lat, double lon) => new()
    {
        Ts = ts,
        When = LogLoader.ParseTimestamp(ts),
        SrcIp = "203.0.113.1",
        Country = "XX",
        User = user,
        Action = "login",
        Status = "success",
        Lat = lat,
        Lon = lon,
    };
}
