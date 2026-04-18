# DataHub Architecture

**Status:** Current (in production as of 2026-04-18; all phases 0–10 shipped)
**Version:** 1.0
**Owner:** Fincept Terminal Core
**Scope:** In-process pub/sub data layer for the entire terminal (markets, news, economics, broker streams, geopolitics, agents, WebSockets).

---

## 1. Problem

Every screen and widget in the terminal currently pulls its own data:

- `~20 dashboard widgets`, `MarketPanel`, `WatchlistScreen`, `PortfolioBlotter`, etc. each own a `QTimer` and call `MarketDataService::fetch_quotes(...)` on independent cadences.
- `55+ screens` have local timers driving their own refresh cycles.
- `27 services` mix three incompatible response styles: `std::function` callbacks, Qt `signals:` with request IDs, and raw `QWebSocket` streams.
- Result: duplicate Python spawns, duplicate HTTP calls, fragmented cache behaviour, no single source of truth for "when was AAPL last updated."

The 30 s `CacheManager` TTL on market quotes partially hides the problem inside a single refresh window, but across tabs and time it breaks down.

**Goal:** one fetch per (symbol, source) at any moment, fanned out to every subscriber — markets, dashboard, watchlist, portfolio, AI chat, MCP tools, agents — via a single push primitive.

---

## 2. Non-Goals

- Not an inter-process broker. We stay in-process (one Qt app). No Redis, ZeroMQ, MQTT.
- Not a replacement for `CacheManager` — the hub **uses** it for persistence.
- Not a replacement for `PythonRunner`, `HttpClient`, or broker SDKs — services remain the I/O owners.
- Not a workflow/event bus — this is for **data state**, not imperative events.

---

## 3. Core Concepts

### 3.1 Topic

A **topic** is a string-keyed slot that holds the last-known-good value for one piece of data.

**Format:** `domain:subdomain:id[:modifier]`

Examples:
```
market:quote:AAPL
market:history:AAPL:1y:1d
market:sparkline:TSLA
news:general
news:symbol:NVDA
econ:fred:GDP
econ:dbnomics:IMF/IFS/USA.PCPI_IX.Q
ws:kraken:BTC-USD
ws:hyperliquid:ETH
broker:zerodha:positions
broker:angelone:orders
geopolitics:hdx:conflicts
agent:hedgefund:run:42
```

**Rules:**
- Lowercase, colon-delimited, no spaces.
- Stable — topic keys are the public contract; producers and subscribers agree on them.
- `*` wildcard allowed at the tail for subscriptions only (`market:quote:*`).

### 3.2 Subscriber

Any `QObject` (widget, screen, service, MCP tool) that calls `DataHub::subscribe(owner, topic, slot)`.

- Subscription is **owned by a QObject**. When the owner is destroyed, the subscription auto-cleans via `QObject::destroyed()` signal.
- Subscriber receives: (a) current cached value immediately on subscribe (if fresh), (b) every future `publish()` for that topic.
- A subscriber may watch many topics; a topic may have many subscribers.

### 3.3 Producer

A service that owns refresh for a set of topic patterns. Implements:

```cpp
class Producer {
  public:
    virtual ~Producer() = default;

    // Patterns this producer owns, e.g. {"market:quote:*", "market:history:*"}
    virtual QStringList topic_patterns() const = 0;

    // Hub calls this when ≥1 subscriber exists and the cached value is stale.
    // Producer fetches (async) and calls hub.publish() when done.
    virtual void refresh(const QStringList& topics) = 0;

    // Optional: max outbound requests per second. Hub scheduler groups and
    // paces refresh calls to respect this. Default 0 = unlimited.
    // Examples: Zerodha REST = 3, Angel One REST = 1, Polymarket = 10.
    virtual int max_requests_per_sec() const { return 0; }

    // Optional: called when last subscriber leaves a topic. Producer may
    // release resources (close WebSocket channel, cancel stream, etc.).
    virtual void on_topic_idle(const QString& /*topic*/) {}
};
```

### 3.4 Refresh Policy

Per-topic metadata stored in the hub:

```cpp
struct TopicPolicy {
    int ttl_ms          = 30000;   // cached value is fresh for this long
    int min_interval_ms = 5000;    // don't refresh more often than this
    bool push_only      = false;   // true → no TTL, no scheduled refresh
                                   //        (WebSocket-driven topics)
    int priority        = 0;       // higher = refreshed first under backpressure
};
```

