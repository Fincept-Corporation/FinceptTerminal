# Phase 2 — Market Data Pilot

**Status:** Not started
**Depends on:** Phase 1 (DataHub primitive)
**Nominal duration:** 1–2 days
**Rough size:** Medium (one service + one widget base class)

---

## Goal

Prove the full DataHub pattern end-to-end on **one producer** (`MarketDataService`) and **one widget** (`QuoteTableWidget`) before migrating anything else.

The point is not breadth — it is to validate that a production data path actually works through the hub: batching, TTL, dedup, cross-thread publish, lifetime cleanup, inspector visibility. If anything in Phase 1's primitive is wrong, it shows up here before 20+ other consumers lock the design in.

`MarketDataService::fetch_quotes(callback)` **must continue to work unchanged** during this phase — every other consumer still uses it. The pilot widget is the only one rewired to the hub.

---

## Deliverables

### Producer conversion — `MarketDataService`
- Implements `fincept::datahub::Producer` interface:
  - `topic_patterns()` → `{"market:quote:*"}`
  - `max_requests_per_sec()` → returns the existing Python runner cadence
    cap so the hub scheduler cannot outpace it (derive from
    `PythonRunner::max_concurrent()` + historical batch size).
  - `refresh(const QStringList& topics)` — extract the symbol suffix from
    each `market:quote:<sym>` topic, dedupe into one batched Python call
    (the existing 100ms coalescing window — preserved inside `refresh()`),
    publish results per-symbol via
    `DataHub::publish("market:quote:" + sym, QVariant::fromValue(quote))`.
  - `on_topic_idle(topic)` — no-op for this producer (stateless REST
    fetch; nothing to close).
- Registers itself with the hub at app-startup:
  `DataHub::instance().register_producer(&MarketDataService::instance())`
  — gated on `FINCEPT_DATAHUB_ENABLED`.
- Registers default topic policy:
  ```cpp
  TopicPolicy p;
  p.ttl_ms = 30'000;
  p.min_interval_ms = 5'000;
  DataHub::instance().set_policy_pattern("market:quote:*", p);
  ```

### Backward-compat wrapper
- `MarketDataService::fetch_quotes(symbols, callback)` stays — reimplemented
  on top of the hub: subscribes to each `market:quote:<sym>` on a throwaway
  `QObject` owner, captures the values until all symbols arrive, fires the
  callback once, unsubscribes by deleting the owner via `deleteLater()`.
- This guarantees Phase 2 is a no-op for all 20+ existing callers — they
  continue to work while Phase 3 migrates them.

### Pilot consumer — `QuoteTableWidget`
File: `src/screens/dashboard/widgets/QuoteTableWidget.cpp:36`.
This base class backs **four dashboard widgets** via factory functions
(`IndicesWidget`, `CryptoWidget`, `ForexWidget`, `CommoditiesWidget`), so one
migration exercises the hub across four visible surfaces simultaneously —
the right size to catch real-world issues without committing to full
migration.

Changes:
- Delete the `refresh_data()` callback-based fetch path.
- In ctor: after the symbol list is known, for each symbol call
  `DataHub::instance().subscribe(this, "market:quote:" + sym, slot)`
  where `slot` routes to the existing `update_row_for_symbol(sym, quote)`
  helper.
- Delete the local `QTimer` and any timer-start in ctor. The hub now owns
  refresh cadence; visibility-driven behaviour is handled by hub through
  subscriber count (subscriber count drops to 0 when widget hides because
  we subscribe/unsubscribe in `showEvent`/`hideEvent`).
- `showEvent()` subscribes; `hideEvent()` destroys the subscription owner
  (or calls `DataHub::unsubscribe_all(this)` — method exists from Phase 1).
  This preserves CLAUDE.md P3: hidden widgets stop consuming the producer.

### Startup wiring
- `MarketDataService::instance().ensure_registered_with_hub()` called from
  the same bootstrap block in `main.cpp` that currently calls
  `datahub::register_metatypes()`. Idempotent: safe if called twice.

### Instrumentation
- Add `LOG_INFO("DataHub", "refresh market:quote batch=%1")` inside the
  producer so we can count spawn events during the before/after baseline.

---

## Success check

- All four dashboard widgets (Indices, Crypto, Forex, Commodities) display
  identical data to the pre-migration baseline. Visual diff screenshot
  captured.
- `hub inspector` (`DataHubInspector` screen added in Phase 1) shows
  `market:quote:*` topics with `subscriber_count ≥ 1` matching the visible
  rows, `last_refresh` ticks on hub cadence not per-widget.
- Python spawn count measured by looking at `PythonRunner` log lines over
  a 60 s window: with four widgets visible, **hub path should produce ≤ 2
  spawns per minute** (one batched fetch every 30 s TTL), vs. the
  pre-migration baseline of ~8 spawns per minute (each widget's 15 s
  timer × four widgets, halved by cache hits).
- Dev shortcut: open the hub inspector, scroll to `market:quote:AAPL`,
  confirm `in_flight` flickers to true on a hub refresh then back to
  false within ~2 s.
- No duplicate Python spawns when other (legacy) `fetch_quotes` callers
  are also active on the same tick — the legacy path now routes through
  the hub wrapper, so the producer still sees one batched request.

---

## Risk

- **Regression in specific widgets** — Indices and Commodities in particular
  use slightly different symbol formatting. Mitigation: verify one symbol
  per category after migration, not just `AAPL`.
- **Hub cadence too slow for user-triggered refresh** (e.g. right-click
  → Refresh) — wire the existing context-menu action to
  `DataHub::request("market:quote:<sym>")` so it bypasses `min_interval`.
- **Subscriber owner lifetime** — `showEvent`/`hideEvent` unsubscribe pattern
  must not leak `QObject` owners. Verify by opening/closing the dashboard
  10× and confirming `hub.stats()` subscriber counts return to baseline.
- **Python spawn coalescing window (100ms) interacts badly with 1s hub
  tick** — if `refresh()` is called per-topic synchronously, coalescing
  window may close early. Mitigation: buffer topics inside `refresh()` into
  a `QTimer::singleShot(100ms, ...)` that then batches. (Same mechanism
  `MarketDataService` already uses — just move it inside `refresh()` from
  `fetch_quotes`.)

---

## Rollback

- **Trivial.** Revert the `QuoteTableWidget` commit; the four dashboard
  widgets fall back to the legacy `fetch_quotes` path (which continues to
  exist as the backward-compat wrapper).
- `MarketDataService` being a Producer is harmless if no one subscribes —
  the scheduler never calls `refresh()` on idle topics. Can be left
  registered with no consumers.
- No user-facing regression risk if rollback is done within the same
  release: the hub path and legacy path produce identical `QuoteData`
  structs.

---

## Out of scope

- Other market-data consumers (dashboard widgets beyond `QuoteTableWidget`,
  `MarketPanel`, `WatchlistScreen`, `PortfolioBlotter`, `ReportBuilderScreen`)
  — all of those are Phase 3.
- `fetch_history` and `fetch_sparklines` producer paths — Phase 3.
- WebSocket tick feeds for crypto — Phase 4.
- Deleting `fetch_quotes(callback)` public API — Phase 10.
