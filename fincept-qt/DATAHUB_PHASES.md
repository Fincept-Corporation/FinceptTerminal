# DataHub Rollout Phases

**Status:** Complete (Phases 0–10 all shipped, 2026-04-18)
**Version:** 1.0
**Companion to:** `DATAHUB_ARCHITECTURE.md`

This document lists the phases to build and migrate to the DataHub. Each phase is **self-contained, testable, and independently shippable**. No phase leaves the codebase in a broken state — the old and new paths coexist until the final phase.

---

## Phase Summary

| # | Phase                                      | Scope                                 | Rough size | Depends on |
|---|--------------------------------------------|---------------------------------------|------------|------------|
| 0 | Prep & Guardrails                          | Meta-types, logging, feature flag     | Small      | —          |
| 1 | Build the DataHub primitive                | `DataHub`, `Producer`, `TopicPolicy`  | Medium     | 0          |
| 2 | Market data — pilot                        | Convert `MarketDataService` + 1 widget| Medium     | 1          |
| 3 | Market data — full migration               | All 20+ quote consumers               | Large      | 2          |
| 4 | WebSocket producers                        | `ExchangeService`, `PolymarketWebSocket` | Medium  | 1          |
| 5 | News                                       | `NewsService` + all news screens      | Medium     | 1          |
| 6 | Economics & DBnomics                       | `EconomicsService`, `DBnomicsService` | Medium     | 1          |
| 7 | Broker account streams                     | 16 broker adapters → `broker:*` topics| Large      | 1, 4       |
| 8 | Geopolitics / Maritime / Gov Data          | Remaining data services               | Medium     | 1          |
| 9 | AI / MCP / Agents integration              | Subscribers, not producers            | Medium     | 3, 4, 5    |
| 10| Enforcement & cleanup                      | Lint rules, docs, kill old APIs       | Small      | All above  |

---

## Phase 0 — Prep & Guardrails  ✅ DONE

**Goal:** set up the ground before touching architecture.

### Deliverables — all landed
- ✅ `FINCEPT_DATAHUB_ENABLED` CMake option added (`CMakeLists.txt:123`), default ON, compiles to `-DFINCEPT_DATAHUB_ENABLED=1`.
- ✅ `Q_DECLARE_METATYPE` for `QuoteData`, `HistoryPoint`, `InfoData`, `NewsArticle`, `EconomicsResult` in `src/datahub/DataHubMetaTypes.h`.
- ✅ `qRegisterMetaType<...>()` calls in `src/datahub/DataHubMetaTypes.cpp::register_metatypes()`, invoked from `main.cpp` after `QApplication` construction, gated on the feature flag.
- ✅ Logger: the project uses free-form string tags (no registered categories), so we standardised on the tag string `"DataHub"` — zero changes to `Logger.h`.
- ✅ Stub headers in place: `src/datahub/DataHub.h`, `Producer.h`, `TopicPolicy.h`.
- ✅ Stub `DataHub.cpp` compiles under MOC and aborts with a clear message if any method (except `peek()`) is called before Phase 1.
- ✅ Sources wired into `add_executable` via `DATAHUB_SOURCES` variable.

### Success check
- Build is green on all three presets (`win-release`, `linux-release`, `macos-release`).
- No runtime changes.

### Risk
- None (preparatory only).

---

## Phase 1 — Build the DataHub Primitive  ✅ DONE

**Goal:** implement the hub itself, with unit tests, zero integrations.

### Deliverables — all landed
- ✅ `src/datahub/DataHub.{h,cpp}` — full public API per `DATAHUB_ARCHITECTURE.md` §4,
  including the typed `subscribe<T>()` template, wildcard patterns, push-only
  bypass, in-flight dedup, per-producer rate limiting, and cross-thread
  marshaling via `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`.
  Registry + scheduler live inline in the hub (single file — splitting into
  `SubscriptionRegistry.cpp` / `TopicScheduler.cpp` would be premature
  abstraction at ~300 lines).
