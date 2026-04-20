# Fincept Terminal Architecture
## Technical Design & System Architecture

This document provides a comprehensive overview of Fincept Terminal v4's architecture — a native C++20 desktop application.

---

## System Overview

### High-Level Architecture

```
┌───────────────────────────────────────────────────────────────────┐
│                         User Interface Layer                       │
│  ┌─────────────────────────────────────────────────────────────┐  │
│  │  Qt6 Widgets + Qt6 Charts                                   │  │
│  │  - Obsidian design system (Bloomberg-style terminal UI)     │  │
│  │  - Real-time data visualization                             │  │
│  │  - Native platform rendering                                │  │
│  └─────────────────────────────────────────────────────────────┘  │
├───────────────────────────────────────────────────────────────────┤
│                       Application Layer                            │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────────┐ │
│  │  Screens  │  │ Services │  │  Trading │  │  MCP Integration │ │
│  │  (40+)   │  │ (Data)   │  │  Engine  │  │  (AI Tools)      │ │
│  └──────────┘  └──────────┘  └──────────┘  └──────────────────┘ │
├───────────────────────────────────────────────────────────────────┤
│                      Infrastructure Layer                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────────┐ │
│  │  HTTP     │  │  SQLite  │  │ WebSocket│  │  Python Bridge   │ │
│  │  (Qt Net) │  │(Qt Sql)  │  │ (Qt WS)  │  │  (100+ scripts)  │ │
│  └──────────┘  └──────────┘  └──────────┘  └──────────────────┘ │
├───────────────────────────────────────────────────────────────────┤
│                       Platform Layer                               │
│  ┌─────────────────────────────────────────────────────────────┐  │
│  │  Qt6 Platform Abstraction                                   │  │
│  │  Windows (MSVC) / macOS (Clang) / Linux (GCC)              │  │
│  └─────────────────────────────────────────────────────────────┘  │
└───────────────────────────────────────────────────────────────────┘
```

---

## Technology Stack

| Component | Technology | Purpose |
|-----------|-----------|---------|
| Language | C++20 | Core application |
| UI Framework | Qt6 Widgets | Native retained-mode GUI |
| Charts | Qt6 Charts | Financial charts & plots |
| Networking | Qt6 Network | HTTP API calls, TLS |
| WebSockets | Qt6 WebSockets | Real-time streaming feeds |
| Database | Qt6 Sql (SQLite) | Local storage, caching |
| JSON | QJsonDocument | Serialization |
| Logging | Custom Logger (QFile) | Structured logging |
| Analytics | Python 3.11+ | Embedded runtime for scripts |
| Build | CMake 3.20+ | Build system |

---

## Source Architecture

```
fincept-qt/src/
├── app/
│   ├── main.cpp                    # Entry point, QApplication setup
│   ├── MainWindow.cpp/h            # Main window, layout, screen hosting
│   └── ScreenRouter.cpp/h          # QStackedWidget-based navigation
│
├── core/                           # Shared infrastructure
│   ├── config/AppConfig.cpp/h      # App-wide constants (URLs, versions)
│   ├── events/EventBus.cpp/h       # Pub/sub for decoupled communication
│   ├── logging/Logger.cpp/h        # Structured logging (LOG_INFO, LOG_ERROR)
│   ├── result/Result.h             # Result<T> error handling type
│   └── session/SessionManager.cpp/h
│
├── ui/                             # Reusable Qt widgets (Obsidian design system)
│   ├── theme/
│   │   ├── Theme.cpp/h             # Color tokens, font constants
│   │   └── StyleSheets.cpp/h       # Qt stylesheets for all components
│   ├── widgets/
│   │   ├── Card.cpp/h              # Panel container
│   │   ├── SearchBar.cpp/h
│   │   ├── StatusBadge.cpp/h
│   │   ├── GeometricBackground.cpp/h
│   │   ├── TabHeader.cpp/h
│   │   └── TabFooter.cpp/h
│   ├── tables/DataTable.cpp/h      # Reusable data table
│   ├── charts/ChartFactory.cpp/h   # Qt6 Charts factory
│   └── navigation/
│       ├── NavigationBar.cpp/h     # Left sidebar navigation
│       ├── FKeyBar.cpp/h           # Function key shortcuts bar
│       ├── StatusBar.cpp/h         # Bottom status bar
│       └── ToolBar.cpp/h           # Top toolbar
│
├── network/
│   ├── http/HttpClient.cpp/h       # QNetworkAccessManager wrapper
│   └── websocket/WebSocketClient.cpp/h  # Qt6 WebSocket wrapper
│
├── storage/
│   ├── sqlite/
│   │   ├── Database.cpp/h          # Main database
│   │   ├── CacheDatabase.cpp/h     # Cache database
│   │   └── migrations/             # Versioned schema migrations
│   ├── cache/
│   │   ├── CacheManager.cpp/h
│   │   └── TabSessionStore.cpp/h
│   ├── secure/SecureStorage.cpp/h  # Encrypted credential storage
│   └── repositories/               # Data access objects (13 repositories)
│       ├── SettingsRepository.cpp/h
│       ├── WatchlistRepository.cpp/h
│       ├── ChatRepository.cpp/h
│       └── ...
│
├── auth/
│   ├── AuthManager.cpp/h           # Login, JWT, guest mode
│   ├── AuthApi.cpp/h               # Auth API calls
│   ├── UserApi.cpp/h               # User API calls
│   ├── SessionGuard.cpp/h          # Auto-logout on 401
│   └── AuthTypes.h                 # Shared auth types
│
├── python/
│   └── PythonRunner.cpp/h          # Execute Python scripts, capture stdout
│
├── trading/
│   ├── BrokerInterface.h           # Abstract broker interface
│   ├── BrokerRegistry.cpp/h        # Broker registration
│   ├── ExchangeService.cpp/h       # Exchange connectivity
│   ├── OrderMatcher.cpp/h          # Order matching engine
│   ├── PaperTrading.cpp/h          # Paper trading engine
│   ├── UnifiedTrading.cpp/h        # Unified trading facade
│   └── brokers/                    # 20+ broker implementations
│       ├── ZerodhaBroker.h
│       ├── FyersBroker.cpp/h
│       ├── UpstoxBroker.h
│       ├── IBKRBroker.h
│       ├── AlpacaBroker.h
│       ├── SaxoBankBroker.h
│       └── ...
│
├── services/
│   ├── markets/MarketDataService.cpp/h
│   └── news/
│       ├── NewsService.cpp/h
│       ├── NewsClusterService.cpp/h
│       └── NewsMonitorService.cpp/h
│
└── screens/                        # Terminal screens
    ├── auth/                       # Login, Register, ForgotPassword, Pricing
    ├── dashboard/                  # Dashboard + 13 widgets
    ├── markets/                    # Market data
    ├── news/                       # News aggregation + clustering
    ├── watchlist/                  # Watchlist management
    ├── crypto_trading/             # Crypto trading (7 components)
    ├── report_builder/             # Report generation (4 components)
    ├── notes/                      # Notes
    ├── profile/                    # User profile
    ├── settings/                   # App settings
    ├── support/                    # Support
    ├── about/                      # About screen
    └── ComingSoonScreen.cpp/h      # Placeholder for upcoming screens
```

