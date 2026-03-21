# C++ Contributor Guide

This guide covers C++ development for Fincept Terminal — 40+ screens, core infrastructure, trading engine, and Qt6 UI.

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
fincept-qt/src/
├── app/
│   ├── main.cpp                    # Entry point, QApplication setup
│   ├── MainWindow.cpp/h            # Main window, layout, screen hosting
│   └── ScreenRouter.cpp/h          # QStackedWidget-based navigation
│
├── core/                           # Shared infrastructure
│   ├── config/AppConfig.cpp/h      # App-wide constants (URLs, versions)
│   ├── events/EventBus.cpp/h       # Pub/sub messaging
│   ├── logging/Logger.cpp/h        # Structured logging (LOG_INFO, LOG_ERROR)
│   ├── result/Result.h             # Result<T> error handling
│   └── session/SessionManager.cpp/h
│
├── ui/                             # Reusable Qt widgets (Obsidian design system)
│   ├── theme/                      # StyleSheets, color tokens, fonts
│   ├── widgets/                    # Card, SearchBar, StatusBadge, TabHeader, etc.
│   ├── tables/DataTable.cpp/h      # Reusable data table
│   ├── charts/ChartFactory.cpp/h   # Qt6 Charts factory
│   └── navigation/                 # NavigationBar, StatusBar, FKeyBar, ToolBar
│
├── network/
│   ├── http/HttpClient.cpp/h       # QNetworkAccessManager wrapper
│   └── websocket/WebSocketClient.cpp/h
│
├── storage/
│   ├── sqlite/                     # Database + migrations
│   ├── cache/                      # CacheManager, TabSessionStore
│   ├── secure/SecureStorage.cpp/h  # Encrypted credential storage
│   └── repositories/              # 13 data access objects
│
├── auth/                           # AuthManager, AuthApi, SessionGuard
├── python/PythonRunner.cpp/h       # Execute Python scripts
├── trading/                        # Trading engine + 20+ broker adapters
├── services/                       # MarketDataService, NewsService
│
└── screens/                        # Terminal screens
    ├── dashboard/                  # Main dashboard + 13 widgets
    ├── markets/
    ├── news/
    ├── watchlist/
    ├── crypto_trading/
    ├── report_builder/
    ├── auth/                       # Login, Register, ForgotPassword, Pricing
    ├── profile/
    ├── settings/
    ├── support/
    ├── about/
    └── ComingSoonScreen.cpp/h      # Placeholder for upcoming screens
```

---

## Key Rules

### 1. Separation: Screens vs Services
- **Screens** (`*Screen.cpp`) render UI only — no HTTP calls, no business logic
- **Services** (`*Service.cpp`) handle fetching, caching, processing
- Screens connect to services via Qt signals/slots, never call `HttpClient` directly

### 2. Use Core Infrastructure
- `Result<T>` for error handling instead of raw error codes
- `LOG_INFO("tag", "message")` / `LOG_ERROR("tag", "message")` for logging
- `EventBus::instance().publish("event.type", data)` for cross-module communication
- `AppConfig::instance().api_base_url()` for constants — no magic strings

### 3. Use UI Widgets
- `fincept::ui::Card` for panel containers
- `fincept::ui::DataTable` for tabular data
- `fincept::ui::SearchBar` for search inputs
- `fincept::ui::StatusBadge` for status indicators
- Apply stylesheets via `fincept::ui::apply_global_stylesheet()`
- Follow `DESIGN_SYSTEM.md` (Obsidian) — color tokens, spacing, typography

### 4. Namespace Convention
```cpp
namespace fincept {         // top-level
namespace fincept::ui {     // UI components
namespace fincept::auth {   // auth module
// etc.
}
```

### 5. Qt Signals & Slots
- Use the new pointer-to-member syntax: `connect(src, &Src::signal, dst, &Dst::slot)`
- Never use old string-based `SIGNAL()`/`SLOT()` macros
- All QObject subclasses must have `Q_OBJECT` in the class body

### 6. Threading
- UI code runs on the main thread only (Qt requirement)
- Background work via `QThread` or `QtConcurrent::run`
- Post results back to UI thread via signals across threads (Qt handles marshalling automatically)
- Protect shared non-Qt state with `QMutex`

---

## Adding a New Screen

1. Create folder: `src/screens/your_feature/`
2. Create files:
   - `YourScreen.h/.cpp` — subclass `QWidget`, add `Q_OBJECT`
   - `YourService.h/.cpp` — data fetching/processing (if needed)
   - `YourTypes.h` — shared data types (if needed)
3. Add `.cpp` files to `CMakeLists.txt` `SCREEN_SOURCES` list
4. Register the screen in `src/app/MainWindow.cpp` via `ScreenRouter`
5. Add navigation entry in `src/ui/navigation/NavigationBar.cpp`
6. Build and test

### Minimal Screen Template

```cpp
// YourScreen.h
#pragma once
#include <QWidget>

namespace fincept {

class YourScreen : public QWidget {
    Q_OBJECT
public:
    explicit YourScreen(QWidget* parent = nullptr);
};

} // namespace fincept
```

```cpp
// YourScreen.cpp
#include "YourScreen.h"
#include "ui/theme/StyleSheets.h"
#include <QVBoxLayout>
#include <QLabel>

namespace fincept {

YourScreen::YourScreen(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Your Screen", this));
}

} // namespace fincept
```

---

## Code Style

- 4-space indentation, 120 column limit
- `snake_case` for functions/variables, `PascalCase` for classes/types
- Trailing underscore for member variables: `data_`, `loading_`
- No `using namespace std;` — use explicit `std::` prefix
- No `using namespace Qt` — use explicit `Qt::` prefix where needed

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
- [ ] New `.cpp` files added to `CMakeLists.txt`
- [ ] `Q_OBJECT` present in all QObject subclasses
- [ ] Signals/slots use pointer-to-member syntax
- [ ] Follows Obsidian design system (`DESIGN_SYSTEM.md`)

---

**Questions?** Open an issue on [GitHub](https://github.com/Fincept-Corporation/FinceptTerminal).