- ✅ `src/datahub/Producer.h` — virtual interface.
- ✅ `src/datahub/TopicPolicy.h` — struct + defaults.
- ✅ Test harness: `FINCEPT_BUILD_TESTS` CMake option (default OFF),
  `tests/CMakeLists.txt` wired in via `add_subdirectory(tests)`.
- ✅ `tests/datahub/test_datahub.cpp` (Qt Test) — 9 cases covering:
  - subscribe → publish → slot invoked
  - subscribe receives cached value immediately if fresh
  - destroyed subscriber auto-unsubscribes
  - wildcard pattern matching (`test:wild:*`)
  - push-only policy skips scheduler
  - `request()` dedups while in-flight
  - cross-thread `publish()` marshals correctly
  - `peek()` reads without subscribing
  - scheduler respects `ttl_ms`
- ✅ Debug screen stub: `src/screens/devtools/DataHubInspector.{h,cpp}`
  wired into `SCREEN_SOURCES`. Refreshes `DataHub::stats()` every 1 s while
  visible; follows CLAUDE.md P3 (timer driven by `showEvent`/`hideEvent`).

### Success check
- All unit tests pass.
- Hub is registered as a global singleton but has zero producers yet — hub is idle.
- Inspector screen loads and shows empty topic list.

### Risk
- `QObject::destroyed()` ordering vs. subscription cleanup — covered by test.
- Cross-thread `publish()` with `QueuedConnection` — covered by test.

---

## Phase 2 — Market Data Pilot

**Goal:** prove the pattern end-to-end on one producer + one widget. Do **not** migrate anything else yet.

### Deliverables
- `MarketDataService` implements `Producer`:
  - declares `topic_patterns()` = `{"market:quote:*"}`
  - moves existing batched Python fetch logic into `refresh(topics)`
  - publishes results via `DataHub::publish("market:quote:<sym>", QVariant::fromValue(quote))`
  - old `fetch_quotes(callback)` API **still works** — becomes a thin wrapper that subscribes once, captures the value, unsubscribes. Zero consumer changes required yet.
- One widget class rewritten: **`QuoteTableWidget`**
  (`src/screens/dashboard/widgets/QuoteTableWidget.cpp:36`). This class is
  the base used by `IndicesWidget`, `CryptoWidget`, `ForexWidget`, and
  `CommoditiesWidget` factory functions, so migrating it in one place
  exercises the hub across four widgets at once.
  - deletes `refresh_data()` callback-based fetch
  - subscribes per-symbol to `market:quote:<sym>` in the ctor
  - stops relying on any external caller/timer to trigger refresh
  - verifies UI still updates identically to the pre-migration baseline
- Register default `TopicPolicy{ttl_ms=30000, min_interval_ms=5000}` for `market:quote:*`.
- Add `MarketDataService` to hub's producer registry at app startup.

### Success check
- Indices / Crypto / Forex / Commodities widgets all show identical data to before
  (they share `QuoteTableWidget`, so all four move together in this phase).
- Log shows `refresh()` fires on hub tick, not per-widget.
- Hub inspector shows the union of their symbols as `market:quote:*` topics,
  each with subscriber_count ≥ 1.
- No duplicate Python spawns when `QuoteTableWidget` instances + legacy
  `fetch_quotes` callers are both active.

### Risk
- Regression in Indices panel behaviour — mitigated by side-by-side test (both paths still work).
- Python process scheduling under hub cadence vs. old per-widget cadence — validated by looking at spawn count metrics before/after.

---

## Phase 3 — Market Data Full Migration

**Goal:** move every market-data consumer onto the hub. Delete per-widget timers.

### Consumers to migrate
Phase 2 already covered `QuoteTableWidget` (and therefore Indices / Crypto /
Forex / Commodities factories). Remaining market-data consumers confirmed
by `grep -rn fetch_quotes src/screens/`:

Dashboard — 11 widgets:
`WatchlistWidget`, `TopMoversWidget`, `StockQuoteWidget`,
`SectorHeatmapWidget`, `ScreenerWidget`, `RiskMetricsWidget`,
`PerformanceWidget`, `MarketSentimentWidget`, `PortfolioSummaryWidget`,
`QuickTradeWidget`, `MarketPulsePanel` (3 callsites inside this file).

