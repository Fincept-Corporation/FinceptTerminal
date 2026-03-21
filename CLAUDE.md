# CLAUDE.md

This file provides guidance to Claude Code when working with the **Fincept Terminal** repository.

## Repository Overview

**Fincept Terminal** is a native C++20 desktop financial intelligence terminal. Version 4 is a complete rewrite from the previous Tauri/React/Rust stack into a pure C++ application using Qt6 and embedded Python for analytics.

**Current Version**: 4.0.0
**License**: AGPL-3.0-or-later
**Build System**: CMake 3.20+

## Quick Reference

### Primary Working Directory
**IMPORTANT**: The main application code is in `fincept-qt/`. All development happens there.

```
finceptTerminal/                    # Root repository
├── fincept-qt/                     # Main C++ application (PRIMARY)
│   ├── src/                        # C++ source code
│   ├── scripts/                    # Python analytics scripts
│   ├── resources/                  # Assets
│   ├── CMakeLists.txt              # Build configuration
│   └── DESIGN_SYSTEM.md           # Obsidian UI/UX specification
├── docs/                           # Repository-wide documentation
├── images/                         # Marketing/promotional images
└── README.md                       # Public-facing documentation
```

### Development Commands

From `fincept-qt/`:
```bash
# Build (Windows MSVC — Developer Command Prompt for VS 2022)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
cmake --build build --config Release

# Build (Linux / macOS)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run
./build/FinceptTerminal              # Linux / macOS
.\build\Release\FinceptTerminal.exe  # Windows
```

## Technology Stack

- **Language**: C++20 (MSVC 2022 / GCC 12+ / Clang 15+)
- **UI**: Qt6 Widgets
- **Charts**: Qt6 Charts
- **Networking**: Qt6 Network + Qt6 WebSockets
- **Database**: Qt6 Sql (SQLite)
- **Serialization**: QJsonDocument
- **Logging**: Custom Logger (QFile-based)
- **Python**: Embedded runtime for 100+ analytics scripts
- **Build**: CMake 3.20+

## Core Systems

1. **Authentication**: Guest + registered users, JWT-based
2. **Payment Integration**: Subscription management (Razorpay/Stripe)
3. **Trading**: Crypto (Kraken/HyperLiquid WebSocket), Equity, Algo trading, Paper trading engine
4. **Data**: 100+ data source connectors, Python data scripts, WebSocket real-time feeds
5. **Python Analytics**: CFA-level modules (equity, portfolio, derivatives, economics, quant, fixed income, corporate finance)
6. **AI**: Multiple agent frameworks (Geopolitics, Economic, Hedge Fund, Trader/Investor, Deep Agents)
7. **QuantLib Suite**: 18 quantitative analysis tabs
8. **AI Quant Lab**: ML models, factor discovery, HFT, RL trading
9. **Workflow System**: Visual node editor
10. **Geopolitics**: HDX, relationship mapping, geopolitical analysis

## Terminal Screens (40+)

Key screen groups:
- **Core**: Dashboard, Markets, News, Watchlist
- **Trading**: Crypto Trading, Equity Trading, Algo Trading, Backtesting, Trade Visualization
- **Research**: Equity Research, Screener, Portfolio, Surface Analytics, M&A Analytics, Derivatives, Alt Investments
- **QuantLib** (18): Core, Analysis, Curves, Economics, Instruments, ML, Models, Numerical, Physics, Portfolio, Pricing, Regulatory, Risk, Scheduling, Solver, Statistics, Stochastic, Volatility
- **AI/ML**: AI Quant Lab, Agent Studio, AI Chat, Alpha Arena
- **Economics**: Economics, DBnomics, AkShare Data, Asia Markets
- **Geopolitics**: Geopolitics, Gov Data, Relationship Map, Maritime, Polymarket
- **Tools**: Code Editor, Node Editor, Excel, Report Builder, Notes, Data Sources, Data Mapping, MCP Servers
- **Community**: Forum, Profile, Settings, Support, Docs, About

Currently implemented screens: Dashboard, Markets, News, Watchlist, Crypto Trading, Report Builder, Notes, Auth (Login/Register/ForgotPassword/Pricing), Profile, Settings, Support, About. Remaining screens use `ComingSoonScreen` as placeholder.

## Working with the Codebase

### Before Starting Any Task
1. Navigate to `fincept-qt/`
2. Read files before modifying them
3. Follow existing patterns

### Key Directories
- C++ source: `fincept-qt/src/`
- Screens: `fincept-qt/src/screens/`
- Core infrastructure: `fincept-qt/src/core/`
- UI widgets: `fincept-qt/src/ui/`
- HTTP client: `fincept-qt/src/network/http/`
- Storage/DB: `fincept-qt/src/storage/`
- Authentication: `fincept-qt/src/auth/`
- Trading: `fincept-qt/src/trading/`
- Python bridge: `fincept-qt/src/python/`
- Python scripts: `fincept-qt/scripts/`
- Documentation: `docs/`