- `push_only = true` for all `ws:*` topics. Hub never schedules them; producer calls `publish()` on every tick.
- `ttl_ms = 30000` default for market quotes (matches current behaviour).
- `ttl_ms = 300000` for news, `ttl_ms = 3600000` for economics series.

---

## 4. Public API

```cpp
namespace fincept::datahub {

class DataHub : public QObject {
    Q_OBJECT
  public:
    static DataHub& instance();

    // ── Subscribing ─────────────────────────────────────────────────────────
    // Returns the Qt connection (for manual disconnect if ever needed).
    // `owner` is the lifetime guard — subscription auto-cancels when owner dies.
    // `slot` receives the current value immediately (if cached + fresh) and
    // every future publish.
    QMetaObject::Connection subscribe(
        QObject* owner,
        const QString& topic,
        std::function<void(const QVariant&)> slot);

    // Typed variant — unwraps QVariant to T automatically. T must be registered
    // with Q_DECLARE_METATYPE + qRegisterMetaType<T>() at startup.
    template <typename T>
    QMetaObject::Connection subscribe(
        QObject* owner,
        const QString& topic,
        std::function<void(const T&)> slot) {
        static_assert(QMetaTypeId2<T>::Defined,
                      "T must be registered with Q_DECLARE_METATYPE");
        return subscribe(owner, topic, [slot](const QVariant& v) {
            if (v.canConvert<T>()) slot(v.value<T>());
        });
    }

    // Wildcard variant — slot receives (topic, value) pairs.
    QMetaObject::Connection subscribe_pattern(
        QObject* owner,
        const QString& pattern,  // e.g. "market:quote:*"
        std::function<void(const QString&, const QVariant&)> slot);

    void unsubscribe(QObject* owner);
    void unsubscribe(QObject* owner, const QString& topic);

    // ── Publishing (producer side) ──────────────────────────────────────────
    // Stores in CacheManager with topic's TTL, emits to all subscribers.
    void publish(const QString& topic, const QVariant& value);

    // Overload for push-only topics that want to override cache behaviour.
    void publish(const QString& topic, const QVariant& value,
                 std::chrono::milliseconds ttl);

    // ── Registration ────────────────────────────────────────────────────────
    void register_producer(Producer* producer);
    void unregister_producer(Producer* producer);

    // Tell the hub how a specific topic should behave.
    void set_policy(const QString& topic, const TopicPolicy& policy);
    void set_policy_pattern(const QString& pattern, const TopicPolicy& policy);

    // ── Pull-through ────────────────────────────────────────────────────────
    // Read current cached value without subscribing. Returns invalid QVariant
    // if unknown. Does NOT trigger a fetch.
    QVariant peek(const QString& topic) const;

    // Ask the hub to refresh now (if policy allows). Subscribers receive
    // update via the normal signal path.
    //
    // `force=false` (default): honours `min_interval_ms` — back-to-back
    // calls inside the interval gate are skipped. Use for routine
    // "kick the scheduler" requests.
    //
    // `force=true`: bypasses `min_interval_ms` so a user-driven refresh
    // button can refetch even inside the interval. Per-producer
    // `max_requests_per_sec()` is still honoured — rage-clicking cannot
    // hammer upstream. Introduced in Phase 6 for economics panels.
    void request(const QString& topic, bool force = false);
    void request(const QStringList& topics, bool force = false);

  signals:
    // Low-level fan-out. Most callers should use subscribe() instead; this
    // signal is for diagnostics, logging, and DevTools-style inspectors.
    void topic_updated(const QString& topic, const QVariant& value);
    void topic_idle(const QString& topic);   // last subscriber left
    void topic_active(const QString& topic); // first subscriber joined

  private:
    DataHub();
    // ... internals: subscription registry, scheduler timer, dispatcher ...
};

} // namespace fincept::datahub
```

---

## 5. Behaviour

### 5.1 Subscribe flow

```
Widget::ctor
  └─ DataHub::subscribe(this, "market:quote:AAPL", slot)
        ├─ register (this, "market:quote:AAPL", slot) in registry
        ├─ connect destroyed(this) → auto-unsubscribe
        ├─ if peek("market:quote:AAPL") is fresh → call slot(cached) now
        ├─ if topic was idle → emit topic_active(topic)
        │                      scheduler considers it on next tick
        └─ return Qt::Connection
```

