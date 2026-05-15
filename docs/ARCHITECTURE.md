# Fincept Terminal — Architecture

**Status:** Living document. Reflects the codebase as of 2026-05 and the target state we are converging on.
**Audience:** Contributors (C++ and Python), maintainers, and AI assistants modifying the codebase.
**Companion docs:** [`REFACTOR_PLAN.md`](REFACTOR_PLAN.md) (phased execution), [`../fincept-qt/DATAHUB_ARCHITECTURE.md`](../fincept-qt/DATAHUB_ARCHITECTURE.md) (data plane spec).

> **How to read this doc.** Sections marked **(current)** describe the codebase today. Sections marked **(target)** describe the shape we're moving toward. Where they diverge, the refactor plan owns the delta.

---

## 1. What this codebase is

Fincept Terminal v4 is a native C++20/Qt6 desktop application — a Bloomberg-style multi-window financial workstation with embedded Python analytics, multi-broker trading, AI agents, and an in-process data plane.

| Dimension | Scale |
|-----------|-------|
| C++ source files | ~1,626 (`.cpp`/`.h`) |
| C++ lines | ~342,000 |
| Python scripts | ~1,423 |
| Screens | 54 (lazy-instantiated) |
| Services | ~50 (data, trading, AI, workflow) |
| Brokers | 16 (equity/F&O) + 2 crypto exchanges |
| Repositories | 26 (typed via `BaseRepository<T>`) |
| MCP tools | 40+ |
| Build target | one Qt6 desktop binary, per OS |

This is a **modular monolith** by intent — single deployable, internally divided into bounded contexts with explicit dependency direction. Microservices are an anti-goal for the desktop runtime.

---

## 2. Architecture at a glance

```
┌─────────────────────────────────────────────────────────────────────┐
│  PRESENTATION                                                       │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │  Screens (54)        DashboardWidgets (13)    DockManager     │  │
│  │  via DockScreenRouter (lazy factory)                          │  │
│  │  state via IStatefulScreen + ScreenStateManager               │  │
│  └───────────────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────────────┤
│  APPLICATION  (bounded contexts)                                    │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────┐ │
│  │ Markets  │  │  News    │  │ Trading  │  │  Agents  │  │ AI   │ │
│  │ Economics│  │ Geopol   │  │ Portfolio│  │ Workflow │  │ Chat │ │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘  └──────┘ │
├─────────────────────────────────────────────────────────────────────┤
│  DATA PLANE                                                         │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │  DataHub  ──  one-fetch/many-subscribers pub/sub by topic     │  │
│  │  CacheManager (SQLite-backed TTL cache)                       │  │
│  └───────────────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────────────┤
│  INTEGRATION ADAPTERS                                               │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────┐ │
│  │  Broker  │  │   MCP    │  │  Python  │  │   HTTP   │  │  WS  │ │
│  │ Adapter  │  │  Tools   │  │  Runner  │  │  Client  │  │ Feed │ │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘  └──────┘ │
├─────────────────────────────────────────────────────────────────────┤
│  INFRASTRUCTURE                                                     │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │  Logger · AppConfig · EventBus · SessionManager · AuthManager │  │
│  │  Database (SQLite + migrations) · CacheDatabase               │  │
│  │  SecureStorage (SQLite + AES-256-GCM)                         │  │
│  │  Repositories (BaseRepository<T>, 26 implementations)         │  │
│  └───────────────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────────────┤
│  PLATFORM  Qt6 abstraction — Windows (MSVC) / macOS (Clang) / Linux │
└─────────────────────────────────────────────────────────────────────┘
```

**Hard rules for dependency direction:**

1. Presentation → Application → Data Plane → Adapters → Infrastructure → Platform. Never reverse.
2. Adapters are leaves. Two services may share an adapter; an adapter may not call a service.
3. Cross-context calls (e.g. Markets → Trading) go through **DataHub topics** or **typed events**, never direct includes.
4. Infrastructure has no business knowledge. It does not know what a "watchlist" is.

---

## 3. Technology stack

