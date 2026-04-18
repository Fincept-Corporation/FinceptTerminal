# Phase 6 — Economics & DBnomics

**Status:** Not started
**Depends on:** Phase 1. Independent of Phases 3/4/5.
**Nominal duration:** 2–3 days
**Rough size:** Medium (3 producers, 30+ per-source panels as subscribers)

---

## Goal

Put long-TTL economic series on the hub. Economics data moves slowly
(most FRED, World Bank, IMF series update daily at most; many
monthly), so the hub's caching + rate-limiting layer is a big win
here — multiple panels asking for the same series collapse to one
remote fetch.

This phase also introduces the **force refresh** semantic: a user
clicking "refresh" on an economics panel must bypass `min_interval`.
Until now, `request()` respected the interval gate. Add a `force=true`
overload.

---

## Targets

### Producers

| Producer              | File                                                           | Notes |
|-----------------------|----------------------------------------------------------------|-------|
| `EconomicsService`    | `src/services/economics/EconomicsService.{h,cpp}`              | Dispatches to 32 per-source Python scripts |
| `DBnomicsService`     | `src/services/dbnomics/DBnomicsService.{h,cpp}`                | Thin wrapper over DBnomics REST |
| `GovDataService`      | `src/services/gov_data/GovDataService.{h,cpp}`                 | Country-level macro + statistics |

Memory: the economics tab was already refactored to per-source
panels (one `.h/.cpp` per provider), and `EconomicsService` is a
singleton. Perfect shape for Producer conversion — the service is
the single entry point already.

### Consumers

- **Economics tab panels** — 32 per-source files under
  `src/screens/economics/panels/` (FRED, World Bank, IMF, OECD,
  ECB, BIS, BLS, BEA, Census, SEC EDGAR, ONS UK, Eurostat, Bank of
  Japan, PBOC, RBI, etc. — verified 64 files = 32 panels × .h/.cpp).
- **DBnomics screen components** — 4 files under
  `src/screens/dbnomics/...`.
- **AkShare Data** and **Asia Markets** screens — use
  `EconomicsService` for some series.
- **Gov Data** screen — 3 panels under `src/screens/gov_data/...`.

---

## Deliverables

### Topic families

- `econ:<source>:<series_id>` — TTL **3600 s (1 h)**. Most economics
  series are daily-or-slower; 1 h TTL is generous but cheap.
  Example: `econ:fred:GDP`, `econ:worldbank:NY.GDP.MKTP.CD`.
- `econ:batch:<source>:<query_hash>` — for multi-series queries
  emitted by chart panels that plot e.g. GDP of 12 countries on the
  same axis. `<query_hash>` is a stable SHA-1 of the sorted
  series-ID list. TTL 1 h.
- `dbnomics:<provider>:<dataset>:<series>` — DBnomics uses three-part
  series keys (FR.CPI.TOT.IDX), so naturally maps to four topic
  segments.
- `govdata:<country>:<dataset>` — e.g. `govdata:IN:rbi_repo_rate`.
  TTL 1 h.

