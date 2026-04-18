# Phase 10 — Enforcement & Cleanup

**Status:** Not started
**Depends on:** All prior phases (0–9) shipped and stable in production
for ≥ 2 weeks.
**Nominal duration:** 2 days
**Rough size:** Small. But **irreversible** — this is the only phase
that deletes code. All prior phases are revertible; this one closes
the door.

---

## Goal

Prevent regression. After nine phases of additive migration, every
consumer has been converted, every producer is on the hub, legacy
callback APIs have been `[[deprecated]]` for months. This phase
removes the safety net: the deprecated wrappers come out, lint
rules prevent re-introduction, and the hub becomes the only
supported way to fetch data across the terminal.

No new features. Just locking the door behind the migration.

---

## Deliverables

### 1. `CLAUDE.md` rule additions

Add these under a new section **D. DataHub Rules (MANDATORY)**
between the existing `P15` and `Security` sections:

- **D1.** Screens and widgets must not call `PythonRunner` directly.
  All Python-backed data flows through a Producer registered with
  `DataHub`.
- **D2.** Services that fetch external data (HTTP, Python, WS)
  must implement `fincept::datahub::Producer` and publish results
  via `DataHub::publish(...)`.
- **D3.** Widgets never own a data-refresh `QTimer`. The hub
  scheduler owns cadence. Widgets subscribe in `showEvent`,
  unsubscribe in `hideEvent` (extends existing P3).
- **D4.** Consumer code must never call a service's `fetch_*`
  method. All reads go through `DataHub::subscribe(...)` or
  `DataHub::peek(...)`.
- **D5.** New topic strings must be registered in
  `docs/DATAHUB_TOPICS.md` (see deliverable 4) with TTL, owner,
  and policy before first use.

Cross-reference: update **P5** (existing) to explicitly reference
the hub and point to `DATAHUB_ARCHITECTURE.md`.

### 2. Lint / CI rule

Add a shell or Python check to CI (`.github/workflows/ci.yml`)
that fails the build if either of these grep patterns returns a
match inside `fincept-qt/src/screens/`:

```bash
# Screens must not spawn Python directly.
rg -t cpp 'PythonRunner::instance\(\).run\(' fincept-qt/src/screens/ && exit 1

# Screens must not call the deprecated fetch_* APIs.
rg -t cpp '(MarketDataService|NewsService|EconomicsService)::instance\(\)\.fetch_' \
   fincept-qt/src/screens/ && exit 1

exit 0
```

Add the check as a dedicated CI step named `datahub-discipline`.
Failing output includes the offending file and one-line
remediation pointing to `DATAHUB_ARCHITECTURE.md` §4.

### 3. Delete deprecated APIs

After confirming the lint rule catches new violations, delete:

- `MarketDataService::fetch_quotes(callback)`,
  `fetch_history(callback)`,
  `fetch_sparklines(callback)` — wrappers introduced in Phase 2/3.
- `NewsService::fetch_general(callback)`,
  `fetch_by_symbol(callback)`,
  `fetch_by_category(callback)`,
  `fetch_all_news_progressive(callback)` — Phase 5.
- `EconomicsService::fetch_series(callback)`,
  `fetch_batch(callback)` — Phase 6.
- `GeopoliticsService`, `MaritimeService`, `GovDataService`,
  `RelationshipMapService`, `MAAnalyticsService` callback fetch
  wrappers — Phase 8.
- Legacy Qt signals: `ExchangeService::tickReceived(...)`,
  `PolymarketWebSocket::orderBookUpdate(...)`, and
  `AccountDataStream::positionsUpdated(...)` / friends — Phases
  4 and 7.

Also delete the pass-through `AccountDataStream` signal layer if
no consumer connects to it any more.

For each deletion: verify with `git grep` that zero callers
remain (they should all have been migrated by Phase 9), remove
the declaration from the `.h`, remove the body from the `.cpp`,
run the build on all three presets, verify test suite green.

### 4. `docs/DATAHUB_TOPICS.md` — the registry

One markdown doc, authoritative for all topic strings in the
terminal. Columns:

| Topic pattern | Producer | TTL | min_interval | push-only | Notes |

Every topic introduced across Phases 2–9 gets a row. A small
number of examples below; the final doc fills in the complete set.

```
market:quote:<sym>                      MarketDataService     30 s     5 s     no   batched
market:history:<sym>:<period>:<interval> MarketDataService    1 h      30 s    no   —
ws:kraken:ticker:<pair>                 ExchangeService       —        —       yes  coalesce 50 ms
broker:<id>:<acct>:positions            BrokerProducer        5 s      2 s     no   —
econ:fred:<series_id>                   EconomicsService      1 h      1 min   no   warm-start subset
news:general                            NewsService           5 min    30 s    no   progressive publish
agent:<id>:stream:<run>                 AgentService          —        —       yes  coalesce 100 ms
```

### 5. Inspector promotion

