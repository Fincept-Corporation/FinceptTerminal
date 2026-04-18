# Phase 5 — News

**Status:** Not started
**Depends on:** Phase 1 (hub primitive). Independent of Phases 3/4.
**Nominal duration:** 3 days
**Rough size:** Medium (1 producer, 7 consumers, progressive-load pattern)

---

## Goal

One fetch per news query — broadcast to every visible news surface.
Today `NewsService` has a mix of Qt signals, direct callbacks, and
`fetch_all_news_progressive` which emits results in chunks. All of
this collapses into hub topics keyed by query intent: general feed,
per-symbol, per-category, per-cluster.

The **progressive-load** pattern — where a single query emits N
partial result chunks before the final "done" marker — is the novel
thing in this phase. The hub does not have a first-class streaming
concept; we handle it by publishing multiple times to the same topic
with a monotonic payload that the consumer knows how to accumulate or
replace.

---

## Targets

### Producer

- `NewsService` (`src/services/news/NewsService.{h,cpp}`). It already
  owns an internal TTL cache; preserve that — the hub's cache serves
  latest-only per topic, but the service's own query cache covers
  history lookups the hub isn't designed for (e.g. "show articles
  between date X and Y").

### Consumers — 7 confirmed via `grep -rn NewsService src/screens/`

| Consumer                  | File |
|---------------------------|------|
| `NewsScreen`              | `src/screens/news/NewsScreen.{h,cpp}` |
| `NewsFeedPanel`           | `src/screens/news/NewsFeedPanel.{h,cpp}` |
| `NewsFeedModel`           | `src/screens/news/NewsFeedModel.{h,cpp}` |
| `NewsFeedDelegate`        | `src/screens/news/NewsFeedDelegate.{h,cpp}` (read-only, indirect via model) |
| `NewsDetailPanel`         | `src/screens/news/NewsDetailPanel.{h,cpp}` |
| `NewsSidePanel`           | `src/screens/news/NewsSidePanel.{h,cpp}` |
| `NewsTickerStrip`         | `src/screens/news/NewsTickerStrip.{h,cpp}` |

Plus non-news-screen consumers:
- Dashboard `NewsWidget` (`src/screens/dashboard/widgets/NewsWidget.{h,cpp}`).
- Equity Research news tab (`src/screens/equity_research/...`).
- `MarketPulsePanel` headlines strip (already touched in Phase 3; the
  news portion moves here).

---

## Deliverables

### Topic families

- `news:general` — TTL 5 min, `min_interval_ms` 30 s.
- `news:symbol:<sym>` — TTL 5 min.
- `news:category:<cat>` — TTL 5 min. `<cat>` is one of the existing
  category enum strings (markets, macro, tech, crypto, etc.).
- `news:cluster:<id>` — push-only. Emitted when server-side clustering
  promotes an article set. No scheduled refresh; driven by cluster
  change events.

Payload: `QVariant::fromValue(QList<NewsArticle>)`. `NewsArticle` was
`Q_DECLARE_METATYPE`d in Phase 0, so this works as-is.

### Progressive publish pattern

`fetch_all_news_progressive` currently emits a signal for each chunk.
Translate to hub semantics:

- The first chunk publishes the partial list as the payload.
- Subsequent chunks publish an **accumulated** list (not just the new
  items) — consumers replace their rendered list each time. Cheap:
  NewsArticle lists are in the hundreds, not millions.
- The producer tracks an in-progress set per topic and stops
  publishing when the query finishes. It does **not** emit a
  sentinel "done" value — consumers just see the final list.
- Alternative considered: wrap payload in a `{list, is_final}` struct.
  Rejected: most consumers don't care, and those that do can compare
  payloads to detect a stable list.

Document this pattern in `DATAHUB_ARCHITECTURE.md §11` as "progressive
publish" — add it as part of this phase.

### Producer conversion
- `NewsService` implements `Producer`:
  - `topic_patterns()` → `{"news:general", "news:symbol:*", "news:category:*", "news:cluster:*"}`
  - `refresh(topics)` — splits by prefix, routes to existing fetch
    paths (`fetch_general`, `fetch_by_symbol`, `fetch_by_category`).
    Reuses the existing in-service cache.
  - `on_topic_idle` — no-op; news fetches are stateless HTTP.
- Registers itself at app startup.
- Publishes to hub in addition to legacy `articlesReceived(...)`
  signals for one release. Consumer migration follows.

### Consumer migration

All seven news components + dashboard NewsWidget + equity research
news tab switch to hub subscription. Pattern:

```cpp
hub.subscribe(this, "news:general",
              [this](const QVariant& v) {
                  auto list = v.value<QList<NewsArticle>>();
                  model_->replace(list);
              });
```

For symbol-scoped components (`NewsSidePanel` when viewing AAPL):
subscribe to `news:symbol:AAPL`; call `unsubscribe_all(this)` +
resubscribe when the active symbol changes.

### Deprecate legacy API
- Mark `NewsService::fetch_general(callback)`,
  `fetch_by_symbol(callback)`, `fetch_by_category(callback)`,
  `fetch_all_news_progressive(callback)` with `[[deprecated]]`.
  Wrappers still work on top of the hub.
- Legacy `articlesReceived` signal stays but is no longer connected
  after migration; delete in Phase 10.

---

## Success check

- Open the news screen, dashboard news widget, and equity research
  news tab simultaneously, all showing the general feed.
  `hub.stats()` shows exactly one `news:general` topic, subscriber
  count = 3. Exactly one HTTP request per refresh interval is logged.
- Symbol-scoped components: navigate AAPL → TSLA → MSFT rapidly.
  Verify no memory leak in subscription registry (subscriber counts
  per symbol return to 0 after move-away). `DataHubInspector` shows
  clean state.
- Progressive load: on a cold cache, visible feed "fills in" in
  chunks as HTTP pages arrive — matches current UX. All three
  consumers update in lockstep.
- User refresh (pull-to-refresh / F5) calls
  `hub.request("news:general")` and bypasses `min_interval`.
- `git grep -l fetch_all_news_progressive src/screens/` returns zero
  entries.

---

## Risk

- **Progressive publish can churn the UI.** If chunks arrive every
  200 ms and each triggers a full list replace on a 500-row model,
  the UI repaints hard. Mitigation: set `coalesce_within_ms = 250` on
  `news:general` and `news:symbol:*`, which the hub implements once
  Phase 4 lands that policy field. If Phase 4 hasn't merged yet, add
  an internal coalescing timer inside the news producer.
- **Double-fire during migration.** Consumers connected to both hub
  and legacy signal render duplicates. Mitigation: per-component
  migration commit explicitly disconnects the legacy `connect(...)`.
- **Stale cluster assignments.** Cluster IDs are server-assigned and
  can change. If a consumer caches `news:cluster:123` and the server
  renumbers, the topic goes stale. Mitigation: on cluster
  renumbering event (the existing server push), publish an empty
  article list to the old cluster topic, then publish to the new.
- **Equity Research news tab reads via its own composite query.**
  Check whether it actually goes through `NewsService` or has its
  own fetcher — if separate, this migration skips it and it's picked
  up in Phase 9 or left alone.

---

## Rollback

- Revert per-consumer commit — legacy signal path still works, the
  service still emits it during the transition window.
- Revert the producer registration call if the service itself
  misbehaves; consumer subscriptions become silent but do not crash.
- `progressive publish` doc note is additive — no rollback needed.

---

## Out of scope

- News classification / clustering pipeline changes — pure
  subscription work.
- Localisation / translation of article payloads — unchanged.
- MCP news tool modules — Phase 9.
- Legacy signal removal — Phase 10.