| Component | Tech | Role |
|-----------|------|------|
| Language | C++20 | Core runtime |
| UI | Qt6 Widgets + Qt6 Charts | Retained-mode native UI |
| Async (target) | QCoro (C++20 coroutines) | `co_await QNetworkReply`, `co_await QFuture` |
| Async (current) | Callbacks with `QPointer` context, signals/slots | Backward-compatible until full QCoro migration |
| Networking | Qt6 Network (HTTP/TLS), Qt6 WebSockets | All I/O |
| Database | Qt6 Sql + SQLite | Local persistence, two physical DBs (`Database`, `CacheDatabase`) |
| Schema | Versioned migrations under `storage/sqlite/migrations/` | Forward-only |
| Crypto | AES-256-GCM via SQLite-side encryption | `SecureStorage` only |
| JSON | `QJsonDocument` | All wire formats |
| Logging | Custom `Logger` with macros (`LOG_INFO`, `LOG_ERROR`) | Async-safe; lazy stringification |
| Python | 3.11.9 via UV-managed bundled venv | Analytics, agents, data fetchers |
| Docking | ADS (Advanced Docking System) via `CDockManager` | Multi-window/multi-panel layouts |
| Build | CMake 3.20+ (target: per-module subdirectories) | One binary per OS |
| Packaging | `windeployqt` / `macdeployqt` + Python bundle | See `fincept-qt/packaging/` |

---

## 4. Bounded contexts

Each context owns its screens, services, types, and DataHub topics. Contexts publish; they do not call each other.

| Context | Owns | Topic prefixes |
|---------|------|----------------|
| **Markets** | Equity/FX/commodity quotes, history, watchlists, sectors | `market:*`, `watchlist:*` |
| **News** | Aggregation, clustering, monitors, dedup, deviation | `news:*` |
| **Economics** | FRED, DBnomics, government indicators | `econ:*` |
| **Geopolitics** | HDX events, ACLED, maritime | `geopolitics:*` |
| **Trading** | Live brokers, paper trading, order matching, positions | `broker:<id>:*`, `paper:*` |
| **Portfolio** | Holdings aggregation, P&L, allocation | `portfolio:*` |
| **Crypto** | Hyperliquid, Kraken, on-chain, wallets | `ws:<exchange>:*`, `wallet:*` |
| **Derivatives** | Option chains, F&O, surface analytics | `derivatives:*` |
| **Predictions** | Polymarket, Kalshi, internal | `prediction:*` |
| **Agents** | Hedge fund, geopolitics, economics, trader, finagent | `agent:<kind>:run:<id>` |
| **AI Chat** | LLM provider routing, prompt context, history | (event-driven, not DataHub) |
| **Workflow** | Node editor, DAG executor, scheduled flows | `workflow:*` |
| **Identity** | Auth, sessions, profile, billing | (event-driven) |

A new feature lives entirely inside one context, or is explicitly cross-cutting (logged in the refactor plan).

---

## 5. Source tree (current, summarized)

```
fincept-qt/
├── CMakeLists.txt                  # Monolithic build (target: per-module CMakes)
├── CMakePresets.json
├── DATAHUB_ARCHITECTURE.md         # Spec for the data plane
├── DATAHUB_PHASES.md
├── cmake/                          # Toolchain helpers
├── packaging/                      # Per-OS installer scripts
├── resources/                      # Icons, fonts, translations
├── scripts/                        # 1,423 Python scripts (analytics, agents, data)
└── src/
    ├── app/                        # Entry point, WindowFrame, routers
    ├── core/                       # Infrastructure: events, logging, session, config…
    ├── ui/                         # Reusable Qt widgets (Obsidian design system)
    ├── network/                    # HTTP + WebSocket clients
    ├── storage/                    # Database, CacheManager, SecureStorage, repositories
    ├── auth/                       # AuthManager, AuthApi, SessionGuard
    ├── python/                     # PythonRunner (subprocess bridge)
    ├── datahub/                    # DataHub pub/sub data plane
    ├── mcp/                        # Model Context Protocol bridge + tool registry
    ├── trading/                    # BrokerInterface, 16 brokers, exchanges, matching
    ├── services/                   # ~50 bounded-context services
    └── screens/                    # 54 screens (Presentation)
```

### 5.1 Subsystem map

