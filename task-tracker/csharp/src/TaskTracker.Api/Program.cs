using Microsoft.EntityFrameworkCore;
using TaskTracker.Api;

// ---------------------------------------------------------------------------
// Task Tracker REST API — ASP.NET Core + EF Core + SQLite.
//
// Same contract as the Flask backend in ../backend/app.py, down to the JSON
// field names and error strings, so the existing front end can point at either.
// ---------------------------------------------------------------------------

var builder = WebApplication.CreateBuilder(args);

var databasePath = builder.Configuration["Database:Path"]
                   ?? Path.Combine(AppContext.BaseDirectory, "tasks.db");

builder.Services.AddDbContext<TaskDbContext>(options =>
    options.UseSqlite($"Data Source={databasePath}"));

// The front end is served separately, so it needs to be allowed to call this.
builder.Services.AddCors(options => options.AddDefaultPolicy(policy =>
    policy.AllowAnyOrigin().AllowAnyHeader().AllowAnyMethod()));

var app = builder.Build();
app.UseCors();

// Create the table on first run. Safe to repeat.
using (var scope = app.Services.CreateScope())
{
    scope.ServiceProvider.GetRequiredService<TaskDbContext>().Database.EnsureCreated();
}

var tasks = app.MapGroup("/api/tasks");

// GET /api/tasks[?completed=true|false] — newest first.
tasks.MapGet("/", async (TaskDbContext db, string? completed) =>
{
    IQueryable<TaskItem> query = db.Tasks;

    query = completed switch
    {
        "true" => query.Where(t => t.Completed),
        "false" => query.Where(t => !t.Completed),
        _ => query,
    };

    // Id breaks ties: two tasks created in the same instant would otherwise
    // come back in an order SQLite doesn't guarantee between runs.
    var results = await query
        .OrderByDescending(t => t.CreatedAt)
        .ThenByDescending(t => t.Id)
        .Select(t => TaskResponse.From(t))
        .ToListAsync();

    return Results.Ok(results);
});

// GET /api/tasks/{id}
tasks.MapGet("/{id:int}", async (TaskDbContext db, int id) =>
{
    var task = await db.Tasks.FindAsync(id);
    return task is null
        ? Results.NotFound(new ErrorResponse("Task not found"))
        : Results.Ok(TaskResponse.From(task));
});

// POST /api/tasks
tasks.MapPost("/", async (TaskDbContext db, CreateTaskRequest? request) =>
{
    if (request is null || string.IsNullOrWhiteSpace(request.Title))
        return Results.BadRequest(new ErrorResponse("Title is required"));

    var priority = request.Priority ?? Priorities.Medium;
    if (!Priorities.IsValid(priority))
        return Results.BadRequest(new ErrorResponse("Priority must be low, medium, or high"));

    var task = new TaskItem
    {
        Title = request.Title.Trim(),
        Description = (request.Description ?? "").Trim(),
        Priority = priority,
        Completed = false,
        CreatedAt = DateTime.UtcNow.ToString("o"),
    };

    db.Tasks.Add(task);
    await db.SaveChangesAsync();

    return Results.Created($"/api/tasks/{task.Id}", TaskResponse.From(task));
});

// PUT /api/tasks/{id} — partial update; omitted fields keep their value.
tasks.MapPut("/{id:int}", async (TaskDbContext db, int id, UpdateTaskRequest? request) =>
{
    if (request is null) return Results.BadRequest(new ErrorResponse("No data provided"));

    var task = await db.Tasks.FindAsync(id);
    if (task is null) return Results.NotFound(new ErrorResponse("Task not found"));

    var priority = request.Priority ?? task.Priority;
    if (!Priorities.IsValid(priority))
        return Results.BadRequest(new ErrorResponse("Priority must be low, medium, or high"));

    if (request.Title is not null)
    {
        if (string.IsNullOrWhiteSpace(request.Title))
            return Results.BadRequest(new ErrorResponse("Title is required"));
        task.Title = request.Title.Trim();
    }

    if (request.Description is not null) task.Description = request.Description.Trim();
    task.Priority = priority;
    if (request.Completed is not null) task.Completed = request.Completed.Value;

    await db.SaveChangesAsync();
    return Results.Ok(TaskResponse.From(task));
});

// DELETE /api/tasks/{id}
tasks.MapDelete("/{id:int}", async (TaskDbContext db, int id) =>
{
    var task = await db.Tasks.FindAsync(id);
    if (task is null) return Results.NotFound(new ErrorResponse("Task not found"));

    db.Tasks.Remove(task);
    await db.SaveChangesAsync();
    return Results.Ok(new MessageResponse("Task deleted"));
});

// GET /api/stats — aggregation done in SQL rather than in the browser.
app.MapGet("/api/stats", async (TaskDbContext db) =>
{
    var total = await db.Tasks.CountAsync();
    var completed = await db.Tasks.CountAsync(t => t.Completed);
    return Results.Ok(new StatsResponse(total, completed, total - completed));
});

app.Run();

/// <summary>Exposed so the test host can boot this exact application.</summary>
public partial class Program;
