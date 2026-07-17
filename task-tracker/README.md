# Task Tracker

A full-stack task management web application with a REST API backend and a
vanilla-JavaScript frontend. Built to practice clean API design, database
operations, and frontend–backend integration.

**▶ [Live demo](https://ryanwien.github.io/Portfolio2026/task-tracker/demo.html)** — a self-contained, in-browser walkthrough of the UI (no backend required).

## Overview

Task Tracker lets a user create, view, update, complete, and delete tasks,
each with a title, description, and priority level. The app shows live summary
statistics and supports filtering by completion status.

It is intentionally built **without frontend frameworks** so that every part of
the data flow HTTP requests, JSON handling, and DOM updates is explicit and
easy to follow.

## Architecture

```
┌─────────────────┐        HTTP / JSON        ┌──────────────────┐
│   Frontend      │  ───────────────────────► │   Flask API      │
│  (HTML/CSS/JS)  │                            │   (Python)       │
│                 │  ◄───────────────────────  │                  │
└─────────────────┘                            └────────┬─────────┘
                                                        │ SQL
                                                ┌───────▼────────┐
                                                │  SQLite DB     │
                                                └────────────────┘
```

- **Frontend** — plain HTML, CSS, and JavaScript. Uses the `fetch` API to call
  the backend and updates the DOM with the results.
- **Backend** — a Flask REST API exposing CRUD endpoints plus a stats endpoint.
- **Database** — SQLite, accessed through parameterized queries to prevent
  SQL injection.

## REST API

| Method | Endpoint              | Description                          |
|--------|-----------------------|--------------------------------------|
| GET    | `/api/tasks`          | List all tasks (optional `?completed=true|false`) |
| GET    | `/api/tasks/<id>`     | Get a single task                    |
| POST   | `/api/tasks`          | Create a task                        |
| PUT    | `/api/tasks/<id>`     | Update a task (partial updates ok)   |
| DELETE | `/api/tasks/<id>`     | Delete a task                        |
| GET    | `/api/stats`          | Summary counts (total/pending/done)  |

### Example request

```bash
curl -X POST http://localhost:5000/api/tasks \
  -H "Content-Type: application/json" \
  -d '{"title": "Write README", "priority": "high"}'
```

## Running locally

### Backend

```bash
cd backend
pip install -r requirements.txt
python app.py
```

The API starts on `http://localhost:5000`.

### Frontend

Open `frontend/index.html` in a browser, or serve it with any static server:

```bash
cd frontend
python -m http.server 8000
```

Then visit `http://localhost:8000`.

## Design decisions

- **Parameterized SQL queries** everywhere (`?` placeholders) to prevent SQL
  injection — user input is never concatenated into query strings.
- **Server-side input validation** — the API rejects tasks without a title and
  validates the priority field, rather than trusting the client.
- **HTML escaping on the frontend** — user-entered text is escaped before being
  inserted into the DOM, a basic guard against XSS.
- **Separation of concerns** — database access, request handling, and rendering
  are kept in distinct functions so each piece can be understood and changed
  independently.
- **No frontend framework** — kept deliberately dependency-light so the core
  mechanics (fetch, JSON, DOM) are visible rather than hidden behind a library.

## Possible next steps

- User accounts and authentication
- Due dates and sorting
- Move from SQLite to PostgreSQL for multi-user deployment
- Automated tests (pytest for the API)
- Deploy to a cloud host

## Tech stack

Python · Flask · SQLite · JavaScript (ES6) · HTML5 · CSS3

## Live demo

`demo.html` is a standalone, no-setup version you can open directly in a
browser — useful for quickly seeing the interface without running the backend.
It keeps tasks in memory (data resets on refresh). The full application
(`backend/app.py` + `frontend/`) uses a real Flask REST API and SQLite database.