#### `app/`
- `main.cpp` — bootstrap (QApplication, theme, Python setup, splash, first window).
- `WindowFrame.{cpp,h}` — main window orchestrator (~1100 LOC); hosts ADS dock manager, command bar, function-key bar, status bar. **Heavy but intentional.**
- `WindowFrame_Setup.cpp` — registers 54 lazy screen factories with `DockScreenRouter`.
- `DockScreenRouter.{cpp,h}` — the real router (`ScreenRouter.{cpp,h}` exists but is legacy/unused).

#### `core/`
- `config/AppConfig` — global constants (URLs, versions). Singleton.
- `events/EventBus` — string-keyed pub/sub (`O(n)` linear scan; 39 callsites). **Will be wrapped by typed event manifest.**
- `logging/Logger` — structured logging macros.
- `result/Result<T>` — error type, used by network/DB results.
- `session/SessionManager` — frame/panel/last-screen persistence.
- `identity/`, `profile/` — user-side state.
- `screen/`, `layout/`, `panel/`, `window/` — frame layout, ADS integration.
- `keys/`, `actions/`, `components/` — keybindings, command palette, popularity tracking.
- `crash/`, `debug/`, `telemetry/` — diagnostics.
- `symbol/` — symbol ref / drag-drop MIME (slated for unification with the new `Instrument` model).
- `net/` — bandwidth meter.
- `report/` — report-builder primitives.

#### `ui/`
- `theme/` — color tokens, font constants, stylesheets.
- `widgets/`, `components/`, `tables/`, `charts/`, `markdown/` — design-system components.
- `navigation/` — nav bar, F-key bar, status bar, toolbar.
- `command/` — command palette.
- `workspace/`, `pushpins/`, `notifications/`, `error/`, `debug/` — orthogonal UI surfaces.

#### `network/`
- `http/HttpClient` — `QNetworkAccessManager` wrapper with context-scoped callbacks (no dangling pointers).
- `websocket/WebSocketClient` — Qt6 WebSocket wrapper.

#### `storage/`
- `sqlite/Database` — main DB (auth, watchlists, portfolios, agent tasks, settings…).
- `sqlite/CacheDatabase` — separate physical DB for ephemeral cache.
- `sqlite/migrations/` — forward-only versioned schema.
- `cache/CacheManager` — TTL key/value store on `CacheDatabase`. The official caching API.
- `cache/TabSessionStore` — per-tab UI state.
- `secure/SecureStorage` — **SQLite-backed AES-256-GCM** (key from `machineUniqueId`). Platform keychains (Keychain/DPAPI/libsecret) are intentionally not used. Requires `Database` to be open first.
- `repositories/` — 26 typed CRUD adapters over `BaseRepository<T>`.
- `workspace/` — workspace-level persistence (multi-window layouts).

#### `auth/`
- `AuthManager` — login state, JWT, guest mode. **`AuthManager::session()` is the canonical source of fincept credentials**; `SettingsRepository` is a fallback persistence copy. Never cache credentials elsewhere.
- `AuthApi`, `UserApi` — server-side calls.
- `SessionGuard` — auto-logout on 401.
- `lock/` — app-lock screen.

#### `python/`
- `PythonRunner` — `QProcess`-based subprocess bridge.
  - Two managed venvs: `venv-numpy1` (legacy NumPy 1.x libs like vectorbt/gluonts) and `venv-numpy2` (default).
  - Concurrency cap of 3 processes; surplus queued.
  - Cold-start 0.5–1.5 s per call; result is JSON on stdout.
  - 22 API keys injected via `SecureStorage` per call; unmanaged credential-shaped env vars stripped to prevent leakage.
- (target) `python/ScriptCatalog` — name-resolved scripts replacing hardcoded path strings.
- (target) `python/WorkerPool` — persistent worker bound by a long-lived stdin/stdout protocol for hot paths.

#### `datahub/`
- `DataHub` — in-process pub/sub. Topic format `domain:subdomain:id[:modifier]`.
- `Producer` — service-side interface (patterns, refresh, rate limit, on-idle).
- `TopicPolicy` — TTL, min interval, push-only flag.
- See [`DATAHUB_ARCHITECTURE.md`](../fincept-qt/DATAHUB_ARCHITECTURE.md) for the contract.

