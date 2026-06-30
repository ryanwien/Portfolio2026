/*
 * app.js — Task Tracker frontend logic
 *
 * Talks to the Flask REST API and updates the page.
 * No frameworks — plain JavaScript (the fetch API + DOM methods)
 * to keep the project easy to read and explain.
 */

const API_URL = "http://localhost:5000/api";

// Track which filter is currently active ('all' | 'pending' | 'completed')
let currentFilter = "all";


// ---------------------------------------------------------------------------
// API helper functions — each wraps one REST call
// ---------------------------------------------------------------------------

async function fetchTasks() {
    // Build the query string based on the active filter.
    let url = `${API_URL}/tasks`;
    if (currentFilter === "pending") url += "?completed=false";
    if (currentFilter === "completed") url += "?completed=true";

    const response = await fetch(url);
    if (!response.ok) throw new Error("Failed to fetch tasks");
    return response.json();
}

async function fetchStats() {
    const response = await fetch(`${API_URL}/stats`);
    if (!response.ok) throw new Error("Failed to fetch stats");
    return response.json();
}

async function createTask(task) {
    const response = await fetch(`${API_URL}/tasks`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(task),
    });
    if (!response.ok) {
        const err = await response.json();
        throw new Error(err.error || "Failed to create task");
    }
    return response.json();
}

async function updateTask(id, updates) {
    const response = await fetch(`${API_URL}/tasks/${id}`, {
        method: "PUT",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(updates),
    });
    if (!response.ok) throw new Error("Failed to update task");
    return response.json();
}

async function deleteTask(id) {
    const response = await fetch(`${API_URL}/tasks/${id}`, {
        method: "DELETE",
    });
    if (!response.ok) throw new Error("Failed to delete task");
    return response.json();
}


// ---------------------------------------------------------------------------
// Rendering — turn data into HTML
// ---------------------------------------------------------------------------

function renderTasks(tasks) {
    const list = document.getElementById("task-list");
    list.innerHTML = "";

    if (tasks.length === 0) {
        list.innerHTML = '<p class="empty">No tasks yet. Add one above!</p>';
        return;
    }

    tasks.forEach((task) => {
        const item = document.createElement("div");
        item.className = `task-item priority-${task.priority}` +
                         (task.completed ? " completed" : "");

        item.innerHTML = `
            <input type="checkbox" class="task-check" ${task.completed ? "checked" : ""}>
            <div class="task-content">
                <span class="task-title">${escapeHtml(task.title)}</span>
                ${task.description
                    ? `<span class="task-desc">${escapeHtml(task.description)}</span>`
                    : ""}
            </div>
            <span class="task-priority">${task.priority}</span>
            <button class="delete-btn">&times;</button>
        `;

        // Toggle completion when the checkbox changes.
        item.querySelector(".task-check").addEventListener("change", async (e) => {
            await updateTask(task.id, { completed: e.target.checked });
            await refresh();
        });

        // Delete when the × button is clicked.
        item.querySelector(".delete-btn").addEventListener("click", async () => {
            await deleteTask(task.id);
            await refresh();
        });

        list.appendChild(item);
    });
}

function renderStats(stats) {
    document.getElementById("stat-total").textContent = stats.total;
    document.getElementById("stat-pending").textContent = stats.pending;
    document.getElementById("stat-completed").textContent = stats.completed;
}

/* Escape user text so it can't inject HTML (basic XSS protection). */
function escapeHtml(text) {
    const div = document.createElement("div");
    div.textContent = text;
    return div.innerHTML;
}


// ---------------------------------------------------------------------------
// Main refresh — re-fetch everything and re-render
// ---------------------------------------------------------------------------

async function refresh() {
    try {
        const [tasks, stats] = await Promise.all([fetchTasks(), fetchStats()]);
        renderTasks(tasks);
        renderStats(stats);
    } catch (err) {
        console.error(err);
        alert("Could not reach the server. Is the backend running?");
    }
}


// ---------------------------------------------------------------------------
// Event wiring — run once when the page loads
// ---------------------------------------------------------------------------

document.getElementById("add-btn").addEventListener("click", async () => {
    const titleInput = document.getElementById("task-title");
    const descInput = document.getElementById("task-description");
    const priorityInput = document.getElementById("task-priority");

    const title = titleInput.value.trim();
    if (!title) {
        alert("Please enter a task title.");
        return;
    }

    try {
        await createTask({
            title: title,
            description: descInput.value.trim(),
            priority: priorityInput.value,
        });
        // Clear the form.
        titleInput.value = "";
        descInput.value = "";
        priorityInput.value = "medium";
        await refresh();
    } catch (err) {
        alert(err.message);
    }
});

// Let the Enter key submit the form from the title field.
document.getElementById("task-title").addEventListener("keypress", (e) => {
    if (e.key === "Enter") document.getElementById("add-btn").click();
});

// Filter buttons.
document.querySelectorAll(".filter-btn").forEach((btn) => {
    btn.addEventListener("click", () => {
        document.querySelectorAll(".filter-btn").forEach((b) =>
            b.classList.remove("active"));
        btn.classList.add("active");
        currentFilter = btn.dataset.filter;
        refresh();
    });
});

// Initial load.
refresh();
