# 📚 Smart Study Planner — C++17 + Built-in Web Server

![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=c%2B%2B&logoColor=white)
![Winsock](https://img.shields.io/badge/Winsock-TCP-0078D4?style=flat-square&logo=windows&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.16%2B-064F8C?style=flat-square&logo=cmake&logoColor=white)
![CI](https://github.com/bodysale5678az-svg/SmartStudyPlanner/actions/workflows/ci.yml/badge.svg)

A study-task manager written in **modern C++17** that ships its **own HTTP server** — no ASP.NET, no Node, no Python. Open `http://localhost:3000` and you get a full Arabic RTL web interface talking to the C++ process over a JSON API.

---

## 📸 Screenshot

![Smart Study Planner](docs/screenshot.jpeg)

---

## ✨ What makes it interesting

| Layer | Implementation |
|---|---|
| **Task storage** | Hand-written **linked list** (`TaskNode`), not `std::list` |
| **Priority ordering** | `std::priority_queue` with a custom `TaskCompare` functor |
| **Daily plan** | `std::deque` capped at 240 working minutes |
| **HTTP server** | Raw **Winsock TCP** sockets, one thread per client |
| **Web UI** | Embedded HTML + Bootstrap RTL + vanilla JS |
| **API** | `GET /api/tasks`, `POST /api/tasks`, `GET /health` |
| **Persistence** | JSON-like flat file (`study_planner.json`) |
| **Scoring** | `urgencyScore() = f(daysLeft, priority)` |

---

## 🧠 The urgency model

Every task scores itself so the planner can rank work objectively:

```cpp
double urgencyScore() const {
    if (!active || completed) return -1.0;

    double daysLeft = difftime(deadline, time(nullptr)) / (24 * 60 * 60.0);
    return std::max(0.0, (10 - daysLeft) * 0.5 + priority * 0.5);
}
```

- Closer deadline → higher score
- Higher priority → higher score
- Completed / inactive tasks are pushed to the bottom (−1.0)

The priority queue is rebuilt whenever the task list changes, so the ordering is always consistent with the stored data.

---

## 🏗️ Architecture

```
src/main.cpp
├── Task                  — entity, ID generation, date parsing, urgency
├── TaskCompare           — priority_queue comparator
├── TaskNode              — linked-list node
├── SmartStudyPlanner     — domain logic + persistence
│   ├── addTaskFromWeb()
│   ├── rebuildPriorityQueue()
│   ├── rebuildExecutionQueue()
│   └── saveData() / loadData()
└── WebServer             — Winsock HTTP server
    ├── handleRequest()   — routing
    ├── sendHTML()
    ├── sendTasksJSON()
    └── parsePOSTData()
```

---

## 🚀 Build & run

### Requirements
- A C++17 compiler (GCC, Clang or MSVC)
- **Windows** (the HTTP server uses Winsock)

### With g++ (MinGW)
```bash
g++ -std=c++17 -O2 -static src/main.cpp -o planner.exe -lws2_32
./planner.exe
```

### With CMake
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/SmartStudyPlanner
```

### With MSVC
```bash
cl /std:c++17 /EHsc src\main.cpp /Fe:planner.exe
```

Then open **http://localhost:3000**

---

## 🔌 API

| Method | Path | Description |
|---|---|---|
| `GET` | `/` | Web interface |
| `GET` | `/api/tasks` | All active tasks as JSON |
| `POST` | `/api/tasks` | Add a task (JSON body) |
| `GET` | `/health` | Liveness probe (`{"status":"ok"}`) |

### Example

```bash
curl -X POST http://localhost:3000/api/tasks \
  -H "Content-Type: application/json" \
  -d '{"name":"Math Revision","subject":"Mathematics","estimated_time":90,"deadline":"2026-11-20","priority":8}'
```

---

## 🐛 Bugs fixed while bringing this to production

The original draft worked in the browser but had several issues that made it
fragile. All were found by running requests through it and reading the raw bytes:

| Issue | Root cause | Fix |
|---|---|---|
| `/api/tasks` returned the HTML page | Routing matched `"GET /"` first, which every path satisfies | Parse the request line into method + path, then match exactly |
| `POST` silently did nothing | The server looked only for `\r\n\r\n`, but some clients send `\n\n` | Accept both separators |
| `POST` crashed on `std::stoi` | `"estimated_time":` is 17 chars, the parser skipped only 16 | Corrected offsets and switched to non-throwing `atoi` |
| Body sometimes incomplete | A single `recv()` was assumed to carry the whole request | Read until `Content-Length` is satisfied |
| Server exited with `0xC0000135` | MinGW runtime DLLs were missing | Added `-static` build flags |
| Page scrolled off-screen on RTL | Missing `overflow-x` / `direction` rules | Added `html/body` reset |
| No way to verify liveness | — | Added `/health` endpoint used by CI |

---

## 📄 License

MIT

---

## 👤 Author

**Mahmoud Saleh** — [@bodysale5678az-svg](https://github.com/bodysale5678az-svg)