Dashboard drivers — 2:
`DashboardScreen` (ticker refresh — drives `TickerBar::set_data`), and
`TickerBar` itself (receives data passively; only the driver needs to
change).

Screens — 4:
`MarketPanel` (`src/screens/markets/MarketPanel.cpp:198`),
`WatchlistScreen` (`src/screens/watchlist/WatchlistScreen.cpp:403`),
`PortfolioBlotter` (`src/screens/portfolio/PortfolioBlotter.cpp`),
`ReportBuilderScreen` (`src/screens/report_builder/ReportBuilderScreen.cpp:399`).

### Per-consumer changes
- Delete local `QTimer` (keep any `showEvent`/`hideEvent` pattern — hub handles idle-when-hidden via subscriber count).
- Replace `fetch_quotes(cb)` callback with `hub.subscribe(this, topic, slot)`.
- Delete any manual `CacheManager` lookups — hub covers that.

### Additional topics
- Add `market:history:<sym>:<period>:<interval>` and `market:sparkline:<sym>` as producer patterns.
- Migrate `fetch_history` and `fetch_sparklines` callers (charts, mini-graphs).

### Deliverables
- Zero `fetch_quotes(callback)` callsites outside `MarketDataService` internal wrapper.
- `MarketDataService::fetch_quotes` marked `[[deprecated]]` (wrapper still works).
- Update `CLAUDE.md` rule P5 to reference DataHub instead of batched calls.

### Success check
- Switching between Dashboard, Markets, Watchlist tabs is instant (cached).
- Python spawn count on hot path drops measurably (target: >60% reduction vs. baseline).
- Hub inspector shows expected subscriber counts matching visible UI.

### Risk
- Large diff surface — split into sub-PRs by screen group (dashboard widgets, markets, watchlist, portfolio, report builder).
- Missing a refresh trigger (e.g. user-triggered "refresh now" button) — add `DataHub::request(topic)` callsites where needed.

---

## Phase 4 — WebSocket Producers

**Goal:** unify real-time streams under the hub.

### Targets
- `trading/ExchangeService` — Kraken + HyperLiquid WebSocket feeds → `ws:kraken:*`, `ws:hyperliquid:*`.
- `services/polymarket/PolymarketWebSocket` → `polymarket:orderbook:<market>`, `polymarket:trades:<market>`.

### Per-producer changes
- Implement `Producer` interface; declare `push_only=true` policy.
- On WS message: `hub.publish(topic, value)` instead of emitting bespoke signals.
- On `on_topic_idle(topic)`: close/unsubscribe that market-specific WS channel (ref-counted — only close when all subscribers gone).
- On first subscriber: open WS channel lazily.

### Deliverables
- CryptoTradingScreen, CryptoWidget, MCP `CryptoTradingTools`, agents — all subscribe via hub.
- Polymarket screens subscribe via hub.
- Kraken/HyperLiquid WS opens only when ≥1 subscriber exists.

### Success check
- WS connection count = number of distinct exchanges with ≥1 subscriber (not per-screen).
- Every tick visible in both dashboard crypto widget and crypto trading screen with no duplicate parsing.

### Risk
- Lazy WS open/close logic must be ref-counted — covered by Producer test harness.
- Backpressure on high-frequency feeds (Kraken can emit 100+/sec) — hub dispatcher must coalesce or drop stale updates. Add a `coalesce_within_ms` field to TopicPolicy if needed.

---

## Phase 5 — News

**Goal:** one fetch per news query, broadcast everywhere.

### Targets
- `NewsService` — currently has mixed signals + callbacks.
- Consumers: Dashboard `NewsWidget`, News screens (8 components), Equity Research news tab, MarketPulsePanel headlines.

### Topics
- `news:general` — TTL 5 min
- `news:symbol:<sym>` — TTL 5 min
- `news:category:<cat>` — TTL 5 min
- `news:cluster:<id>` — push on cluster change

