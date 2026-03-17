# C++ Contributor Guide

This guide covers C++ development for Fincept Terminal — 40+ screens, core infrastructure, trading engine, and ImGui UI.

> **Prerequisites**: Read the [Contributing Guide](./CONTRIBUTING.md) first for setup and workflow.

---

## Overview

The C++ codebase handles:
- **40+ Screens** — Financial tools, analytics, trading interfaces
- **Core Infrastructure** — HTTP, database, logging, event bus
- **Trading Engine** — Broker integrations, order matching, WebSocket streams
- **Python Bridge** — Execute 100+ analytics scripts
- **MCP Integration** — Model Context Protocol for AI tools

**Related Guides:**
- [Python Guide](./PYTHON_CONTRIBUTOR_GUIDE.md) — Analytics scripts executed by C++

---

## Project Structure

```
fincept-cpp/src/
├── main.cpp                    # Entry point, GLFW/OpenGL setup
├── app.cpp/h                   # App state machine, routing, tab bar
│
├── core/                       # Shared infrastructure
│   ├── config.h                # App-wide constants
│   ├── event_bus.h             # Pub/sub messaging
│   ├── logger.h                # Structured logging
│   ├── result.h                # Result<T> error handling
│   ├── notification.h/cpp      # Desktop notifications
│   └── raii.h                  # RAII resource wrappers
│
├── ui/                         # Reusable ImGui widgets
│   └── widgets/                # Card, Table, Modal, SearchBar, etc.
│
├── http/                       # HTTP client (libcurl wrapper)
├── storage/                    # SQLite database
├── auth/                       # Authentication (JWT, guest mode)
├── python/                     # Python runtime bridge
├── mcp/                        # Model Context Protocol
├── trading/                    # Trading engine + 15+ brokers
├── portfolio/                  # Portfolio management
│
└── screens/                    # 40+ terminal screens
    ├── dashboard/              # Main dashboard + widgets
    ├── markets/                # Market data
    ├── crypto_trading/         # Crypto trading
    ├── equity_trading/         # Equity trading
    ├── algo_trading/           # Algorithmic trading
    ├── research/               # Equity research panels
    ├── quantlib/               # 18 quant modules
    ├── ai_chat/                # AI chat
    ├── economics/              # Economic data
    ├── geopolitics/            # Geopolitical analysis
    └── ...                     # 30+ more screens
```

---

## Key Rules

### 1. Separation: Screens vs Services
- **Screens** (`*_screen.cpp`) render UI only — no HTTP calls, no business logic
- **Data/Service** classes (`*_data.cpp`) handle fetching, caching, processing
- Screens call data services, never `HttpClient` directly

### 2. Use Core Infrastructure
- `Result<T>` for error handling instead of raw error codes
- `LOG_INFO("tag", "message %s", arg)` for logging
- `EventBus::instance().publish("event.type", data)` for cross-module communication
- `config::API_BASE_URL` for constants — no magic strings

### 3. Use UI Widgets
- `ui::BeginCard()` / `ui::EndCard()` for panels
- `ui::BeginDataTable()` / `ui::EndDataTable()` for tables
- `ui::MetricCard()` for value displays
- `theme::AccentButton()` for primary buttons

### 4. Namespace Convention
```cpp
namespace fincept::module_name {
    // All code in a module namespace
}
```

### 5. Threading
- UI code runs on main thread only (ImGui requirement)
- Background work via `std::async` / `std::thread`
- Protect shared state with `std::mutex`
- Use `std::atomic` for simple flags

---

## Adding a New Screen

1. Create folder: `src/screens/your_feature/`
2. Create files:
   - `your_screen.h/.cpp` — UI rendering
   - `your_data.h/.cpp` — data fetching/processing (if needed)
   - `your_types.h` — data types (if needed)
3. Add `.cpp` files to `CMakeLists.txt` `SOURCES` list
4. Add screen instance in `app.cpp`
5. Add tab entry in `render_tab_bar()`
6. Add navigation in `render_top_bar()` Navigate menu

---

## Code Style

- `.clang-format` enforces style — run `clang-format -i src/**/*.cpp src/**/*.h`
- 4-space indentation, 120 column limit
- `snake_case` for functions/variables, `PascalCase` for types/classes
- Trailing underscore for member variables: `data_`, `loading_`
- No `using namespace std;` — use explicit `std::` prefix

---

## Commit Messages

```
feat: add crypto trading screen
fix: resolve WebSocket reconnection crash
refactor: extract market data service from dashboard
docs: update CONTRIBUTING.md with new architecture
perf: optimize order book rendering
```

---

## Pull Request Checklist

- [ ] Code compiles without warnings (`-Wall -Wextra`)
- [ ] No duplicated code — use `core/` and `ui/widgets/`
- [ ] Screens don't call HTTP directly
- [ ] New files added to `CMakeLists.txt`
- [ ] `.clang-format` applied

---

**Questions?** Open an issue on [GitHub](https://github.com/Fincept-Corporation/FinceptTerminal).