#### `mcp/`
- `McpService` — unified tool surface for AI chat, agents, node editor.
- `McpProvider` — internal C++ tool registry.
- `McpManager` — external MCP server lifecycle and RPC.
- `dispatch/ToolDispatcher` — multi-round tool orchestration state machine.
- `dispatch/ProviderAdapter` — OpenAI / Anthropic / Gemini protocol shims.
- `tools/` — 40+ tool implementations bridging `MarketDataService`, `NewsService`, `AgentService`, `WatchlistTools`, `NotesTools`, `SettingsTools`, etc.

#### `trading/`
- `BrokerInterface.h` — base contract (32 virtual methods). **Shallow-but-wide today; refactor target is a deep `BrokerAdapter` with shared OAuth/mapping/parsing infrastructure.**
- `BrokerRegistry` — broker discovery.
- `brokers/BrokerHttp` — shared synchronous-blocking HTTP helper (uses `QEventLoop`; **must be called from a worker thread**).
- `brokers/<name>/` — 16 implementations (Zerodha, Fyers, Upstox, IBKR, Alpaca, Saxo, Kotak, Angel One, Dhan, AliceBlue, FivePaisa, Groww, IIFL, Motilal, Shoonya, Tradier).
- `exchanges/` — Hyperliquid, Kraken (crypto WebSocket).
- `instruments/`, `auth/` — symbol parsing, broker OAuth flows.
- `OrderMatcher`, `PaperTrading`, `UnifiedTrading` — three coherent engines: live routing, paper simulation, matching/SL/TP triggers.
- `TradingTypes.h` — stable shared vocabulary (`UnifiedOrder`, `BrokerOrderInfo`, `BrokerPosition`, enums).

#### `services/`
~50 services across the 13 bounded contexts. Three flavors:

1. **Data services (the default).** Own one or more DataHub topic patterns and refresh them on subscription demand. Never return a widget; never read from the UI thread synchronously. Examples: `MarketDataService`, `NewsService`, `EconomicsService`.
2. **Imperative services.** One-shot request/response over HTTP or Python (no topic, no cache). Used for search-as-you-type, ad-hoc tests, etc. Examples: `MarketSearchService`, `DataMappingTestClient`. They wrap `HttpClient`/`PythonRunner` so screens don't.
3. **UI-coordinator services.** Intentionally own modal dialog lifecycles on behalf of UI requesters because the dialog is the user's authority surface (wallet connect, update install, sign transaction). They may accept a `QWidget*` for dialog parenting — this is a deliberate, scoped exception, not a leak. Examples: `WalletService`, `UpdateService`. Test discipline: business logic still lives in producers; the dialog is purely a UI shell over a state machine the service already owns.

Caveats today: ~13 shallow data-services are thin Python-script wrappers; ~5 reach directly into `SettingsRepository::instance()` rather than via injected access. These are tagged in the refactor plan.

#### `screens/`
54 screens. The contract is:

> A screen is a `QWidget` subclass that renders state and accepts user input. It does not call `HttpClient` directly, does not own caches, and does not contain business logic (deduplication, deviation detection, risk calculation, etc.).

Caveats today: 6 screens violate the no-HTTP rule, ~3 screens (News, Derivatives) carry domain logic that belongs in services. These are tagged in the refactor plan.

---

## 6. Cross-cutting concerns

### 6.1 Threading

Qt's main thread is the UI thread. Everything that takes more than a microsecond must run elsewhere.

| What runs where | Rule |
|---|---|
| `QWidget`/`QPainter`/UI state | **Main thread only** (Qt requirement). |
| HTTP (`HttpClient`) | Worker via `QNetworkAccessManager`; callbacks marshalled back via Qt's event loop. |
| SQL (`Database`, repositories) | **Background** for any non-trivial query. Synchronous repository calls from UI are tolerated for tiny lookups only. The refactor plan eliminates the remaining UI-thread query sites. |
| Python (`PythonRunner`) | `QProcess` worker; results captured via signals. Always async. |
| Broker REST (`BrokerHttp`) | Worker; uses internal `QEventLoop` so callers **must not** be on the UI thread. |
| WebSocket feeds | Dedicated thread per exchange / per source. |
| `DataHub::publish` | Safe from any thread (queued connection to dispatch slot). |
| Slot dispatch | Always on the subscriber's thread (Qt::AutoConnection). |