All topics: `push_only = false`, `min_interval_ms = 60_000`
(don't hit upstream more than once per minute even on force).

### Force refresh

Extend the hub:
```cpp
void DataHub::request(const QString& topic, bool force = false);
```
When `force` is true, the scheduler ignores `min_interval_ms` but
still honours per-producer `max_requests_per_sec()` — so a rage-click
on refresh can't hammer FRED. This is a small, contained addition to
the Phase 1 code.

### Producer conversion

- `EconomicsService` implements `Producer`:
  - `topic_patterns()` → `{"econ:fred:*", "econ:worldbank:*", "econ:imf:*",` *...one per source, 32 patterns total...* `, "econ:batch:*"}`.
  - `refresh(topics)` — splits by source segment, routes to the
    existing per-source dispatch table.
  - `max_requests_per_sec()` returns 2 (generous; upstream rate
    limits are usually much higher, but Python spawn overhead
    matters).
- `DBnomicsService` — same pattern, one pattern `"dbnomics:*"`.
- `GovDataService` — one pattern `"govdata:*"`.

### FRED / long-TTL warm-start

Economics users typically open the same 6–10 series every session
(GDP, CPI, unemployment, key central-bank rates). Set
`rehydrate_on_start = true` on these specific series in the policy.
The hub reads the last value from `CacheManager` at startup and
seeds `peek()` so panels show data immediately without blocking on
a network fetch.

List maintained in `src/services/economics/warm_topics.cpp` — an
explicit array of topic strings. Start with the 10 most-opened
series from telemetry (or a hard-coded reasonable list if no
telemetry yet).

### Consumer migration

Per-source panels today call `EconomicsService::instance().fetch_series(...)`.
Replace with:
```cpp
hub.subscribe(this, "econ:fred:" + series_id,
    [this](const QVariant& v) {
        auto series = v.value<EconomicsResult>();
        render_chart(series);
    });
```

For the batch topics (multi-series charts), the panel computes the
query hash and subscribes to `econ:batch:worldbank:<hash>`. The
service's `refresh()` handler recognises the pattern and runs the
existing batch query.

### Documentation

- `DATAHUB_ARCHITECTURE.md` gets a note under §4 about the `force`
  flag and its interaction with `min_interval_ms` vs.
  `max_requests_per_sec`.
- `docs/DATAHUB_TOPICS.md` stub (finalised in Phase 10) gets its
  first table: every economics topic family listed with TTL + owner.

---

## Success check

- Open a FRED panel (GDP) and a World Bank panel (same metric) in
  the same session. `hub.stats()` shows one `econ:fred:GDP` topic
  and one `econ:worldbank:NY.GDP.MKTP.CD` topic — each fetched
  once.
- Close and reopen the economics tab within 5 minutes: panels show
  data immediately from the hub cache, no Python spawn occurs.
- Kill the terminal, restart: warm-start topics pre-seed; the
  first render of GDP panel is within 200 ms (reads from
  `CacheManager`), a fresh fetch then runs in the background and
  updates if newer data exists.
- Click "refresh" on a panel: `request(topic, force=true)` fires; a
  new Python spawn is logged even if the TTL hasn't expired.
- `git grep -l EconomicsService::instance().fetch_series src/screens/`
  returns zero entries.

---

## Risk

- **32 per-source panels is a lot of touch points.** Mitigation: the
  panels share a common base class (per memory: Economics Tab
  refactor created a per-source panel architecture). Migrate the
  base class first — every panel inherits the new subscribe pattern
  for free. Only panels with custom fetch logic need individual
  attention.
- **Batch query hash must be stable.** Any reordering or
  case-sensitivity difference produces a different topic. Mitigation:
  canonicalise the series list in `EconomicsService` (sort + lower-case)
  before hashing, and do it in exactly one place.
- **Warm-start list goes stale.** If the listed series aren't the
  ones users actually look at now, warm-start wastes startup time
  on cold series. Mitigation: revisit the list every release based
  on hub inspector telemetry; keep it at ≤ 10 entries.
- **FRED API key gating.** Users without a FRED key fall through to
  a fallback path. Ensure the producer's `refresh()` routes this
  correctly — do not publish an error payload; publish nothing and
  let the consumer `peek()` show "no data" state.

---

## Rollback

- Per-consumer revert; legacy `fetch_series` API still wraps over
  the hub (same deprecation-with-wrapper pattern from Phases 2, 3, 5).
- If warm-start misbehaves (stale data rendered): remove
  `rehydrate_on_start = true` from the policy; panels fall back to
  "loading" state on startup. No data-correctness risk.
- If `force` flag causes unexpected upstream load: revert its usage
  in refresh buttons, leave the hub API intact (additive change, no
  hot-path cost if unused).

---

## Out of scope

- Economics chart library or visualisation code — the hub just
  delivers the `EconomicsResult`; rendering is unchanged.
- QuantLib Economics tab (different data flow — internal models,
  not external series) — not on the hub. Stays as-is.
- DBnomics provider authentication — no changes.
- MCP `economics_tools` module — Phase 9.