### 5.2 Publish flow

```
MarketDataService::on_batch_result
  └─ hub.publish("market:quote:AAPL", QVariant::fromValue(quote))
        ├─ CacheManager::put(topic, value, policy.ttl_ms, domain)
        ├─ emit topic_updated(topic, value)
        │     └─ Qt fans out to all slots connected via subscribe()
        └─ update last_publish_ts[topic]
```

### 5.3 Scheduler flow

Hub owns a single `QTimer` at 1 s resolution. Each tick:

```
for topic in topics_with_subscribers:
    if policy.push_only:            continue
    if not expired(topic, policy):  continue
    if in_flight(topic):            continue
    group_by_producer[producer_of(topic)].append(topic)

for producer, topics in group_by_producer:
    producer->refresh(topics)   // producer is free to batch further
```

- Topics with no subscribers are skipped entirely — zero cost for idle data.
- `in_flight` tracking prevents double-fetches when a refresh is still pending.

### 5.4 Unsubscribe flow

```
Widget::~Widget
  └─ QObject::destroyed signal fires
        └─ hub.unsubscribe(widget)
              ├─ remove all (widget, *) entries from registry
              └─ for each topic that now has 0 subscribers:
                    emit topic_idle(topic)
                    producer->on_topic_idle(topic)  // may close WS, etc.
```

---

## 6. Data Types

Values flow as `QVariant`. Producers register their structs with Qt's meta-type system:

```cpp
// In MarketDataService.h
Q_DECLARE_METATYPE(fincept::services::QuoteData)

// At startup
qRegisterMetaType<fincept::services::QuoteData>();
```

Subscribers unwrap:
```cpp
hub.subscribe(this, "market:quote:AAPL", [this](const QVariant& v) {
    auto quote = v.value<services::QuoteData>();
    update_ui(quote);
});
```

For wildcard subscribers watching mixed types, the convention is to publish
`QVariantMap` / `QVariantList` instead of typed structs, or to key topics by
type so each subscriber knows what it's getting.

---

## 7. Threading

- `DataHub` lives on the main (GUI) thread.
- `publish()` is safe to call from any thread — it uses `QMetaObject::invokeMethod(this, ..., Qt::QueuedConnection)` to marshal to the hub thread.
- Slots are invoked on the subscriber's thread (auto-connection rules).
- Internal registry is protected by `QMutex` for the rare case of cross-thread subscribe/unsubscribe.

This matches `CLAUDE.md` rule P15 (thread safety) and P1 (never block UI).

---

## 8. Caching

- Hub delegates persistent cache to `CacheManager` — same keys as topic names.
- `CacheManager` already supports TTL, categories, and (de)serialization.
- `peek()` and the subscribe-sends-current-value path both hit `CacheManager::try_get()`.
- **One source of truth:** a screen never reads `CacheManager` directly; it always goes through the hub. This prevents the current split where some services cache and some don't.

---

## 9. Relationship to Existing Infrastructure

| Existing piece              | Role after DataHub                                       |
|-----------------------------|----------------------------------------------------------|
| `MarketDataService`         | Becomes a `Producer` for `market:*` topics               |
| `NewsService`               | Becomes a `Producer` for `news:*` topics                 |
| `EconomicsService`          | Becomes a `Producer` for `econ:*` topics                 |
| `ExchangeService` (crypto)  | Becomes a push-only `Producer` for `ws:kraken:*`, `ws:hyperliquid:*` |
| `PolymarketWebSocket`       | Becomes a push-only `Producer` for `polymarket:*`        |
| Broker adapters (16)        | Each a `Producer` for `broker:<id>:*`                    |
| `CacheManager`              | Backing store for topic values (unchanged)               |
| `PythonRunner`              | Unchanged — still the Python I/O primitive               |
| `HttpClient`                | Unchanged                                                |
| `QWebSocket` clients        | Wrapped inside producers, push via `publish()`           |
| Per-widget `QTimer` refresh | **Deleted** — hub scheduler replaces all of them         |

---

## 10. Worked Example — Market Quote

