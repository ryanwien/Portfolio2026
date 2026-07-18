using Microsoft.EntityFrameworkCore;

namespace TaskTracker.Api;

/// <summary>
/// EF Core context over the same tasks table the Flask backend uses.
///
/// EF Core parameterizes every query it generates, so user input can never be
/// concatenated into SQL — the same protection the Flask version gets by
/// passing tuples to sqlite3 rather than formatting strings.
/// </summary>
public sealed class TaskDbContext(DbContextOptions<TaskDbContext> options) : DbContext(options)
{
    public DbSet<TaskItem> Tasks => Set<TaskItem>();

    protected override void OnModelCreating(ModelBuilder builder)
    {
        var task = builder.Entity<TaskItem>();
        task.ToTable("tasks");
        task.HasKey(t => t.Id);

        task.Property(t => t.Id).HasColumnName("id").ValueGeneratedOnAdd();
        task.Property(t => t.Title).HasColumnName("title").IsRequired();
        task.Property(t => t.Description).HasColumnName("description");
        task.Property(t => t.Priority).HasColumnName("priority").HasDefaultValue(Priorities.Medium);
        task.Property(t => t.Completed).HasColumnName("completed");
        task.Property(t => t.CreatedAt).HasColumnName("created_at").IsRequired();
    }
}