### Code Standards
- C++20, `snake_case` for functions/variables, `PascalCase` for types/classes
- Screens render UI only — no HTTP calls, no business logic
- Services handle fetching, caching, processing
- Screens connect to services via Qt signals/slots
- Use `Result<T>` for error handling, `LOG_INFO` / `LOG_ERROR` for logging
- Use `Q_OBJECT` in all QObject subclasses
- Use pointer-to-member signal/slot syntax
- Namespace convention: `namespace fincept {}`, `namespace fincept::ui {}`
- No `using namespace std;` — use explicit `std::` prefix
- Follow `fincept-qt/DESIGN_SYSTEM.md` (Obsidian design system) for all UI

## Performance & Architecture Rules

These rules are **mandatory** for all code changes. They exist because violations caused severe UI freezing and 25+ simultaneous Python process spawns at startup.

### P1. Never Block the Main/UI Thread

- **NEVER** call `waitForFinished()`, `waitForStarted()`, or `waitForReadyRead()` on the UI thread
- The only acceptable uses are: (a) one-time startup before `QApplication::exec()`, (b) shutdown teardown in destructors
- If you need synchronous results from a subprocess or network call, wrap in `QtConcurrent::run()` and post results back via `QMetaObject::invokeMethod(this, [...]() { ... }, Qt::QueuedConnection)`
- Qt docs explicitly state: "Calling these functions from the main thread may cause your user interface to freeze"

### P2. Lazy Screen Construction

- Any screen that fetches data, spawns processes, starts timers, or does network I/O **must** use `router_->register_factory()`, not `register_screen()`
- Only trivial screens (static UI, no data) may use eager `register_screen()`
- When a `ComingSoonScreen` placeholder is replaced with a real implementation, it **must** move to `register_factory()`
- This prevents constructors from spawning work for screens the user hasn't navigated to

### P3. Timers Must Respect Visibility

- Every screen/widget with a `QTimer` **must** override `showEvent()` and `hideEvent()` to start/stop timers
- **NEVER** call `timer->start()` in a constructor — only set the interval and connect the signal
- Let `showEvent()` start the timer, `hideEvent()` stop it
- Pattern:
```cpp
void MyScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    my_timer_->start();
}
void MyScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    my_timer_->stop();
}
```

### P4. Python Process Concurrency

- `PythonRunner` enforces a max of 3 concurrent Python processes (configurable via `set_max_concurrent()`)
- Requests beyond the limit are queued and drained as processes finish
- **NEVER** bypass `PythonRunner` by creating raw `QProcess` instances for Python scripts — always go through `PythonRunner::instance().run()`
- The only exception is `ExchangeService` which manages its own long-running WebSocket process

### P5. Batch Data Requests

- When multiple widgets need market data, prefer one batched `fetch_quotes()` call over many individual ones
- `MarketDataService` should be the single entry point for all market data — widgets must not call `PythonRunner` directly
- Future improvement: implement request coalescing in `MarketDataService` (collect symbols over a 100ms window, deduplicate, single Python call, fan out results)

### P6. Service Layer Separation

- **Screens render UI only** — no `PythonRunner` calls, no `QProcess`, no direct HTTP
- **Services** (`MarketDataService`, `NewsService`, `ExchangeService`) handle all data fetching, caching, and processing
- Services should manage their own async patterns and emit signals — screens should not manage `QtConcurrent::run()` themselves
- If a screen needs data, it calls a service method with a callback or connects to a service signal

### P7. Stylesheet Performance

- There are 772+ inline `setStyleSheet()` calls across the codebase — each triggers a CSS reparse and widget repaint
- **For new code**: prefer `setObjectName()` + global stylesheet selectors over inline `setStyleSheet()`
- **For repeated styles** (same style on 10+ labels): use a parent-level stylesheet with descendant selectors instead of per-widget calls
- Group related styles in `Theme.cpp` / `StyleSheets.cpp` using class or object name selectors
- Example — instead of:
```cpp
label->setStyleSheet("color:#d97706;font-size:14px;font-weight:700;");
```
Do:
```cpp
label->setObjectName("sectionTitle");
// In global stylesheet: #sectionTitle { color: #d97706; font-size: 14px; font-weight: 700; }
```

### P8. Safe Async Lambda Captures