**Target pattern (new code):** `QCoro::Task<T>` returning coroutines for async I/O — callers `co_await` HTTP, DB queries, Python calls. Existing callback code stays until migrated.

### 6.2 Async patterns

| Pattern | Where used | Status |
|---|---|---|
| Callback with `QPointer` context | Most current async APIs | Maintained for back-compat |
| Qt signals + request IDs | Older services (Markets, Watchlist, Equity) | Maintained |
| `QFuture` / `QtConcurrent::run` | Spotty | Will retire in favor of QCoro |
| `co_await` (QCoro) | New code | **Preferred going forward** |

The data plane normalizes this: services publish to DataHub regardless of how they fetched. UI subscribers only see topic updates.

### 6.3 Error handling

- `Result<T>` for synchronous fallible operations (DB queries, parsing).
- Signals for async failure (`xxx_failed(QString message)`).
- DataHub serves **last-known-good** on producer failure; retries with backoff per `TopicPolicy`.
- `SessionGuard` auto-logs out on HTTP 401 from fincept servers; broker 401s are handled per adapter.
- Logging via `LOG_ERROR("tag", "msg with " + context)`. No exceptions across module boundaries.

### 6.4 Security

| Surface | Mechanism |
|---|---|
| Credentials (broker tokens, API keys, fincept session) | `SecureStorage` — SQLite + AES-256-GCM. Key derived from `machineUniqueId`. Requires `Database` open. |
| Fincept session token | `AuthManager::session()` is canonical; `SettingsRepository` is fallback only. |
| TLS | Qt6's OS-trust-store path for all HTTPS. |
| 401 handling | `SessionGuard` auto-logout for fincept; per-adapter token-refresh for brokers. |
| Python env injection | 22 known API keys whitelisted; all other `*_API_KEY`/`*_SECRET`/`*_PASSWORD`/`*_TOKEN` env vars stripped from subprocess env to prevent leakage via `/proc/<pid>/environ`. |
| Storage on disk | SQLite DB on disk; encryption key derived locally. **Not** at-rest encrypted by the OS unless the user encrypts their disk. |
| Source secrets | None in repo. Env vars / `SecureStorage` only. |

### 6.5 Build

Today: a single `CMakeLists.txt` (~3,300 LOC).

Target: per-module library targets, declared in subdirectory `CMakeLists.txt` files, linked together by the top-level config:

```
fincept_core         (no Qt dependencies beyond QtCore where avoidable)
fincept_ui           (depends on fincept_core)
fincept_network      (depends on fincept_core)
fincept_storage      (depends on fincept_core)
fincept_auth         (depends on fincept_network, fincept_storage)
fincept_datahub      (depends on fincept_core, fincept_storage)
fincept_python       (depends on fincept_core)
fincept_trading      (depends on fincept_network, fincept_storage, fincept_datahub)
fincept_mcp          (depends on fincept_datahub, fincept_python)
fincept_services_*   (one target per bounded context)
fincept_screens_*    (one target per context's screens, links to fincept_ui)
fincept_app          (entry point; links everything)
```

Dependency direction enforced by CMake. Circular dependencies become impossible.

---

## 7. Data flow

### 7.1 Subscription-driven fetch (DataHub path — preferred)

```
User opens Markets screen
       │
       ▼
Screen subscribes to "market:quote:AAPL" via DataHub
       │
       ▼
DataHub checks CacheManager → fresh? deliver immediately.
                              stale? notify MarketDataProducer.
       │
       ▼
MarketDataProducer.refresh({"market:quote:AAPL"})
       │
       ├─── HttpClient → JSON parse  (async)
       └─── or PythonRunner          (async)
       │
       ▼
Producer calls hub.publish("market:quote:AAPL", quote)
       │
       ▼
All subscribers (Markets, Watchlist, Dashboard, AI Chat) receive update.
CacheManager persists.
```

Properties: **one fetch per (topic, source)**; subscribers fan out for free; cache and live-feed are unified.

### 7.2 Imperative command (MCP / agent path)

