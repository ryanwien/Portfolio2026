using System.Net;
using System.Net.Http.Json;
using System.Text;
using Microsoft.AspNetCore.Mvc.Testing;
using Microsoft.Extensions.DependencyInjection;
using TaskTracker.Api;
using Xunit;

namespace TaskTracker.Tests;

/// <summary>
/// Boots the real application against a throwaway SQLite file, so these
/// exercise the actual HTTP pipeline, routing, model binding and EF Core —
/// not a hand-rolled stand-in for them.
/// </summary>
public sealed class ApiFactory : WebApplicationFactory<Program>, IDisposable
{
    private readonly string _databasePath =
        Path.Combine(Path.GetTempPath(), $"tasktracker_test_{Guid.NewGuid():N}.db");

    protected override void ConfigureWebHost(Microsoft.AspNetCore.Hosting.IWebHostBuilder builder)
    {
        builder.UseSetting("Database:Path", _databasePath);
    }

    protected override void Dispose(bool disposing)
    {
        base.Dispose(disposing);
        if (disposing && File.Exists(_databasePath))
        {
            try { File.Delete(_databasePath); } catch (IOException) { /* best effort */ }
        }
    }
}

public sealed class ApiTests : IClassFixture<ApiFactory>
{
    private readonly HttpClient _client;

    public ApiTests(ApiFactory factory) => _client = factory.CreateClient();

    private async Task<TaskResponse> CreateAsync(
        string title, string? description = null, string? priority = null)
    {
        var response = await _client.PostAsJsonAsync("/api/tasks",
            new CreateTaskRequest { Title = title, Description = description, Priority = priority });
        response.EnsureSuccessStatusCode();
        return (await response.Content.ReadFromJsonAsync<TaskResponse>())!;
    }

    // ---- create ----------------------------------------------------------

    [Fact]
    public async Task CreatingATaskReturns201AndTheCreatedResource()
    {
        var response = await _client.PostAsJsonAsync("/api/tasks",
            new CreateTaskRequest { Title = "Review pull request", Priority = "high" });

        Assert.Equal(HttpStatusCode.Created, response.StatusCode);

        var task = await response.Content.ReadFromJsonAsync<TaskResponse>();
        Assert.NotNull(task);
        Assert.True(task!.Id > 0);
        Assert.Equal("Review pull request", task.Title);
        Assert.Equal("high", task.Priority);
        Assert.False(task.Completed);
        Assert.NotEmpty(task.CreatedAt);
    }

    [Fact]
    public async Task ANewTaskDefaultsToMediumPriorityAndNotCompleted()
    {
        var task = await CreateAsync("Defaults");
        Assert.Equal("medium", task.Priority);
        Assert.False(task.Completed);
        Assert.Equal("", task.Description);
    }

    [Fact]
    public async Task TitleAndDescriptionAreTrimmed()
    {
        var task = await CreateAsync("   padded title   ", "   padded body   ");
        Assert.Equal("padded title", task.Title);
        Assert.Equal("padded body", task.Description);
    }

    [Theory]
    [InlineData(null)]
    [InlineData("")]
    [InlineData("   ")]
    public async Task ATaskWithoutATitleIsRejected(string? title)
    {
        var response = await _client.PostAsJsonAsync("/api/tasks",
            new CreateTaskRequest { Title = title });

        Assert.Equal(HttpStatusCode.BadRequest, response.StatusCode);
        var error = await response.Content.ReadFromJsonAsync<ErrorResponse>();
        Assert.Equal("Title is required", error!.Error);
    }

    [Fact]
    public async Task AnUnknownPriorityIsRejected()
    {
        var response = await _client.PostAsJsonAsync("/api/tasks",
            new CreateTaskRequest { Title = "Bad priority", Priority = "urgent" });

        Assert.Equal(HttpStatusCode.BadRequest, response.StatusCode);
        var error = await response.Content.ReadFromJsonAsync<ErrorResponse>();
        Assert.Equal("Priority must be low, medium, or high", error!.Error);
    }

    // ---- read ------------------------------------------------------------

