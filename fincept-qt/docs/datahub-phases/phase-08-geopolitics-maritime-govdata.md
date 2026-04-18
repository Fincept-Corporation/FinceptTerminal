# Phase 8 — Geopolitics / Maritime / Gov Data

**Status:** Not started
**Depends on:** Phase 1. Pattern is the same as Phase 5/6 — no new hub
features needed.
**Nominal duration:** 3 days
**Rough size:** Medium (5 services, cold data, low risk)

---

## Goal

Finish the remaining data services — the cold, long-TTL data that
drives geopolitics, maritime tracking, gov data, relationship mapping,
and M&A analytics. These are the easiest wins in the whole rollout:
fetches are infrequent, consumers are few, the pattern is already
proven in Phases 5 and 6.

This phase intentionally has no novel machinery. It is rote
application of the Producer pattern across five services so Phase 10's
cleanup can claim "all data services are on the hub" without
exception.

---

## Targets

| Service                   | File                                                                    |
|---------------------------|-------------------------------------------------------------------------|
| `GeopoliticsService`      | `src/services/geopolitics/GeopoliticsService.{h,cpp}`                   |
| `MaritimeService`         | `src/services/maritime/MaritimeService.{h,cpp}`                         |
| `GovDataService`*         | `src/services/gov_data/GovDataService.{h,cpp}`                          |
| `RelationshipMapService`  | `src/services/relationship_map/RelationshipMapService.{h,cpp}`          |
| `MAAnalyticsService`      | `src/services/ma_analytics/MAAnalyticsService.{h,cpp}`                  |

\* `GovDataService` was *partially* addressed in Phase 6 under the
`govdata:*` topic family for macro series. This phase picks up the
rest (datasets, country reports, regulatory filings).

### Consumers
- **Geopolitics screen** — 4 panels under `src/screens/geopolitics/`.
- **Maritime screen** — vessel tracking, port activity.
- **RelationshipMap screen** — graph view.
- **M&A Analytics screen** — deals, comparables.
- **Gov Data screen** — 3 panels under `src/screens/gov_data/`.

Lower-frequency consumers than earlier phases; most screens have 1–3
subscribers each.

---

## Deliverables

### Topic families

- `geopolitics:hdx:<dataset>` — HDX Humanitarian Data Exchange
  datasets. TTL 1 h.
- `geopolitics:relationship_graph:<entity>` — graph slice rooted at
  an entity. TTL 30 min.
- `geopolitics:analysis:<scenario_id>` — scenario reports,
  published when an agent produces analysis. TTL 6 h (effectively
  static; produced once per session).
- `maritime:vessel:<imo>` — position, heading, speed. TTL 2 min
  (AIS updates are slow).
- `maritime:port:<unlocode>` — port activity. TTL 15 min.
- `govdata:<country>:<dataset>` — e.g. `govdata:US:federal_register`.
  TTL 1 h.
- `ma:deals:<filter_hash>` — M&A deals matching a filter. TTL 15 min.
- `ma:company:<ticker>` — per-company deal history. TTL 1 h.

All non-push-only, no new `TopicPolicy` fields needed.

### Producer conversion (× 5)

Identical pattern to Phase 6. Each service:
- Implements `Producer`.
- `topic_patterns()` lists its prefixes.
- `refresh(topics)` splits by prefix and routes to existing fetch
  methods — the services already have per-dataset dispatch tables.
- `max_requests_per_sec()` → 1. These are slow, expensive upstream
  calls (HDX can be 5–15 s per dataset; M&A filters hit
  proprietary aggregators).
- No `on_topic_idle` action — all stateless.

### Consumer migration

- Geopolitics panels (4) — replace direct service fetch calls with
  subscriptions to the relevant topics. Per-scenario reports
  (`geopolitics:analysis:*`) get their IDs from the scenario list
  service, which remains a signal-based dispatcher for control plane
  interactions (not data).
- Maritime screen — vessel list subscribes to `maritime:vessel:*`
  with a dynamic ID list. Port detail view subscribes to one
  specific `maritime:port:<ul>`.
- RelationshipMap screen — one subscription to
  `geopolitics:relationship_graph:<root_entity>`; changing the root
  triggers `unsubscribe_all` + resubscribe.
- M&A Analytics — filter panel builds the hash, subscribes to
  `ma:deals:<hash>`; company pages subscribe to `ma:company:<ticker>`.
- Gov Data — 3 panels subscribe to their respective country +
  dataset topics.

### Deprecation

All five services gain `[[deprecated]]` on their callback-based fetch
APIs, with wrappers delegating to the hub. Deletion in Phase 10.

---

## Success check

- Open geopolitics screen twice (two tabs), viewing the same
  scenario. `hub.stats()` shows one `geopolitics:hdx:<dataset>`
  topic, subscriber count = 2.
- Maritime: open vessel list + one vessel detail panel
  simultaneously. Detail panel's topic has subscriber count = 1;
  list's topic batch-subscribes the 20 on-screen vessels. Per-tick
  cost is cheap because TTL is long and publishing is rare.
- M&A filter: change the filter criteria; previous
  `ma:deals:<old_hash>` topic goes idle (subscriber count 0),
  producer logs `topic_idle`; new hash subscribes.
- `git grep -l GeopoliticsService::instance().fetch src/screens/`,
  `grep ... MaritimeService::`, etc. all return zero.
- No visible behaviour change from the user's perspective — all
  these screens were already cached on long TTLs; the hub
  migration is invisible to users but saves the duplicate fetches
  that happen when two panels viewed the same dataset.

---

## Risk

- **Low.** This is the tail of the migration. All patterns are
  established. The main risk is simple bugs from repetitive work —
  mitigated by reviewing each service migration independently.
- **HDX datasets are large** (sometimes > 5 MB JSON). Storing in
  the hub's value cache is fine (hub holds one copy); passing via
  `QVariant` is a reference-counted shared pointer under the hood
  so it doesn't copy on every subscriber notification. Confirm this
  with a memory-usage check for one large dataset.
- **M&A filter hash collisions** — unlikely (SHA-1 is overkill for
  this) but if two distinct filters produce the same hash somehow,
  subscribers cross-contaminate. Mitigation: include the filter
  source JSON in a debug log on first publication so mismatches can
  be spotted in the inspector.
- **Relationship graph rooted at dynamic entity** — if the active
  entity changes very fast (user hovering a node), rapid
  subscribe/unsubscribe cycles are fine (hub is cheap at these
  rates) but we may end up thrashing the upstream fetch.
  Mitigation: apply `min_interval_ms = 5000` on
  `geopolitics:relationship_graph:*` so rapid hover-switches
  coalesce to one fetch per entity per 5 seconds.

---

## Rollback

- Per-service revert. Each producer is independent; reverting one
  leaves the other four on the hub. No cross-service dependencies
  in this phase.
- Legacy callback wrappers remain; any straggler consumer still
  works.

---

## Out of scope

- `MAAnalyticsService`'s deal-comparison ML model — pure service
  internals, no hub involvement.
- Polymarket — already on the hub from Phase 4.
- HDX authentication & quota handling — unchanged.
- Agents consuming geopolitics topics — Phase 9.
- Legacy API deletion — Phase 10.
