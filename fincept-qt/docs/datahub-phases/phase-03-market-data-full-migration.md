# Phase 3 — Market Data Full Migration

**Status:** Not started
**Depends on:** Phase 2 (pilot working in production for ≥1 week)
**Nominal duration:** 4–6 days
**Rough size:** Large (17 consumer callsites + 2 new topic families)

---

## Goal

Move **every** market-data consumer off the legacy
`MarketDataService::fetch_quotes(callback)` path and onto
`DataHub::subscribe(...)`. Delete per-widget refresh timers. Add history
and sparkline topic families so those fetch paths are covered too.

End state of this phase: `MarketDataService` has exactly one public entry
point into Python (the Producer's `refresh()`), every visible consumer
gets its quotes by subscribing to the hub, and `fetch_quotes(callback)`
is marked `[[deprecated]]` — the wrapper stays alive until Phase 10.

---

## Consumers to migrate

Counts below come from `grep -rn fetch_quotes src/screens/` executed
against the current tree. Phase 2 already migrated `QuoteTableWidget`
(which backs four dashboard factories: Indices, Crypto, Forex,
Commodities), so they are not re-listed here.

### Dashboard widgets — 11

| Widget                    | File                                                                    |
|---------------------------|-------------------------------------------------------------------------|
| `WatchlistWidget`         | `src/screens/dashboard/widgets/WatchlistWidget.cpp`                     |
| `TopMoversWidget`         | `src/screens/dashboard/widgets/TopMoversWidget.cpp`                     |
| `StockQuoteWidget`        | `src/screens/dashboard/widgets/StockQuoteWidget.cpp`                    |
| `SectorHeatmapWidget`     | `src/screens/dashboard/widgets/SectorHeatmapWidget.cpp`                 |
| `ScreenerWidget`          | `src/screens/dashboard/widgets/ScreenerWidget.cpp`                      |
| `RiskMetricsWidget`       | `src/screens/dashboard/widgets/RiskMetricsWidget.cpp`                   |
| `PerformanceWidget`       | `src/screens/dashboard/widgets/PerformanceWidget.cpp`                   |
| `MarketSentimentWidget`   | `src/screens/dashboard/widgets/MarketSentimentWidget.cpp`               |
| `PortfolioSummaryWidget`  | `src/screens/dashboard/widgets/PortfolioSummaryWidget.cpp`              |
| `QuickTradeWidget`        | `src/screens/dashboard/widgets/QuickTradeWidget.cpp`                    |
| `MarketPulsePanel`        | `src/screens/dashboard/widgets/MarketPulsePanel.cpp` (3 callsites)      |

### Dashboard drivers — 2

- `DashboardScreen` — ticker refresh loop. Replace with hub subscription
  to the ticker symbol set; push result into `TickerBar::set_data`.
- `TickerBar` — receives data passively; **no consumer change**. Listed
  for completeness only.

### Screens — 4

| Screen                  | File                                                                     |
|-------------------------|--------------------------------------------------------------------------|
| `MarketPanel`           | `src/screens/markets/MarketPanel.cpp:198`                                |
| `WatchlistScreen`       | `src/screens/watchlist/WatchlistScreen.cpp:403`                          |
| `PortfolioBlotter`      | `src/screens/portfolio/PortfolioBlotter.cpp`                             |
| `ReportBuilderScreen`   | `src/screens/report_builder/ReportBuilderScreen.cpp:399`                 |

---

## Deliverables

### Per-consumer changes (identical pattern everywhere)
- Delete local `QTimer` + any `timer->start()` in ctor. The hub scheduler
  owns cadence. Keep existing `showEvent`/`hideEvent` blocks — they now
  attach/detach hub subscriptions (see pattern from Phase 2).
- Replace `MarketDataService::instance().fetch_quotes(symbols, cb)` with:
  ```cpp
  for (const auto& sym : symbols)
      hub.subscribe(this, "market:quote:" + sym,
                    [this, sym](const QVariant& v) {
                        update_row(sym, v.value<QuoteData>());
                    });
  ```
- Delete any direct `CacheManager::get/put` calls for quotes — the hub
  already serves cached values (via `peek()` behaviour on subscribe).
- For consumers with a dynamic symbol set (watchlist, screener),
  unsubscribe the old set before subscribing to the new one:
  `hub.unsubscribe_all(this)` then resubscribe. Cheap — subscription
  registry is a hash table.

### New topic families
- `market:history:<sym>:<period>:<interval>` — e.g.
  `market:history:AAPL:1y:1d`. TTL 60s for intraday intervals, 1h for
  daily+. Policy configured via `set_policy_pattern` at service init.
- `market:sparkline:<sym>` — shorter-period mini-series. TTL 30 s.
  Policy: `min_interval_ms = 5000`.
- `MarketDataService::refresh()` handler dispatches by topic prefix:
  `market:quote:*` → existing batched quote fetch;
  `market:history:*` → extract `sym/period/interval`, call history
  Python script; `market:sparkline:*` → call sparkline script.

### Deprecate legacy API
- `[[deprecated("Subscribe to market:quote:<sym> via DataHub instead")]]`
  on `fetch_quotes(callback)`, `fetch_history(callback)`,
  `fetch_sparklines(callback)`.
- Wrappers still functional (implemented on top of hub) so any stragglers
  keep working. Deletion happens in Phase 10.

### Documentation
- Update `CLAUDE.md` rule **P5** to reference `DataHub` as the single
  entry point for market data, instead of the existing batched-calls
  language. Add a pointer to `DATAHUB_ARCHITECTURE.md`.
- New rows in `docs/DATAHUB_TOPICS.md` (created in Phase 10 — for this
  phase, stage the content in a comment block at the bottom of
  `MarketDataService.cpp`).

### Sub-PR split
To keep reviews tractable (the raw diff is ~40 files), land in five PRs:
1. Dashboard widgets batch A (5 widgets).
2. Dashboard widgets batch B (6 widgets).
3. Dashboard drivers (`DashboardScreen` + ticker).
4. Markets + Watchlist screens.
5. Portfolio + Report Builder + topic families for history/sparkline +
   `[[deprecated]]` annotations.

---

## Success check

- `git grep -l fetch_quotes src/screens/` returns **zero** entries.
- `git grep -l fetch_history src/screens/` returns **zero** entries.
- `git grep -l fetch_sparklines src/screens/` returns **zero** entries.
- Switching tabs Dashboard → Markets → Watchlist is visibly instant on a
  warm cache (< 100 ms first paint).
- Python spawn count for the market-data hot path drops **≥ 60 %** vs.
  the pre-Phase-2 baseline. Measure by counting
  `LOG_INFO("PythonRunner", "spawn...")` lines over a 5-minute session
  with Dashboard, Markets, and Watchlist all visited.
- `DataHubInspector` topic list shows subscriber counts that track the
  visible UI — hiding a widget drops its symbol's subscriber count;
  showing restores it.
- User-triggered refresh (context menu, F5) works on every consumer —
  re-verify after each sub-PR.

---

## Risk

- **Large diff surface.** Mitigation: sub-PR split above; each sub-PR is
  independently reversible.
- **Missing a refresh trigger.** Some widgets have "Refresh now" buttons
  that used to call `fetch_quotes` with an empty cache. Audit each
  migrated widget for manual-refresh affordances; wire them to
  `DataHub::request(topic)` which bypasses `min_interval`.
- **Dynamic symbol sets (watchlist, screener).** If `unsubscribe_all` is
  called mid-refresh, an in-flight result may arrive after resubscribe
  with the new set, causing a stale row. Mitigation: subscribe slot must
  verify the symbol is still in the current active set before updating.
- **Portfolio blotter edge case.** `PortfolioBlotter` mixes quote data
  with position data (which comes from broker APIs, not
  `MarketDataService`). Only the quote-lookup path changes here —
  position data stays on its existing service until Phase 7.
- **Ticker symbol churn.** `DashboardScreen` ticker rotates through a
  large symbol list on a schedule. Subscribing to all at once is fine
  (hub will batch), but verify the producer's `max_requests_per_sec`
  gate does not starve visible widgets. If it does, bump the cap or
  split ticker into its own topic family with a different policy.

---

## Rollback

- **Per sub-PR.** If batch N breaks, revert that PR; earlier batches and
  the legacy wrapper continue to work — the hub and `fetch_quotes`
  coexist by design.
- **Emergency full rollback:** revert all five PRs; `MarketDataService`
  stays registered as a Producer with zero subscribers, which is inert.
- **Irreversible steps:** none in this phase. Legacy API is only
  *marked* deprecated, not removed. Removal is Phase 10.

---

## Out of scope

- WebSocket tick feeds (Kraken, HyperLiquid, broker streams) — Phase 4
  and Phase 7.
- News, economics, geopolitics — Phases 5, 6, 8.
- Deleting the `fetch_quotes` / `fetch_history` / `fetch_sparklines`
  wrappers — Phase 10.
- CI / lint rule to block re-introduction of `fetch_quotes(...)` in
  screens — Phase 10.
