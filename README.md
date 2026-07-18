# Hi, I'm Ryan 👋

Software engineer with a background in tools development and technical
problem-solving. I build full-stack applications, automation tools, and
data-driven programs, and I care about writing clean, well-documented code.

🌐 **[Live portfolio & demos → ryanwien.github.io/Portfolio2026](https://ryanwien.github.io/Portfolio2026/)** — every project below runs live in your browser, with no backend.

🔭 **Currently:** working through the **Microsoft Cybersecurity Analyst**
certificate, and building out the projects below.

🌱 **Recently learned:** full-stack web development (Microsoft) and supervised
machine learning (DeepLearning.AI).

## 🛠️ Tech I work with

**Languages:** Python · C++ · C# · JavaScript · SQL · HTML/CSS
**Backend:** ASP.NET Core · Flask · EF Core · SQLite · REST APIs
**Data & ML:** scikit-learn · pandas · NumPy
**Tooling:** Git · CMake · MSVC · xUnit · GitHub Pages

## 📌 Projects

Each project has a **live browser demo** and a **written design rationale** —
the reasoning matters more than the line count.

| Project | What it is | Languages |
|---|---|---|
| **[Order Book Matching Engine](matching-engine/)** <br> [▶ live demo](https://ryanwien.github.io/Portfolio2026/matching-engine/orderbook_terminal.html) | A price-time-priority limit order book with limit/market orders, partial fills and O(1) cancels, wired to a live depth-ladder terminal and order-flow simulator. | C++20 · Python · JS |
| **[Security Log Analyzer](security-log-analyzer/)** <br> [▶ live demo](https://ryanwien.github.io/Portfolio2026/security-log-analyzer/demo.html) | A SIEM-style detection engine turning raw auth logs into ranked, MITRE ATT&CK-tagged alerts — brute force, password spraying, impossible travel, port scans. Runs on your own logs. | C# · Python · JS |
| **[Task Tracker](task-tracker/)** <br> [▶ live demo](https://ryanwien.github.io/Portfolio2026/task-tracker/demo.html) | A task management app with full CRUD, server-side validation and SQL-injection protection, behind a REST API. | C# · Python · JS |
| **[File Organizer](file-organizer/)** <br> [▶ live demo](https://ryanwien.github.io/Portfolio2026/file-organizer/demo.html) | Sorts a folder by file type with dry-run preview, collision-safe renaming and undo. The browser demo can organize a real folder on your machine. | C++20 · Python · JS |
| **[House Price Prediction](ml-housing/)** <br> [▶ live demo](https://ryanwien.github.io/Portfolio2026/ml-housing/demo.html) | Supervised regression predicting sale price from six features, R² = 0.94 on held-out data. The demo runs the real trained weights. | Python · JS |

## ✅ How this repo is verified

Several projects exist in more than one language. Rather than claim the
implementations agree, each pair is **tested against the other**, because a port
that silently diverges is worse than no port at all.

| Project | Equivalence check | Tests |
|---|---|---|
| Security Log Analyzer | Both engines read the same 221-event log; their reports are **byte-for-byte identical** | 30 |
| Order Book | Both replay a shared [`reference_scenario.txt`](matching-engine/reference_scenario.txt); every trade and the final book **match exactly** | 56 |
| File Organizer | Both run over identical folders and produce **identical directory trees**, collision renames included | 45 |
| Task Tracker | The C# API implements the Flask contract exactly — same routes, same JSON field names, same error strings | 22 |

Holding those equivalences honest surfaced details that are easy to get wrong:
Python dictionaries iterate in insertion order and the detection rules depend on
it; `"{:.0f}"` rounds half to **even**, not half away from zero; and
`directory_iterator` returns entries in an unspecified order, so the C++
organizer sorts before planning.

It also surfaced real defects — an unhandled exception when a file is locked by
another process, and a latency benchmark that was measuring the clock rather
than the code. Both are written up in the project READMEs.

## 🚀 Running things locally

Every demo works with no setup — just open the live links above. To run the
sources:

```bash
# Python
pip install -r <project>/requirements.txt
python security-log-analyzer/analyze.py

# C# — .NET 9 SDK
dotnet run  --project security-log-analyzer/csharp/src/SecurityLogAnalyzer
dotnet test security-log-analyzer/csharp/tests/SecurityLogAnalyzer.Tests

# C++ — C++20 compiler
cd matching-engine/cpp && cmake -B build && cmake --build build
./build/engine_tests && ./build/benchmark
```

The C++ projects ship a `CMakeLists.txt` as the portable entry point and a
`build.bat` for MSVC. Local verification was done with MSVC on Windows.

## 📫 Reach me

- Email: [ryanwien3d@gmail.com](mailto:ryanwien3d@gmail.com)
- LinkedIn: [linkedin.com/in/ryanwien3d](https://www.linkedin.com/in/ryanwien3d)