### Deliverables
- `NewsService` implements `Producer`.
- Keep existing article struct as `QVariant` payload.
- Subscribers migrated; old callback API deprecated.

### Risk
- News has some progressive-load semantics (`fetch_all_news_progressive`) — may need a `partial` publish pattern (multiple publishes per topic). Document in architecture §11 if used.

---

## Phase 6 — Economics & DBnomics

**Goal:** long-TTL series data through the hub.

### Targets
- `EconomicsService`, `DBnomicsService`, `GovDataService` (partially — gov data overlaps with geopolitics).

### Topics
- `econ:<source>:<series_id>` — TTL 1 h (series update daily at most)
- `econ:batch:<source>:<query_hash>` — for multi-series queries

### Deliverables
- Producers implemented.
- Economics tab + per-source panels (already refactored, per memory) → subscribers only.
- AkShare, DBnomics screens migrated.

### Risk
- Long-TTL + user-triggered refresh — `request(topic)` forces refresh bypassing `min_interval` (needs `force=true` overload).

---

## Phase 7 — Broker Account Streams

**Goal:** unified live account data from 16 broker integrations.

### Targets
Per-broker subdirectories under `src/trading/brokers/`:
Zerodha, Angel One, Upstox, Fyers, Dhan, Groww, Kotak, IIFL, 5paisa,
AliceBlue, Shoonya, Motilal, IBKR, Alpaca, Tradier, Saxo
(verified against the current directory listing — 16 broker adapters).

### Topics
- `broker:<id>:positions` — push when broker WS pushes position update, poll fallback 5 s
- `broker:<id>:orders` — same pattern
- `broker:<id>:balance` — TTL 30 s
- `broker:<id>:ticks:<sym>` — push-only for broker WS feeds (Zerodha Kite, Angel SmartStream)

### Deliverables
- Base `BrokerProducer` mixin — slots into `BrokerInterface` without breaking existing adapter contracts.
- `AccountDataStream` (per memory: multi-account architecture) refactored to publish to hub per-account.
- Equity trading screen, portfolio, multi-account selector → subscribers.

### Risk
- Broker SDKs each have different threading models. Each adapter's publish path must marshal to hub thread via queued connection — already covered by hub thread-safety guarantees.
- Per-account isolation: topics include broker ID so two accounts on same broker don't collide. If multi-account is per-user, topic may extend to `broker:<id>:<account_id>:...`.

---

## Phase 8 — Geopolitics / Maritime / Gov Data

**Goal:** remaining data services.

### Targets
`GeopoliticsService`, `MaritimeService`, `GovDataService`, `RelationshipMapService`, `MAAnalyticsService`.

### Topics
- `geopolitics:hdx:<dataset>`
- `geopolitics:relationship_graph:<entity>`
- `maritime:vessel:<imo>`
- `govdata:<country>:<dataset>`
- `ma:deals:<filter_hash>`

### Risk
- Low — these are long-TTL, relatively cold data sources. Pattern is well-established by Phases 5–6.

---

## Phase 9 — AI / MCP / Agents Integration

**Goal:** the AI and agent surface can read hub topics the same way screens do.

### Targets
- MCP tool modules in `src/mcp/tools/` (24 modules) — replace direct service calls with `DataHub::peek()` / `subscribe()` where appropriate.
- `AgentService` — agents can subscribe to market/news/broker topics to get context without bespoke service plumbing.
- `LlmService` / AI Chat — can reference `hub.stats()` and peek topics for tool-use responses.

### Deliverables
- MCP `datahub_peek`, `datahub_subscribe`, `datahub_list_topics` tool module (new).
- Agent framework helper: `AgentContext::peek(topic)` wraps `hub.peek()`.
- Agent configs that previously embedded `fetch_quotes` calls replaced with topic subscriptions.

### Risk
- Agents running in background threads must use `peek()` or subscribe with queued connection — document in agent contributor guide.

---

## Phase 10 — Enforcement & Cleanup  ✅ DONE (2026-04-18)

**Goal:** prevent regression.