    [Fact]
    public async Task AKnownTaskCanBeFetchedById()
    {
        var created = await CreateAsync("Fetch me");
        var fetched = await _client.GetFromJsonAsync<TaskResponse>($"/api/tasks/{created.Id}");

        Assert.NotNull(fetched);
        Assert.Equal(created.Id, fetched!.Id);
        Assert.Equal("Fetch me", fetched.Title);
    }

    [Fact]
    public async Task AnUnknownTaskReturns404WithAnErrorBody()
    {
        var response = await _client.GetAsync("/api/tasks/999999");

        Assert.Equal(HttpStatusCode.NotFound, response.StatusCode);
        var error = await response.Content.ReadFromJsonAsync<ErrorResponse>();
        Assert.Equal("Task not found", error!.Error);
    }

    [Fact]
    public async Task TheCompletedFilterSelectsOnlyMatchingTasks()
    {
        var done = await CreateAsync("Filter: finished");
        await _client.PutAsJsonAsync($"/api/tasks/{done.Id}",
            new UpdateTaskRequest { Completed = true });
        var open = await CreateAsync("Filter: still open");

        var completed = await _client.GetFromJsonAsync<List<TaskResponse>>("/api/tasks?completed=true");
        var pending = await _client.GetFromJsonAsync<List<TaskResponse>>("/api/tasks?completed=false");

        Assert.Contains(completed!, t => t.Id == done.Id);
        Assert.DoesNotContain(completed!, t => t.Id == open.Id);
        Assert.Contains(pending!, t => t.Id == open.Id);
        Assert.DoesNotContain(pending!, t => t.Id == done.Id);
    }

    [Fact]
    public async Task TasksComeBackNewestFirst()
    {
        var first = await CreateAsync("Ordering: older");
        var second = await CreateAsync("Ordering: newer");

        var all = await _client.GetFromJsonAsync<List<TaskResponse>>("/api/tasks");
        var ids = all!.Select(t => t.Id).ToList();

        Assert.True(ids.IndexOf(second.Id) < ids.IndexOf(first.Id),
            "the more recently created task should appear first");
    }

    // ---- update ----------------------------------------------------------

    [Fact]
    public async Task AnUpdateOnlyChangesTheFieldsItSends()
    {
        var created = await CreateAsync("Partial update", "original body", "low");

        var response = await _client.PutAsJsonAsync($"/api/tasks/{created.Id}",
            new UpdateTaskRequest { Completed = true });
        response.EnsureSuccessStatusCode();

        var updated = await response.Content.ReadFromJsonAsync<TaskResponse>();
        Assert.True(updated!.Completed);
        Assert.Equal("Partial update", updated.Title);      // untouched
        Assert.Equal("original body", updated.Description); // untouched
        Assert.Equal("low", updated.Priority);              // untouched
    }

    [Fact]
    public async Task UpdatingAnUnknownTaskReturns404()
    {
        var response = await _client.PutAsJsonAsync("/api/tasks/999999",
            new UpdateTaskRequest { Title = "ghost" });

        Assert.Equal(HttpStatusCode.NotFound, response.StatusCode);
    }

    [Fact]
    public async Task AnUpdateCannotBlankOutTheTitle()
    {
        var created = await CreateAsync("Keeps its title");

        var response = await _client.PutAsJsonAsync($"/api/tasks/{created.Id}",
            new UpdateTaskRequest { Title = "   " });

        Assert.Equal(HttpStatusCode.BadRequest, response.StatusCode);

        // And the stored task is untouched.
        var unchanged = await _client.GetFromJsonAsync<TaskResponse>($"/api/tasks/{created.Id}");
        Assert.Equal("Keeps its title", unchanged!.Title);
    }

    [Fact]
    public async Task AnUpdateWithAnUnknownPriorityIsRejected()
    {
        var created = await CreateAsync("Priority guard");

        var response = await _client.PutAsJsonAsync($"/api/tasks/{created.Id}",
            new UpdateTaskRequest { Priority = "critical" });

        Assert.Equal(HttpStatusCode.BadRequest, response.StatusCode);
    }

    // ---- delete ----------------------------------------------------------