---

## Design Patterns

### Screen/Service Separation

Every screen follows a strict separation:

- **Screens** (`*Screen.cpp`) — render UI only, no HTTP calls, no business logic
- **Services** (`*Service.cpp`) — handle fetching, caching, processing
- Screens connect to services via Qt signals/slots, never call `HttpClient` directly

```
User Interaction
      │
      ▼
Screen (*Screen.cpp)         ← UI rendering only (QWidget subclass)
      │  signals/slots
      ▼
Service (*Service.cpp)       ← Fetching, caching, processing
      │
      ├─── HttpClient        ← API calls (QNetworkAccessManager)
      ├─── PythonRunner      ← Analytics scripts
      └─── Database          ← Local storage (Qt Sql / SQLite)
```

### Core Infrastructure

- **`Result<T>`** for error handling instead of raw error codes or exceptions
- **`LOG_INFO("tag", "msg")`** for structured logging
- **`EventBus::instance().publish("event", data)`** for cross-module communication
- **`AppConfig::instance().api_base_url()`** for constants — no magic strings

### Threading Model

- UI code runs on the main thread only (Qt requirement)
- Background work via `QThread` or `QtConcurrent`
- Results posted back to UI thread via `QMetaObject::invokeMethod` or signal/slot across threads
- Shared state protected with `QMutex`

---

## Data Flow

### API Data Flow

```
User clicks "Get AAPL quote"
        │
        ▼
Screen (MarketsScreen.cpp)
        │
        ▼
Service (MarketDataService.cpp)
        │
        ├─── Option A: HTTP API call (HttpClient / QNetworkAccessManager)
        │         │
        │         ▼
        │    Parse JSON (QJsonDocument)
        │
        └─── Option B: Python Script (PythonRunner)
                  │
                  ▼
             Spawns Python process
                  │
                  ▼
             Script outputs JSON to stdout
                  │
                  ▼
             C++ parses JSON response
        │
        ▼
Signal emitted → Screen slot updates UI
```

### Python Integration

```
C++ (PythonRunner.cpp)
    │
    ▼
QProcess spawns Python interpreter
    │
    ▼
Executes script (scripts/Analytics/... or scripts/*.py)
    │
    ▼
Script outputs JSON to stdout
    │
    ▼
C++ reads stdout, parses QJsonDocument
    │
    ▼
Data returned to calling service/screen
```

---

## Security Architecture

- **Encrypted credential storage** via `SecureStorage` (platform keychain or AES)
- **Input sanitization** at system boundaries
- **Qt TLS** for all HTTPS connections
- **No secrets in source** — environment-based configuration
- **SessionGuard** automatically logs out on 401 responses

---

## Build System

### CMake + Qt6

```cmake
find_package(Qt6 REQUIRED COMPONENTS
    Widgets Charts PrintSupport Network Sql
)
find_package(Qt6 QUIET COMPONENTS WebSockets)
```

No external package manager required — Qt6 provides all core dependencies.

### Platform Targets

| Platform | Compiler | Output | Qt DLL bundling |
|----------|----------|--------|-----------------|
| Windows | MSVC 2022 | `.exe` | `windeployqt` (automatic POST_BUILD) |
| macOS | Clang 15+ | native binary | `macdeployqt` |
| Linux | GCC 12+ | native binary | system Qt packages |

---

## Contact

- **Email:** support@fincept.in
- **GitHub Issues:** https://github.com/Fincept-Corporation/FinceptTerminal/issues
- **Discord:** https://discord.gg/ae87a8ygbN

---

**Version**: 4.0.1
**Last Updated**: March 2026
