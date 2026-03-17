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
│  │  Dear ImGui (Docking) + ImPlot + Yoga Layout                │  │
│  │  - Bloomberg-style Terminal UI                              │  │
│  │  - Real-time Data Visualization                             │  │
│  │  - GPU-accelerated Rendering (OpenGL 3.3+)                  │  │
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
│  │  (curl)   │  │  Storage │  │  Streams │  │  (100+ scripts)  │ │
│  └──────────┘  └──────────┘  └──────────┘  └──────────────────┘ │
├───────────────────────────────────────────────────────────────────┤
│                       Platform Layer                               │
│  ┌─────────────────────────────────────────────────────────────┐  │
│  │  GLFW 3 + OpenGL 3.3+ + OpenSSL                             │  │
│  │  Windows (MSVC) / macOS / Linux                             │  │
│  └─────────────────────────────────────────────────────────────┘  │
└───────────────────────────────────────────────────────────────────┘
```

---

## Technology Stack

| Component | Technology | Purpose |
|-----------|-----------|---------|
| Language | C++20 | Core application |
| UI Framework | Dear ImGui (docking) | Immediate-mode GUI |
| Charts | ImPlot | Financial charts & plots |
| Layout | Yoga | Flexbox-based layout engine |
| Window/Input | GLFW 3 | Cross-platform windowing |
| Graphics | OpenGL 3.3+ | GPU rendering |
| HTTP | libcurl + OpenSSL | API calls, TLS |
| Database | SQLite 3 | Local storage, caching |
| JSON | nlohmann/json | Serialization |
| Logging | spdlog | Structured logging |
| Audio | miniaudio | Sound notifications |
| Video | libmpv (optional) | Inline video playback |
| Analytics | Python 3.11+ | Embedded runtime for scripts |
| Build | CMake 3.20+ | Build system |
| Dependencies | vcpkg | Package management |

---

## Source Architecture

```
fincept-cpp/src/
├── main.cpp                    # Entry point, GLFW/OpenGL setup
├── app.cpp/h                   # App state machine, routing, tab bar
│
├── core/                       # Shared infrastructure
│   ├── config.h                # App-wide constants (URLs, timeouts, versions)
│   ├── event_bus.h             # Pub/sub for decoupled module communication
│   ├── logger.h                # Structured logging (LOG_INFO, LOG_ERROR)
│   ├── result.h                # Result<T> error handling type
│   ├── notification.h/cpp      # Desktop notifications
│   ├── raii.h                  # RAII resource wrappers
│   ├── hot_reload.cpp          # Live config reload
│   ├── font_loader.cpp         # Font management
│   └── window.cpp              # Window management
│
├── ui/                         # Reusable ImGui widgets
│   ├── widgets/                # Card, Table, Modal, SearchBar, etc.
│   └── tile_map.cpp            # Tile-based layout system
│
├── http/                       # HTTP client + API types
│   └── http_client.cpp         # libcurl wrapper
│
├── storage/                    # SQLite + encrypted storage
│   └── database.cpp            # Database operations
│
├── auth/                       # Authentication
│   └── auth_manager.cpp        # Login, JWT, guest mode
│
├── python/                     # Python runtime bridge
│   ├── python_runner.cpp       # Execute Python scripts
│   └── setup_manager.cpp       # Python environment setup
│
├── mcp/                        # Model Context Protocol
│   ├── mcp_client.h            # MCP client
│   ├── mcp_provider.cpp        # MCP provider management
│   └── mcp_service.cpp         # MCP service layer
│
├── trading/                    # Trading infrastructure
│   ├── broker_interface.h      # Abstract broker interface
│   ├── broker_registry.cpp     # Broker registration
│   ├── exchange_service.cpp    # Exchange connectivity
│   ├── order_matcher.cpp       # Order matching engine
│   └── brokers/                # Broker implementations
│       ├── generic_broker.h
│       ├── alpaca_broker.cpp
│       ├── zerodha_broker.cpp
│       ├── ibkr_broker.cpp
│       └── ...                 # 15+ broker adapters
│
├── portfolio/                  # Portfolio management
├── media/                      # Media playback
├── voice/                      # Voice features
├── vendor/                     # Vendored third-party code
│
└── screens/                    # 40+ terminal screens
    ├── dashboard/              # Main dashboard + widgets
    ├── markets/                # Market data display
    ├── news/                   # News aggregation
    ├── watchlist/              # Watchlist management
    ├── crypto_trading/         # Crypto trading
    ├── equity_trading/         # Equity trading
    ├── algo_trading/           # Algorithmic trading
    ├── backtesting/            # Strategy backtesting
    ├── research/               # Equity research panels
    ├── screener/               # Stock screener
    ├── quantlib/               # 18 quant modules
    ├── ai_chat/                # AI chat interface
    ├── ai_quant_lab/           # ML/AI analytics
    ├── agent_studio/           # AI agent management
    ├── alpha_arena/            # Alpha Arena
    ├── economics/              # Economic data
    ├── dbnomics/               # DBnomics data
    ├── akshare/                # Chinese market data
    ├── asia_markets/           # Asia markets
    ├── geopolitics/            # Geopolitical analysis
    ├── gov_data/               # Government data
    ├── maritime/               # Maritime tracking
    ├── polymarket/             # Prediction markets
    ├── relationship_map/       # Entity relationships
    ├── code_editor/            # Code editor
    ├── node_editor/            # Visual workflows
    ├── excel/                  # Spreadsheet
    ├── report_builder/         # Report generation
    ├── notes/                  # Notes
    ├── data_sources/           # Data source management
    ├── data_mapping/           # Data mapping
    ├── mcp_servers/            # MCP server management
    ├── settings/               # App settings
    ├── profile/                # User profile
    ├── docs/                   # In-app documentation
    ├── forum/                  # Community forum
    ├── support/                # Support
    └── about/                  # About screen
