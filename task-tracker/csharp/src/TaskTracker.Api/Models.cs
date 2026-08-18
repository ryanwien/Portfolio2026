using System.Text.Json.Serialization;

namespace TaskTracker.Api;

/// <summary>A task as it is stored. Column names match the Flask/SQLite schema.</summary>
public sealed class TaskItem
{
    public int Id { get; set; }
    public string Title { get; set; } = "";
    public string Description { get; set; } = "";
    public string Priority { get; set; } = Priorities.Medium;
    public bool Completed { get; set; }

    /// <summary>UTC ISO-8601, the same format the Flask API writes.</summary>
    public string CreatedAt { get; set; } = "";
}

public static class Priorities
{
    public const string Low = "low";
    public const string Medium = "medium";
    public const string High = "high";

    public static readonly string[] All = { Low, Medium, High };

    public static bool IsValid(string? value) => value is not null && All.Contains(value);
}

public static class Titles
{
    /// <summary>
    /// Matches <c>maxlength="120"</c> on the front end's title field. The
    /// browser attribute is a convenience, not a control — one devtools edit
    /// or one curl call and it is gone — so the server enforces it too.
    /// </summary>
    public const int MaxLength = 120;

    public const string TooLong = "Title must be 120 characters or less";
}

/// <summary>
/// Wire shape of a task. Property names are snake_case to stay byte-compatible
/// with the Flask API, so the same front end can talk to either backend.
/// </summary>
public sealed record TaskResponse(
    [property: JsonPropertyName("id")] int Id,
    [property: JsonPropertyName("title")] string Title,
    [property: JsonPropertyName("description")] string Description,
    [property: JsonPropertyName("priority")] string Priority,
    [property: JsonPropertyName("completed")] bool Completed,
    [property: JsonPropertyName("created_at")] string CreatedAt)
{
    public static TaskResponse From(TaskItem task) =>
        new(task.Id, task.Title, task.Description, task.Priority, task.Completed, task.CreatedAt);
}

/// <summary>Body of POST /api/tasks.</summary>
public sealed class CreateTaskRequest
{
    [JsonPropertyName("title")] public string? Title { get; set; }
    [JsonPropertyName("description")] public string? Description { get; set; }
    [JsonPropertyName("priority")] public string? Priority { get; set; }
}

/// <summary>
/// Body of PUT /api/tasks/{id}. Every field is optional: a null means "leave
/// this one alone", which is what makes the update a partial one.
/// </summary>
public sealed class UpdateTaskRequest
{
    [JsonPropertyName("title")] public string? Title { get; set; }
    [JsonPropertyName("description")] public string? Description { get; set; }
    [JsonPropertyName("priority")] public string? Priority { get; set; }
    [JsonPropertyName("completed")] public bool? Completed { get; set; }
}

public sealed record StatsResponse(
    [property: JsonPropertyName("total")] int Total,
    [property: JsonPropertyName("completed")] int Completed,
    [property: JsonPropertyName("pending")] int Pending);

public sealed record ErrorResponse([property: JsonPropertyName("error")] string Error);

public sealed record MessageResponse([property: JsonPropertyName("message")] string Message);