    [Fact]
    public async Task ATaskCanBeDeletedAndIsThenGone()
    {
        var created = await CreateAsync("Delete me");

        var deleted = await _client.DeleteAsync($"/api/tasks/{created.Id}");
        Assert.Equal(HttpStatusCode.OK, deleted.StatusCode);
        var message = await deleted.Content.ReadFromJsonAsync<MessageResponse>();
        Assert.Equal("Task deleted", message!.Message);

        var fetch = await _client.GetAsync($"/api/tasks/{created.Id}");
        Assert.Equal(HttpStatusCode.NotFound, fetch.StatusCode);
    }

    [Fact]
    public async Task DeletingAnUnknownTaskReturns404()
    {
        var response = await _client.DeleteAsync("/api/tasks/999999");
        Assert.Equal(HttpStatusCode.NotFound, response.StatusCode);
    }

    // ---- stats -----------------------------------------------------------

    [Fact]
    public async Task StatsCountTotalCompletedAndPendingConsistently()
    {
        var stats = await _client.GetFromJsonAsync<StatsResponse>("/api/stats");

        Assert.NotNull(stats);
        Assert.Equal(stats!.Total, stats.Completed + stats.Pending);

        var all = await _client.GetFromJsonAsync<List<TaskResponse>>("/api/tasks");
        Assert.Equal(all!.Count, stats.Total);
        Assert.Equal(all.Count(t => t.Completed), stats.Completed);
    }

    // ---- injection -------------------------------------------------------

    [Fact]
    public async Task AtitleContainingSqlIsStoredAsTextNotExecuted()
    {
        // The classic payload. EF Core parameterizes, so this is just a string.
        const string payload = "Robert'); DROP TABLE tasks;--";

        var created = await CreateAsync(payload);
        Assert.Equal(payload, created.Title);

        // If the table had been dropped, this request would fail outright.
        var all = await _client.GetFromJsonAsync<List<TaskResponse>>("/api/tasks");
        Assert.NotNull(all);
        Assert.Contains(all!, t => t.Title == payload);
    }

    [Fact]
    public async Task AQuoteHeavyTitleSurvivesARoundTrip()
    {
        const string payload = "He said \"hi\" -- it's 100% fine; SELECT * FROM tasks";

        var created = await CreateAsync(payload);
        var fetched = await _client.GetFromJsonAsync<TaskResponse>($"/api/tasks/{created.Id}");

        Assert.Equal(payload, fetched!.Title);
    }

    // ---- malformed input -------------------------------------------------

    [Fact]
    public async Task AMalformedJsonBodyIsRejectedRatherThanCrashing()
    {
        var content = new StringContent("{ not json", Encoding.UTF8, "application/json");
        var response = await _client.PostAsync("/api/tasks", content);

        Assert.True(response.StatusCode is HttpStatusCode.BadRequest
                        or HttpStatusCode.UnsupportedMediaType,
            $"expected a 4xx for malformed JSON, got {(int)response.StatusCode}");
    }
}

/// <summary>Checks the persistence layer directly, without the HTTP hop.</summary>
public sealed class PersistenceTests : IClassFixture<ApiFactory>
{
    private readonly ApiFactory _factory;

    public PersistenceTests(ApiFactory factory) => _factory = factory;

    [Fact]
    public async Task TasksSurviveInTheDatabaseAcrossScopes()
    {
        int id;
        using (var scope = _factory.Services.CreateScope())
        {
            var db = scope.ServiceProvider.GetRequiredService<TaskDbContext>();
            var task = new TaskItem
            {
                Title = "Persisted",
                Description = "",
                Priority = Priorities.High,
                CreatedAt = DateTime.UtcNow.ToString("o"),
            };
            db.Tasks.Add(task);
            await db.SaveChangesAsync();
            id = task.Id;
        }

        using (var scope = _factory.Services.CreateScope())
        {
            var db = scope.ServiceProvider.GetRequiredService<TaskDbContext>();
            var loaded = await db.Tasks.FindAsync(id);
            Assert.NotNull(loaded);
            Assert.Equal("Persisted", loaded!.Title);
            Assert.Equal(Priorities.High, loaded.Priority);
        }
    }
}