- **NEVER** capture raw `[this]` in lambdas passed to `QtConcurrent::run()` — the widget may be destroyed before the lambda executes, causing a crash
- Use `QPointer` guard:
```cpp
QPointer<MyScreen> self = this;
QtConcurrent::run([self]() {
    auto result = expensive_operation();
    if (!self) return;  // widget was destroyed
    QMetaObject::invokeMethod(self, [self, result]() {
        if (!self) return;
        self->update_ui(result);
    }, Qt::QueuedConnection);
});
```
- For `connect()` with `this` as context object, Qt auto-disconnects on destroy — this is safe:
```cpp
connect(reply, &QNetworkReply::finished, this, [this]() { ... });  // safe
```

### P9. Paint Performance

- **Cache expensive paint operations** as `QPixmap` — redraw only on `resizeEvent()`, blit in `paintEvent()`
- `GeometricBackground` uses this pattern — follow it for any custom-painted widget with static/semi-static content
- Keep `QTimer`-driven animations at 20fps or less unless smooth animation is critical (scrolling ticker: 50ms interval, not 30ms)
- Avoid `QPainter::Antialiasing` render hint on large fills — it's expensive and often not visible
- Avoid calling `update()` from inside `paintEvent()` — it creates an infinite repaint loop

### P10. Avoid Redundant Object Creation

- Don't create widgets you won't show — use factories or conditional construction
- Don't allocate `new QProcess` for every Python call when a persistent worker process would suffice
- Reuse `QNetworkAccessManager` instances (one per service, not one per request) — already done in `HttpClient` and `NewsService`
- Don't create new `QTimer` objects when you can reuse one with `setInterval()` changes

### P11. Data Caching

- `NewsService` caches articles with a TTL — follow this pattern for all data fetches
- `ExchangeService` caches prices in `price_cache_` — services should always cache last-known-good data
- Screens should display cached/stale data immediately, then update when fresh data arrives
- Never show a blank screen while waiting for data — show loading state or cached data

### P12. Signal/Slot Best Practices

- Use pointer-to-member signal/slot syntax: `connect(obj, &Class::signal, this, &MyClass::slot)`
- Never use string-based `SIGNAL()`/`SLOT()` macros — they bypass compile-time checking
- Prefer direct connections for same-thread, `Qt::QueuedConnection` for cross-thread
- Disconnect or guard connections when objects have different lifetimes

### P13. Memory and Resource Management

- Use `deleteLater()` for QObjects being deleted inside signal handlers — never `delete` directly
- Use parent-child ownership for widgets — pass `this` as parent to avoid leaks
- Guard `QProcess*` with `deleteLater()` in both `finished` and `errorOccurred` handlers
- Use RAII wrappers (`std::unique_ptr`, `QScopedPointer`) for non-QObject heap allocations

### P14. Logging Discipline

- Use `LOG_INFO` for state changes and milestones (screen navigated, data fetched)
- Use `LOG_WARN` for recoverable issues (feed timeout, cache miss)
- Use `LOG_ERROR` for failures that affect functionality (script crash, parse error)
- Use `LOG_DEBUG` for verbose diagnostics (HTTP request URLs, timer ticks)
- Include context in log messages: `LOG_INFO("MarketData", "Fetched 12 quotes")` not `LOG_INFO("", "done")`
- Never log sensitive data (API keys, credentials, user tokens)

### P15. Thread Safety

- Use `QMutex`/`QMutexLocker` for shared state between Qt threads, not `std::mutex` (which can deadlock with Qt's event loop)
- Use `std::atomic<bool>` for simple flags (like `candles_fetching_`)
- Never access UI widgets from background threads — always post via `QMetaObject::invokeMethod` with `Qt::QueuedConnection`
- `QProcess` must be created and managed on the thread that owns it

## Security

- Encrypted credential storage via `SecureStorage`
- Input sanitization at system boundaries
- Never commit `.env`, API keys, or credentials

## Build & Deployment

- Windows: `.exe` (MSVC x64), Qt DLLs bundled via `windeployqt`
- macOS: native binary, Qt frameworks bundled via `macdeployqt`
- Linux: native binary, system Qt packages
- Build uses CMake 3.20+

## Documentation

```
docs/
├── CONTRIBUTING.md
├── GETTING_STARTED.md
├── ARCHITECTURE.md
├── CODE_OF_CONDUCT.md
├── COMMERCIAL_LICENSE.md
├── PYTHON_CONTRIBUTOR_GUIDE.md
├── CPP_CONTRIBUTOR_GUIDE.md
└── translations/              # Internationalized READMEs
```

## Links

- **GitHub**: https://github.com/Fincept-Corporation/FinceptTerminal
- **Releases**: https://github.com/Fincept-Corporation/FinceptTerminal/releases
- **Discord**: https://discord.gg/ae87a8ygbN
- **Email**: support@fincept.in

---

**Last Updated**: 2026-03-21
**Version**: 4.0.0
**Maintained By**: Fincept Corporation