**Before (current state, `QuoteTableWidget::refresh_data` at
`src/screens/dashboard/widgets/QuoteTableWidget.cpp:36`):**

```cpp
void QuoteTableWidget::refresh_data() {
    set_loading(true);
    services::MarketDataService::instance().fetch_quotes(
        symbols_, [this](bool ok, QVector<services::QuoteData> quotes) {
            set_loading(false);
            if (!ok || quotes.isEmpty()) { /* ... */ return; }
            populate(quotes);
        });
}
```

`QuoteTableWidget` is reused by `IndicesWidget`, `CryptoWidget`,
`ForexWidget`, and `CommoditiesWidget` (all are factory functions that
construct a `QuoteTableWidget` with a different symbol list).
`WidgetGrid` or the dashboard owns a timer that periodically calls
`refresh_data()` on the widget.

**After:**
```cpp
// QuoteTableWidget ctor — subscribe once per symbol, no timer anywhere
for (const auto& sym : symbols_) {
    datahub::DataHub::instance().subscribe(this, "market:quote:" + sym,
        [this, sym](const QVariant& v) {
            if (!v.isValid()) return;
            update_row(sym, v.value<services::QuoteData>());
        });
}
// No refresh_data(), no timer. The dashboard's refresh button calls
// DataHub::instance().request({"market:quote:..."}) to force refresh.
```

**Inside `MarketDataService` (now a Producer):**
```cpp
QStringList MarketDataService::topic_patterns() const {
    return {"market:quote:*", "market:history:*", "market:sparkline:*"};
}

void MarketDataService::refresh(const QStringList& topics) {
    QStringList quote_syms;
    for (const auto& t : topics)
        if (t.startsWith("market:quote:"))
            quote_syms << t.mid(13);

    if (quote_syms.isEmpty()) return;

    // existing batched Python call — unchanged internal logic
    batched_python_fetch(quote_syms, [this](QVector<QuoteData> quotes) {
        auto& hub = datahub::DataHub::instance();
        for (const auto& q : quotes)
            hub.publish("market:quote:" + q.symbol, QVariant::fromValue(q));
    });
}
```

Dashboard's `IndicesWidget`, `WatchlistWidget`, `QuoteTableWidget`, markets panel, portfolio blotter — all of them subscribing to overlapping symbols — trigger **one** `refresh()` call per hub tick. The Python process spawns once.

---

## 11. Worked Example — WebSocket Push

```cpp
// ExchangeService wraps the Kraken WebSocket
void ExchangeService::on_kraken_tick(const QJsonObject& msg) {
    QString pair = msg["pair"].toString();
    double price = msg["price"].toDouble();

    // Push-only topic — no TTL, no scheduled refresh
    datahub::DataHub::instance().publish(
        "ws:kraken:" + pair,
        QVariant::fromValue(TickData{pair, price, ...}));
}

// In ExchangeService ctor:
TopicPolicy p;
p.push_only = true;
datahub::DataHub::instance().set_policy_pattern("ws:kraken:*", p);
```

Crypto trading screen, dashboard crypto widget, MCP `CryptoTradingTools`, agents — all subscribe to `ws:kraken:BTC-USD` and receive every tick. One WebSocket connection serves everyone.

### 11.1 Progressive Publish Pattern (Phase 5)

Some producers resolve a query in **chunks** rather than a single final payload — `NewsService::fetch_all_news_progressive()` hits N RSS feeds in parallel and wants to fan out partial results as each feed arrives. The hub has no first-class streaming primitive; we express progressive loading as a **sequence of full-list publishes** to the same topic:

```cpp
// Inside the progressive fetch — each RSS feed that finishes triggers
// a re-publish of the accumulated (not delta) list.
void NewsService::publish_articles_to_hub(const QVector<NewsArticle>& accumulated) {
    hub.publish("news:general", QVariant::fromValue(accumulated));
}
```

**Rules for consumers:**

- Each payload **replaces** the previously rendered list — do not accumulate on the subscriber side.
- Lists are small enough (hundreds, not millions) that full replacement per chunk is cheap.
- There is **no sentinel "done" marker** — subscribers just see successive lists, with the last one being the final state. Consumers that need to know completion should compare the payload identity or count stability.

**Rules for producers:**

