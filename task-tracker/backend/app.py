"""
app.py — Task Tracker REST API

A small full-stack task management application backend.
Built with Flask and SQLite. Exposes a REST API for creating,
reading, updating, and deleting tasks (full CRUD).

Run:
    pip install -r requirements.txt
    python app.py

The server starts on http://localhost:5000
"""

from flask import Flask, request, jsonify
from flask_cors import CORS
import sqlite3
import os
from datetime import datetime

app = Flask(__name__)
CORS(app)  # Allow the frontend (served separately) to call this API

DB_PATH = os.path.join(os.path.dirname(__file__), "tasks.db")


# ---------------------------------------------------------------------------
# Database setup
# ---------------------------------------------------------------------------
def get_db_connection():
    """Open a connection to the SQLite database.

    sqlite3.Row lets us access columns by name (row['title'])
    instead of by index (row[1]), which keeps the code readable.
    """
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def init_db():
    """Create the tasks table if it does not already exist.

    Called once on startup. Safe to run repeatedly because of
    'IF NOT EXISTS'.
    """
    conn = get_db_connection()
    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS tasks (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            title       TEXT NOT NULL,
            description TEXT,
            priority    TEXT DEFAULT 'medium',
            completed   INTEGER DEFAULT 0,
            created_at  TEXT NOT NULL
        )
        """
    )
    conn.commit()
    conn.close()


def task_to_dict(row):
    """Convert a database row into a JSON-serializable dictionary."""
    return {
        "id": row["id"],
        "title": row["title"],
        "description": row["description"],
        "priority": row["priority"],
        "completed": bool(row["completed"]),
        "created_at": row["created_at"],
    }


# ---------------------------------------------------------------------------
# Routes (the REST API)
# ---------------------------------------------------------------------------
@app.route("/api/tasks", methods=["GET"])
def get_tasks():
    """Return all tasks, newest first.

    Supports an optional ?completed=true|false query parameter
    to filter the list.
    """
    completed_filter = request.args.get("completed")

    conn = get_db_connection()
    if completed_filter == "true":
        rows = conn.execute(
            "SELECT * FROM tasks WHERE completed = 1 ORDER BY created_at DESC"
        ).fetchall()
    elif completed_filter == "false":
        rows = conn.execute(
            "SELECT * FROM tasks WHERE completed = 0 ORDER BY created_at DESC"
        ).fetchall()
    else:
        rows = conn.execute(
            "SELECT * FROM tasks ORDER BY created_at DESC"
        ).fetchall()
    conn.close()

    return jsonify([task_to_dict(row) for row in rows])


@app.route("/api/tasks/<int:task_id>", methods=["GET"])
def get_task(task_id):
    """Return a single task by its id, or 404 if it does not exist."""
    conn = get_db_connection()
    row = conn.execute(
        "SELECT * FROM tasks WHERE id = ?", (task_id,)
    ).fetchone()
    conn.close()

    if row is None:
        return jsonify({"error": "Task not found"}), 404
    return jsonify(task_to_dict(row))


@app.route("/api/tasks", methods=["POST"])
def create_task():
    """Create a new task from JSON in the request body.

    Required field: title
    Optional fields: description, priority
    """
    data = request.get_json()

    # Basic input validation — title is required.
    if not data or not data.get("title"):
        return jsonify({"error": "Title is required"}), 400

    title = data["title"].strip()
    description = data.get("description", "").strip()
    priority = data.get("priority", "medium")

    if priority not in ("low", "medium", "high"):
        return jsonify({"error": "Priority must be low, medium, or high"}), 400

    created_at = datetime.utcnow().isoformat()

    conn = get_db_connection()
    cursor = conn.execute(
        """
        INSERT INTO tasks (title, description, priority, completed, created_at)
        VALUES (?, ?, ?, 0, ?)
        """,
        (title, description, priority, created_at),
    )
    conn.commit()
    new_id = cursor.lastrowid

    row = conn.execute(
        "SELECT * FROM tasks WHERE id = ?", (new_id,)
    ).fetchone()
    conn.close()

    return jsonify(task_to_dict(row)), 201


@app.route("/api/tasks/<int:task_id>", methods=["PUT"])
def update_task(task_id):
    """Update an existing task.

    Any subset of fields can be sent; only the provided ones change.
    """
    data = request.get_json()
    if not data:
        return jsonify({"error": "No data provided"}), 400

    conn = get_db_connection()
    existing = conn.execute(
        "SELECT * FROM tasks WHERE id = ?", (task_id,)
    ).fetchone()

    if existing is None:
        conn.close()
        return jsonify({"error": "Task not found"}), 404

    # Use the new value if provided, otherwise keep the existing one.
    title = data.get("title", existing["title"])
    description = data.get("description", existing["description"])
    priority = data.get("priority", existing["priority"])
    completed = data.get("completed", bool(existing["completed"]))

    if priority not in ("low", "medium", "high"):
        conn.close()
        return jsonify({"error": "Priority must be low, medium, or high"}), 400

    conn.execute(
        """
        UPDATE tasks
        SET title = ?, description = ?, priority = ?, completed = ?
        WHERE id = ?
        """,
        (title, description, priority, 1 if completed else 0, task_id),
    )
    conn.commit()

    row = conn.execute(
        "SELECT * FROM tasks WHERE id = ?", (task_id,)
    ).fetchone()
    conn.close()

    return jsonify(task_to_dict(row))


@app.route("/api/tasks/<int:task_id>", methods=["DELETE"])
def delete_task(task_id):
    """Delete a task by id. Returns 404 if it does not exist."""
    conn = get_db_connection()
    existing = conn.execute(
        "SELECT * FROM tasks WHERE id = ?", (task_id,)
    ).fetchone()

    if existing is None:
        conn.close()
        return jsonify({"error": "Task not found"}), 404

    conn.execute("DELETE FROM tasks WHERE id = ?", (task_id,))
    conn.commit()
    conn.close()

    return jsonify({"message": "Task deleted"}), 200


@app.route("/api/stats", methods=["GET"])
def get_stats():
    """Return simple summary statistics about the tasks.

    Demonstrates a small bit of server-side aggregation rather
    than making the frontend compute everything.
    """
    conn = get_db_connection()
    total = conn.execute("SELECT COUNT(*) FROM tasks").fetchone()[0]
    completed = conn.execute(
        "SELECT COUNT(*) FROM tasks WHERE completed = 1"
    ).fetchone()[0]
    conn.close()

    return jsonify({
        "total": total,
        "completed": completed,
        "pending": total - completed,
    })


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    init_db()
    app.run(debug=True, port=5000)