```
LLM emits tool call: place_order(symbol="AAPL", qty=10, side="buy")
       │
       ▼
ToolDispatcher → McpService::execute_openai_function_async(...)
       │
       ▼
Internal McpProvider tool → UnifiedTrading::place_order(account_id, order)
       │
       ├─── PaperTrading (if paper mode)
       └─── BrokerAdapter::place_order  (live)
       │
       ▼
Result returned to dispatcher; DataHub topic broker:<id>:orders updated.
       │
       ▼
UI screens subscribed to broker:<id>:orders refresh automatically.
```

### 7.3 Agentic mode

```
User starts hedge-fund agent task
       │
       ▼
AgentService::start_task(task_def)
       │
       ▼
PythonRunner spawns scripts/agents/finagent_core/main.py with streaming callback
       │
       ▼
Agent emits per-step events on stdout (JSON lines)
       │
       ├─── persisted to agent_tasks SQLite table  (durable)
       └─── published on DataHub topic agent:<kind>:run:<id>
       │
       ▼
AiChatScreen and AgentConfigScreen subscribe to topic for live progress.
```

Crash-resume is durable: state lives in SQL.

---

## 8. Public contracts

These are the surfaces external contributors and AI assistants should treat as stable:

| Contract | Where | Notes |
|---|---|---|
| DataHub topic format | `datahub/DataHub.h` + `DATAHUB_ARCHITECTURE.md` | Topic strings are versioned by their contents (e.g. `:1d` vs `:1m`). |
| `Producer` interface | `datahub/Producer.h` | New data sources implement this. |
| `IBroker` (`BrokerInterface.h`) | `trading/BrokerInterface.h` | Target: deepen into `BrokerAdapter` base. |
| `BrokerEnumMap<T>` (typed wire-value tables) | `trading/adapter/BrokerEnumMap.h` | Replaces per-broker `<broker>_order_type/side/product` switches with a data table. First adopted by Zerodha; other brokers migrate broker-by-broker. |
| `Instrument` (canonical symbol vocabulary) | `trading/instruments/InstrumentTypes.h` | All cross-broker code uses this — never raw broker strings. `Instrument::canonical_topic_id()` builds DataHub keys. |
| `InstrumentSource` + `SymbolResolver` | `trading/instruments/InstrumentSource.h`, `SymbolResolver.h` | New brokers register a source; `InstrumentService` dispatches through the resolver, never an if-chain. |
| `IStatefulScreen` | `screens/common/IStatefulScreen.h` | Screens that need to persist UI state. |
| `BaseRepository<T>` | `storage/repositories/BaseRepository.h` | Pattern for new typed repos. |
| `BaseWidget` | `screens/dashboard/widgets/BaseWidget.h` | New dashboard widgets inherit and override. |
| MCP tool definition | `mcp/McpProvider.h` | Tools are JSON-schema'd in their registration. |
| `Result<T>` | `core/result/Result.h` | Synchronous fallible return type. |
| Logging macros | `core/logging/Logger.h` | `LOG_INFO/WARN/ERROR("tag", "msg")`. |
| Theme tokens | `ui/theme/Theme.h` | Don't bake colors; use tokens. |

---

## 9. Patterns and anti-patterns

### Patterns (do this)

- A new data source → implement `Producer`, register topic patterns, publish on refresh.
- A new screen → register a **factory** with `DockScreenRouter` (lazy). Implement `IStatefulScreen` if state needs to survive restart.
- A new broker → (target) extend `BrokerAdapter`; provide endpoint table, field-name map, error mapper. Don't reimplement OAuth/HTTP/error-handling.
- A new MCP tool → define schema + handler in `McpProvider`. The dispatcher handles routing.
- A new Python script → (target) register in `ScriptCatalog`; declare input/output schema; refer by name.
- Async work → `co_await` via QCoro (new code) or signals with request IDs (existing).

### Anti-patterns (don't do this)