- Always publish the **accumulated** list, never deltas.
- Pair progressive topics with `TopicPolicy::coalesce_within_ms` (e.g. 250 ms for news) so cold-cache fills don't repaint the UI on every chunk. The hub will keep the most-recent payload and fan out once per coalesce window.
- `refresh(topics)` should call the progressive fetcher; the scheduler's `min_interval_ms` prevents back-to-back cold fills.

Rejected alternative: wrapping the payload in `{list, is_final}` struct. Most consumers don't care about completion — those that do can observe list identity stability. Keeps the metatype small.

---

## 12. Enforcement Rules (after migration)

To prevent regression, a new set of CLAUDE.md rules:

- **D1.** Screens and widgets **must not** call `PythonRunner::instance().run()` or `HttpClient::*` or service `fetch_*` callbacks directly. They subscribe via `DataHub`.
- **D2.** Services that fetch external data **must** implement `Producer` and publish results via `DataHub::publish()`.
- **D3.** No `QTimer` in a widget whose only job is to trigger a data refresh. The hub's scheduler owns cadence.
- **D4.** All topic keys **must** be documented in `docs/DATAHUB_TOPICS.md` (a registry added in Phase 4).
- **D5.** `push_only` topics (WebSocket-driven) **must** declare their policy at producer construction time, not inline.

---

## 13. Observability

Hub exposes a debug interface:

```cpp
struct TopicStats {
    QString topic;
    int subscriber_count;
    qint64 last_publish_ms;
    qint64 last_refresh_request_ms;
    int total_publishes;
    bool in_flight;
};

QVector<TopicStats> DataHub::stats() const;
```

A dev-only screen (behind a feature flag) shows live topic table — useful for catching "why is X still fetching when nothing is visible" bugs. Also surfaced through MCP so AI chat can inspect hub state.

---

## 14. Failure Modes

| Failure                               | Hub behaviour                                           |
|---------------------------------------|---------------------------------------------------------|
| Producer raises / Python script fails | Hub logs, serves stale cached value to subscribers, retries on next scheduler tick with backoff |
| Subscriber slot throws                | Caught at Qt boundary, logged, other subscribers unaffected |
| Subscriber destroyed mid-publish      | `QPointer` guard + `destroyed()` cleanup — no crash     |
| WebSocket disconnect                  | Producer decides: reconnect internally, optionally publish a sentinel value |
| Cache backend unavailable             | Hub degrades to in-memory only (no persistence); logs warning |
| Topic has no producer registered      | `subscribe()` still works but `request()` logs a warning; value stays null |

---

## 15. Locked Decisions

1. **Topic key format** — `domain:subdomain:id[:modifier]`, lowercase, colon-delimited. Wildcard `*` allowed at the tail for subscriptions only.
2. **Typed subscribe API** — `subscribe<T>(owner, topic, void(const T&))` ships in Phase 1 alongside the untyped `QVariant` variant. Template wrapper unwraps via `QVariant::value<T>()` with a `Q_DECLARE_METATYPE` static assertion.
3. **Request-response commands** (agent runs, LLM calls) — stay **outside** the hub. They're commands, not data. `AgentService::run(...)` and `LlmService::complete(...)` keep their existing APIs.
4. **Persistence on app restart** — hub starts **cold**. `CacheManager` still persists values for warm restarts within TTL, but the hub does **not** rehydrate subscribers on boot. Opt-in per-topic `rehydrate_on_start` flag in `TopicPolicy` (default false) for long-TTL, non-time-sensitive data (news, economics) — added later if needed.
5. **Rate limits** — enforced **hub-level, per producer**. `Producer` interface adds `max_requests_per_sec() const` (default: unlimited). The scheduler honours it when grouping refresh calls. Keeps rate-limit policy in one place, visible and uniform.

### Remaining open item
**Inspector screen visibility** — during Phase 1 it lives behind the `FINCEPT_DATAHUB_ENABLED` compile flag only. Promote to a user-visible "Developer mode" tab in Phase 10, once the topic registry is stable.

---

## 16. Success Criteria

- Zero duplicate Python spawns for the same symbol within 30 s across any combination of screens.
- No widget owns a data-refresh `QTimer` after Phase 2 ships.
- Switching tabs is instant — subscribers read cached values; no blocking fetch.
- WebSocket feeds serve N subscribers through one connection.
- `hub.stats()` shows coherent subscriber counts that match what's actually on screen.