### Deliverables
- **CLAUDE.md updates:** ✅ rules D1–D5 added (`fincept-qt/CLAUDE.md` §D). D4 was refined after audit to distinguish streaming data (hub-only) from one-shot catalog/info lookups (callback APIs may stay).
- **Lint / CI rule:** ✅ `datahub-discipline` job added to `.github/workflows/lint.yml`. Fails on `src/screens/**/*.cpp` containing `PythonRunner::instance().run(` (D1) or `(MarketData|News|Economics|DBnomics|GovData)Service::instance().fetch_` (D4).
- **Delete deprecated APIs:** ✅ compile-time-disabled legacy branches removed across 60 files (1043 lines) by stripping `#ifdef FINCEPT_DATAHUB_ENABLED` guards. Streaming callback APIs (`fetch_quotes`, `fetch_sparklines`, `fetch_history`) remain as one-shot internal-use methods; the hub is the only supported consumer path.
- **`FINCEPT_DATAHUB_ENABLED` CMake flag** removed from `CMakeLists.txt`. DataHub is the only supported data path.
- **docs/DATAHUB_TOPICS.md** — ✅ full topic registry for Phases 2–9 (markets, ws, news, econ, dbnomics, govdata, broker, geopolitics, maritime, M&A, agents, LLM sessions).
- **Inspector screen** ✅ promoted to Settings → Developer section (`SettingsScreen::build_developer()`).

### Success check
- `git grep PythonRunner::instance().run src/screens` — clean.
- `git grep FINCEPT_DATAHUB_ENABLED src/` — only documentation comments remain (no build-gating).
- CI rule in place and green.
- Topic registry doc matches `hub.stats()` output at runtime.

---

## Rollback Strategy

Every phase is reversible until Phase 10. If any phase causes a regression in production:

1. **Phases 2–9:** consumer migrations are local — revert the affected widget/screen commits. Producer keeps working, no data loss.
2. **Phase 4 (WebSocket):** if hub push-only coalescing causes tick drops, revert to direct-signal path; producers can emit both hub `publish()` and their legacy signals in parallel during transition.
3. **Phase 1 (hub itself):** controlled by `FINCEPT_DATAHUB_ENABLED` compile flag. If a critical bug surfaces before Phase 2 ships, flip flag off — nothing depends on it yet.
4. **Phase 10:** irreversible (deletes deprecated APIs). Only run after 2+ weeks of stable Phase 9.

---

## Milestones & Target Dates

Dates are indicative; adjust based on actual velocity. Treat as relative ordering, not hard commitments.

| Phase | Nominal duration | Cumulative |
|-------|------------------|------------|
| 0     | 1 day            | 1 day      |
| 1     | 3–4 days         | ~1 week    |
| 2     | 1–2 days         | ~1.5 weeks |
| 3     | 4–6 days         | ~2.5 weeks |
| 4     | 3–4 days         | ~3 weeks   |
| 5     | 3 days           | ~3.5 weeks |
| 6     | 2–3 days         | ~4 weeks   |
| 7     | 1–2 weeks        | ~5.5 weeks |
| 8     | 3 days           | ~6 weeks   |
| 9     | 1 week           | ~7 weeks   |
| 10    | 2 days           | ~7.5 weeks |

---

## Locked Decisions (pre-Phase 0)

1. **Topic key format** — `domain:subdomain:id[:modifier]`.
2. **Typed `subscribe<T>()` API** — ships in Phase 1 alongside the `QVariant` variant.
3. **Inspector screen** — dev-only under `FINCEPT_DATAHUB_ENABLED` for Phases 1–9. Promoted to user-visible Developer-mode tab in Phase 10.
4. **Persistence on restart** — hub starts cold. `CacheManager` still persists for warm restart within TTL. `rehydrate_on_start` becomes an opt-in per-topic policy flag, added when the first long-TTL producer asks for it.
5. **Rate limits** — hub-level. `Producer::max_requests_per_sec()` returns a producer-level cap; scheduler paces refresh calls accordingly.

See `DATAHUB_ARCHITECTURE.md` §15 for full reasoning.