- **Screen calling `HttpClient::instance()` directly.** Goes through a service (data, imperative, or `MarketSearchService`-style search).
- **Data service exposing `QWidget*`.** Signals only. UI-coordinator services are the explicit exception.
- **Hard-coding broker enums in `if`-trees in screens.** Use `UnifiedOrder` + `BrokerAdapter`.
- **Caching in a screen via `QHash`.** Use `CacheManager` via DataHub. A `QHash<QString, ...>` field in a screen is permitted only if it's (a) a live-feed dispatch table cleared on hide, or (b) a view/index over data already owned by DataHub.
- **`qApp->setStyleSheet(...)` from inside a widget's own event handler.** Wayland will crash. Coalesce theme changes and dispatch via `Qt::QueuedConnection`.
- **Calling `SecureStorage` before `Database::open()`.** SecureStorage depends on the DB.
- **Caching fincept credentials.** Always read live from `AuthManager::session()`; `SettingsRepository` is fallback.
- **Synchronous DB query on the UI thread.** Marshal it.
- **Adding a new singleton.** Use the dependency container being introduced.

---

## 10. Known weaknesses (and their owners)

These are the deltas between current and target state. Each has a phase in [`REFACTOR_PLAN.md`](REFACTOR_PLAN.md):

| Weakness | Impact | Phase |
|---|---|---|
| 60–75% duplication across 16 broker implementations — `BrokerEnumMap` migration **complete for 14 of 16** (Alpaca + FivePaisa pending; they never had named helpers). Envelope/parse helpers still to come | Reduced; bigger wins still ahead in Phase 4.3+ | 4.x |
| ~~Symbol/Instrument model fragmented across 3 representations~~ — canonical `Instrument` already existed; `SymbolResolver` seam added Phase 3 | Down to per-broker migration (Phase 4) | 3 |
| ~~4 screens call `HttpClient` directly~~ ✅ resolved Phase 2 (via `MarketSearchService` + `DataMappingTestClient`) | — | 2 |
| ~~Wallet/Update services expose `QWidget*`~~ — reclassified as UI-coordinator services (intentional) | — | 2 |
| 40+ singletons accessed by `::instance()` | Test impossibility; static-init ordering risk | 9 |
| `EventBus` is stringly-typed, O(n) lookup, phantom events possible | Silent breakage | 10 |
| 13–15 shallow Python-wrapper services | Conceptual overhead | 5 |
| 3-way caching (CacheManager + ad-hoc QHash + service debouncers) | Drift, double-fetch | 6 |
| Hardcoded Python script paths in 10+ services | Rename breaks build | 11 |
| Python cold-start 0.5–1.5 s × per call | UX latency | 11 |
| Screens never unload — 100–250 MB resident per window | Multi-window users pay multiplicatively | 12 |
| 5 screen files >1,000 LOC without clear single-concern | Maintainability | 13 |
| Monolithic 3,300-LOC `CMakeLists.txt` | Dependency direction unenforced | 8 |
| `GenericBroker.h` is dead code; ~12 shallow utility headers | Cognitive load | 1 |
| Some UI-thread SQL query sites | Frame stutter | 7 |

---

## 11. Operational details

### Bundled Python

- UV bootstraps `python-3.11.9` and creates two venvs in parallel under `.aqt-venv/`.
- First-run installs ~150 packages; ~3–5 minutes on broadband.
- Cached for subsequent runs.
- Venvs live in the user data dir, not the install dir.

### Updates

- `UpdateService` polls `updates.json` from `fincept.in`; offers in-app updater (Windows/macOS).
- Linux builds rely on system Qt packages and are not auto-updated.

### Telemetry

- `core/telemetry/` provides opt-in crash and usage reporting.
- Off by default; requires explicit user consent in `SettingsScreen`.

---

## 12. Contact and process

- **Issues:** https://github.com/Fincept-Corporation/FinceptTerminal/issues
- **Contributing:** see `CONTRIBUTING.md`, `CPP_CONTRIBUTOR_GUIDE.md`, `PYTHON_CONTRIBUTOR_GUIDE.md`.
- **Architecture decisions:** add an ADR under `docs/adr/` when reversing or refining a section here.
- **Discord:** https://discord.gg/ae87a8ygbN
- **Email:** support@fincept.in

---

**Version:** 5.0.0-draft (architecture refresh)
**Supersedes:** previous 4.0.1 architecture doc (March 2026)
**Last updated:** 2026-05-15