`DataHubInspector` (built in Phase 1 under the dev flag) becomes a
user-visible screen in Settings → **Developer Mode** → **DataHub
Inspector**. Behind the existing "Developer mode" setting toggle
(which users flip on themselves; not exposed by default). No
change to the inspector code itself; the Phase 1 implementation
is production-ready.

Remove the `#ifdef FINCEPT_DATAHUB_ENABLED` guard from the
inspector's inclusion in `SCREEN_SOURCES`; the feature flag no
longer gates anything (hub is now the only data path).

### 6. Flag removal

Remove `FINCEPT_DATAHUB_ENABLED` CMake option. The hub is no
longer optional. Delete the flag from `CMakeLists.txt:123`, all
its `#ifdef FINCEPT_DATAHUB_ENABLED` blocks (roughly: `main.cpp`
metatype registration, any Producer registration calls), and
update `CMakePresets.json` if it references the flag.

### 7. Architecture & phases doc status

- Mark all phases 0–10 as ✅ DONE in `DATAHUB_PHASES.md`.
- Move `DATAHUB_ARCHITECTURE.md` status from **Proposal** to
  **Current**.
- Link the two documents from the top of `docs/ARCHITECTURE.md`.
- Delete per-phase planning docs (`docs/datahub-phases/phase-0*.md`)
  or move to `docs/datahub-phases/archive/` — the phases doc plus
  commit history is enough once the work has landed.

---

## Success check

- `git grep PythonRunner::instance\(\).run src/screens/` → zero.
- `git grep MarketDataService::instance\(\).fetch src/screens/` → zero.
- `git grep NewsService::instance\(\).fetch src/screens/` → zero.
- `git grep EconomicsService::instance\(\).fetch src/screens/` → zero.
- CI `datahub-discipline` step is green; intentionally add a
  sacrificial violation on a scratch branch and verify it fails
  the build.
- Build on all three presets (`win-release`, `linux-release`,
  `macos-release`) without the deprecated APIs — green.
- Full test suite passes (Qt Test `test_datahub` + any downstream
  suites added in Phases 2–9).
- Manual smoke test on a live build: every major screen visited,
  data rendering identical to the last pre-Phase-10 release.
- `docs/DATAHUB_TOPICS.md` table row count matches
  `hub.stats().size()` at runtime after opening every screen.
- `FINCEPT_DATAHUB_ENABLED` no longer appears anywhere in
  `git grep`.

---

## Risk

- **Irreversibility.** This is the whole point of Phase 10, and
  the biggest risk. Mitigation:
  - Only run Phase 10 after two weeks of stable production on
    Phase 9.
  - Commit each deletion as a separate small commit so any single
    regression can be reverted.
  - Keep one tagged release (`v-pre-phase10`) on hand for fast
    rollback to a world where the legacy wrappers still exist.
- **Hidden consumers.** Some third-party code (user plugins,
  scripts in `scripts/`) may still call the deprecated APIs.
  Mitigation: grep the `scripts/` tree too; keep APIs exposed
  through the Python bridge if necessary (they'd go through hub
  internally). The user-facing Python API surface doesn't change.
- **CI check false positives.** `rg` patterns may match comments
  or test fixtures. Mitigation: exclude `tests/**`, allow
  `// NOLINT(datahub-discipline)` comment override, document the
  override clearly so it isn't abused.
- **Agent / MCP regressions.** Phase 9 deletions may affect
  third-party MCP consumers that call deprecated service APIs
  directly. Mitigation: MCP tool surface doesn't expose service
  APIs; external MCP clients call tool names, implementation is
  internal. Low risk.

---

## Rollback

Rollback at Phase 10 means restoring the deprecated APIs from the
tagged pre-phase-10 commit. Ugly but tractable:

1. Check out `v-pre-phase10`.
2. Cherry-pick any bug fixes that landed post-Phase-10 but before
   the regression.
3. Redeploy.

If rollback is triggered by a subtle regression (e.g. an agent
script breaking), a more surgical path:

- Re-add just the specific deprecated method that broke, as a
  thin wrapper calling `DataHub::peek()` + `subscribe()`.
- Temporarily bypass the `datahub-discipline` CI rule for the
  affected file with a NOLINT comment.
- File a ticket to migrate the straggler and revisit.

Because Phase 10 is the *only* irreversible phase, the rollback
story is documented upfront and practiced once on a staging
branch before execution.

---

## Out of scope

- New hub features — none in this phase. Hub is feature-complete
  after Phase 9.
- Performance optimisations — hub has already been instrumented
  through Phases 2–9; any further tuning is post-rollout.
- New topic families — additions happen post-Phase-10 per the
  normal service addition process (with `DATAHUB_TOPICS.md`
  entry + D1–D5 compliance).
- Migration of `scripts/` Python code — those scripts are the
  producers' back ends, not consumers. They don't interact with
  the hub directly.