```

---

## Design Patterns

### Screen/Service Separation

Every screen follows a strict separation:

- **Screens** (`*_screen.cpp`) — render UI only, no HTTP calls, no business logic
- **Data/Service** classes (`*_data.cpp`) — handle fetching, caching, processing
- Screens call data services, never `HttpClient` directly

```
User Interaction
      │
      ▼
Screen (*_screen.cpp)        ← UI rendering only
      │
      ▼
Data Service (*_data.cpp)    ← Fetching, caching, processing
      │
      ├─── HTTP Client       ← API calls
      ├─── Python Runner     ← Analytics
      └─── SQLite            ← Local storage
```

### Core Infrastructure

- **`Result<T>`** for error handling instead of raw error codes
- **`LOG_INFO("tag", "msg")`** for structured logging
- **`EventBus::instance().publish("event", data)`** for cross-module communication
- **`config::API_BASE_URL`** for constants — no magic strings

### Threading Model

- UI code runs on main thread only (ImGui requirement)
- Background work via `std::async` / `std::thread`
- Shared state protected with `std::mutex`
- Simple flags use `std::atomic`

---

## Data Flow

### API Data Flow

```
User clicks "Get AAPL quote"
        │
        ▼
Screen (markets_screen.cpp)
        │
        ▼
Data Service (markets_data.cpp)
        │
        ├─── Option A: HTTP API call (libcurl)
        │         │
        │         ▼
        │    Parse JSON (nlohmann/json)
        │
        └─── Option B: Python Script
                  │
                  ▼
             Python Runner executes script
                  │
                  ▼
             Returns JSON to C++
        │
        ▼
UI updates with data
```

### Python Integration

```
C++ (python_runner.cpp)
    │
    ▼
Spawns Python process
    │
    ▼
Executes script (scripts/Analytics/...)
    │
    ▼
Script outputs JSON to stdout
    │
    ▼
C++ parses JSON response
    │
    ▼
Data returned to calling screen
```

---

## Security Architecture

- **AES-256-GCM** encrypted credential storage
- **Input sanitization** at system boundaries
- **OpenSSL** for all TLS connections
- **No secrets in source** — environment-based configuration

---

## Build System

### CMake + vcpkg

```bash
# vcpkg dependencies (vcpkg.json)
glfw3, curl, nlohmann-json, sqlite3, openssl,
imgui (docking + freetype), yoga, stb, implot,
spdlog, miniaudio

# Optional
libmpv (video playback)
```

### Unity Builds

Unity (jumbo) builds are enabled by default for faster compilation — CMake merges `.cpp` files into batches of 12.

### Platform Targets

| Platform | Compiler | Output |
|----------|----------|--------|
| Windows | MSVC 2022 | `.exe` |
| macOS | Clang 15+ | native binary |
| Linux | GCC 12+ | native binary |

---

## Contact

- **Email:** support@fincept.in
- **GitHub Issues:** https://github.com/Fincept-Corporation/FinceptTerminal/issues
- **Discord:** https://discord.gg/ae87a8ygbN

---

**Version**: 4.0.0
**Last Updated**: March 2026
