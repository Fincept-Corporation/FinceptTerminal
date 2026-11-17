#pragma once

#include "datahub/Producer.h"
#include "datahub/TopicPolicy.h"

#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariant>
#include <QVector>

#include <atomic>
#include <chrono>
#include <functional>

namespace fincept::datahub {

/// Lightweight snapshot of one topic's state — surfaced by `stats()` and
/// the DataHub inspector screen.
struct TopicStats {
    QString topic;
    int subscriber_count = 0;
    qint64 last_publish_ms = 0;         ///< QDateTime::currentMSecsSinceEpoch at last publish
    qint64 last_refresh_request_ms = 0; ///< last time scheduler asked producer to refresh
    qint64 last_error_ms = 0;           ///< last time a refresh error was reported (0 = never)
    int total_publishes = 0;
    int total_errors = 0;
    bool in_flight = false; ///< producer has an outstanding refresh()
    bool push_only = false;
    QString last_error;        ///< most recent error string, empty if none or cleared
    int error_streak = 0;      ///< consecutive failed refreshes since the last successful publish
    qint64 retry_after_ms = 0; ///< wall-clock ms before which the hub won't refetch (0 = no backoff)
};

/// In-process pub/sub data layer. See DATAHUB_ARCHITECTURE.md §4.
///
/// Singleton. Lives on the main thread. `publish()` is safe to call
/// from any thread — it marshals to the hub thread via queued connection.
class DataHub : public QObject {
    Q_OBJECT
  public:
    static DataHub& instance();

    // ── Subscribing ─────────────────────────────────────────────────────────

    /// Subscribe to a single topic. `owner` is the lifetime guard —
    /// subscription auto-cancels when the owner is destroyed. `slot`
    /// receives the current cached value immediately (if fresh) and every
    /// future `publish()` for this topic.
    QMetaObject::Connection subscribe(QObject* owner, const QString& topic, std::function<void(const QVariant&)> slot);

    /// Typed variant — unwraps `QVariant` to `T` automatically. `T` must
    /// be registered via `Q_DECLARE_METATYPE` + `qRegisterMetaType<T>()`.
    template <typename T>
    QMetaObject::Connection subscribe(QObject* owner, const QString& topic, std::function<void(const T&)> slot) {
        static_assert(QMetaTypeId2<T>::Defined, "T must be registered with Q_DECLARE_METATYPE");
        return subscribe(owner, topic, [slot](const QVariant& v) {
            if (v.canConvert<T>())
                slot(v.value<T>());
        });
    }

    /// Subscribe to a pattern (`*`-suffix wildcard, e.g. `"market:quote:*"`).
    /// The slot receives `(topic, value)` for every matching publish.
    QMetaObject::Connection subscribe_pattern(QObject* owner, const QString& pattern,
                                              std::function<void(const QString&, const QVariant&)> slot);

    /// Remove every subscription (concrete + pattern) owned by `owner`.
    void unsubscribe(QObject* owner);
    /// Remove only the concrete-topic subscription for `owner` on `topic`.
    /// Does NOT touch pattern subscriptions — use `unsubscribe_pattern()`
    /// for that. The two registries are separate.
    void unsubscribe(QObject* owner, const QString& topic);
    /// Remove only the wildcard-pattern subscription for `owner` on `pattern`.
    /// Symmetric to `subscribe_pattern()`. No-op if not currently subscribed.
    void unsubscribe_pattern(QObject* owner, const QString& pattern);

    // ── Error subscriptions ─────────────────────────────────────────────────
    //
    // The `topic_error` Qt signal fans every error to every connected slot;
    // each handler then has to filter by topic name. For consumers that
    // only care about errors on one specific topic family, the
    // `subscribe_errors` family below provides per-topic / per-pattern
    // filtering inside the hub. The `owner` lifetime contract is identical
    // to the data subscribe API: auto-cleanup on destroyed().

    /// Receive `publish_error()` events for a single concrete topic.
    ///
    /// If the topic is currently in error state (its last refresh failed and
    /// nothing has been published since), `slot` also receives that current
    /// error once, queued to the owner's thread (next event-loop turn, never
    /// from inside this call) — the same way `subscribe()` delivers a cached
    /// value. The replay is skipped if, when it runs, the topic has recovered,
    /// failed again (the live event covers that), or has a refetch in flight
    /// (e.g. the caller's own `request(topic, true)`: its outcome is
    /// delivered live instead).
    QMetaObject::Connection subscribe_errors(QObject* owner, const QString& topic,
                                             std::function<void(const QString& error)> slot);

    /// Receive `publish_error()` events for every topic matching `pattern`.
    /// Like `subscribe_errors()`, replays the current error of every
    /// already-known matching topic that is in error state.
    QMetaObject::Connection
    subscribe_pattern_errors(QObject* owner, const QString& pattern,
                             std::function<void(const QString& topic, const QString& error)> slot);

    void unsubscribe_errors(QObject* owner, const QString& topic);
    void unsubscribe_pattern_errors(QObject* owner, const QString& pattern);

    // ── Publishing (producer side) ──────────────────────────────────────────

    /// Store `value` under `topic` and fan out to every subscriber.
    /// Uses the topic's registered policy TTL for the in-memory copy.
    /// Thread-safe.
    void publish(const QString& topic, const QVariant& value);

    /// Per-publish TTL override (used by push-only producers that want
    /// values to age out even though the scheduler ignores them).
    void publish(const QString& topic, const QVariant& value, std::chrono::milliseconds ttl);

    /// Signal that a refresh() for `topic` has failed. Clears the
    /// `in_flight` flag, notifies `subscribe_errors` / `subscribe_pattern_errors`
    /// slots and emits `topic_error(topic, error)` so subscribers can surface
    /// the failure in the UI. Does NOT update the cached value — last-known-
    /// good stays visible. Thread-safe; marshals to the hub thread.
    ///
    /// Error backoff: each consecutive failure pushes the topic's next
    /// automatic refetch out along a ladder (2 s, 5 s, 15 s, 60 s, then
    /// 300 s; ±10 % jitter). Until it expires neither the scheduler, a
    /// non-forced `request()`, nor a new subscriber's cold-start fetch will
    /// re-request the topic. A successful `publish()` or a user-initiated
    /// `request(..., force=true)` resets it.
    void publish_error(const QString& topic, const QString& error);

    /// As above, with a producer hint for errors that will not clear by
    /// retrying soon. The topic is not refetched automatically for at least
    /// `retry_after` (the ladder still applies if it is longer). Use
    /// `kNegativeCacheRetry` for "symbol not found / delisted"; a shorter
    /// hint suits an upstream rate limit. A non-positive hint behaves like
    /// the two-argument overload.
    void publish_error(const QString& topic, const QString& error, std::chrono::milliseconds retry_after);

    /// Retry hint for long-lived errors (unknown symbol, delisted
    /// instrument): negative-cache the topic for ~15 minutes.
    static constexpr std::chrono::milliseconds kNegativeCacheRetry{15 * 60 * 1000};

    /// Forget the error backoff of every known topic matching `pattern`
    /// (`*`-suffix wildcard), so failed topics are retried on the next
    /// scheduler pass. For producers whose upstream has just recovered
    /// (e.g. Python finished installing). The stored error is kept until the
    /// next successful publish. Thread-safe.
    void clear_backoff(const QString& pattern);

    // ── Registration ────────────────────────────────────────────────────────

    void register_producer(Producer* producer);
    void unregister_producer(Producer* producer);

    void set_policy(const QString& topic, const TopicPolicy& policy);
    void set_policy_pattern(const QString& pattern, const TopicPolicy& policy);
    /// Remove a previously-registered pattern policy. Existing TopicState
    /// entries that inherited the policy keep their current copy — to
    /// apply a new policy, call `set_policy_pattern()` after clearing.
    /// Returns the number of pattern entries removed (0 if not present).
    int clear_policy_pattern(const QString& pattern);

    // ── Pull-through ────────────────────────────────────────────────────────

    /// Read the current cached value without subscribing. Returns an
    /// invalid `QVariant` if the topic is unknown, unset, or the cached
    /// value has aged past its `ttl_ms`. Push-only topics ignore TTL and
    /// always return the last published value. Does NOT trigger a fetch.
    ///
    /// Use `peek_raw()` if you want the last-known-good value regardless
    /// of TTL (diagnostic / stats / report-builder fallbacks).
    QVariant peek(const QString& topic) const;

    /// Read the cached value without TTL check. Intended for diagnostic
    /// callers (DataHubTools MCP tool, stats inspector) and for fallbacks
    /// that prefer stale data over a blank screen. Returns an invalid
    /// QVariant if the topic has no cached value at all.
    QVariant peek_raw(const QString& topic) const;

    /// Ask the hub to refresh `topic` now (subject to policy + rate limit).
    /// Subscribers are notified via the usual signal path when the
    /// producer publishes. No-op if no producer owns the topic.
    ///
    /// `force=true` bypasses `min_interval_ms` (so a user-driven refresh
    /// button can refetch even inside the interval gate). Per-producer
    /// `max_requests_per_sec()` is still honoured — rage-clicking can't
    /// hammer upstream.
    ///
    /// `force=true` is treated as user-initiated: it also bypasses and resets
    /// the topic's error backoff (see `publish_error`). `force=false`
    /// requests for a topic that is backing off are dropped. Do not call
    /// `request(..., true)` from an error slot or a timer — that defeats the
    /// backoff.
    void request(const QString& topic, bool force = false);
    void request(const QStringList& topics, bool force = false);

    // ── Introspection ───────────────────────────────────────────────────────

    /// Wall-clock ms of this topic's last publish, or 0 if it has never
    /// published / is unknown. Cheap O(1) hash lookup — `stats()` walks every
    /// topic and is far too heavy to call from a per-widget refresh.
    ///
    /// Exists so consumers can render a freshness indicator. A financial
    /// terminal showing a price with no indication of its age is actively
    /// misleading: the user cannot distinguish a live quote from one frozen
    /// twenty minutes ago by a dead producer.
    qint64 last_publish_ms(const QString& topic) const;

    /// Convenience: ms since the last publish, or -1 if never published.
    /// Compare against the topic's TTL to decide "fresh / stale / dead".
    qint64 age_ms(const QString& topic) const;

    /// The topic's current error — the last `publish_error()` text if its
    /// most recent refresh failed and nothing has been published since —
    /// or an empty string. Cheap O(1) lookup.
    QString last_error(const QString& topic) const;

    /// Automatic fetches not made because nobody could see the topic: a
    /// `pause_when_inactive` topic is neither refreshed by the scheduler nor
    /// cold-started by subscribe() while every live subscriber is a hidden
    /// widget or one in a minimised window (non-widget owners — services,
    /// stores — always count as watching). Counted once per freshness window
    /// of each skipped topic, i.e. roughly the fetches that did not happen.
    /// Cumulative since start; thread-safe.
    quint64 hidden_skip_count() const { return hidden_skip_count_.load(std::memory_order_relaxed); }

    QVector<TopicStats> stats() const;

    /// Diagnostic: list the QObject owners currently subscribed to `topic`.
    /// Pointers are raw (not QPointer) — only safe to use for identity
    /// comparison, logging, or `QObject::objectName()` lookup on the GUI
    /// thread while the hub mutex is not held by the caller.
    /// Returns an empty list if the topic has no subscribers.
    QList<QObject*> subscribers(const QString& topic) const;

    /// Drop a topic's cached state entirely (value, publish counters,
    /// coalesce state). Subscriptions are preserved — they remain attached
    /// and will receive the next publish normally. Used by producers that
    /// generate disposable per-run topics (e.g. agent output:<run_id>) to
    /// avoid unbounded hub memory growth. No-op if the topic is unknown.
    void retire_topic(const QString& topic);

    /// Test hook: drive the scheduler manually. Normally the internal
    /// QTimer fires this at 1 s intervals. Exposed so unit tests don't
    /// have to spin the event loop for seconds.
    void tick_scheduler();

    /// Tune the request-coalesce window (default 100ms). Larger windows
    /// merge more bursts into one producer dispatch at the cost of
    /// perceived latency on user-initiated requests. Must be >= 0; 0
    /// disables coalescing entirely (request() dispatches immediately).
    void set_coalesce_window_ms(int ms);
    int coalesce_window_ms() const { return coalesce_window_ms_; }

  signals:
    void topic_updated(const QString& topic, const QVariant& value);
    void topic_idle(const QString& topic);
    void topic_active(const QString& topic);
    /// Emitted when a producer reports a refresh failure (via
    /// `publish_error`) or when the scheduler times out a stuck in_flight
    /// topic. `error` is a short diagnostic string ("http 429",
    /// "refresh_timeout", ...). The topic's cached value is unchanged —
    /// subscribers still see the last good value if any.
    void topic_error(const QString& topic, const QString& error);

  private:
    DataHub();
    ~DataHub() override = default;
    DataHub(const DataHub&) = delete;
    DataHub& operator=(const DataHub&) = delete;

    // ── Internal types ──────────────────────────────────────────────────────
    struct Subscription {
        QPointer<QObject> owner;
        std::function<void(const QVariant&)> slot; // single-topic
        std::function<void(const QString&, const QVariant&)> pattern_slot;
        bool is_pattern = false;
    };

    struct TopicState {
        QVariant value;
        qint64 last_publish_ms = 0;
        qint64 last_refresh_request_ms = 0;
        qint64 last_error_ms = 0;
        int total_publishes = 0;
        int total_errors = 0;
        bool in_flight = false;
        TopicPolicy policy;
        bool policy_explicit = false; ///< set_policy() was called
        QString last_error;           ///< most recent error string; cleared on successful publish
        // Error backoff — see publish_error(). Both reset on a successful
        // publish and on a user-initiated request(force=true).
        int error_streak = 0;      ///< consecutive failures (publish_error or refresh timeout)
        qint64 retry_after_ms = 0; ///< no automatic refetch before this wall-clock ms
        // Coalesce state — non-zero policy.coalesce_within_ms only.
        bool coalesce_pending = false; ///< a deferred publish is armed
        QVariant coalesce_latest;      ///< most recent value inside window
        // Gap 3 fix: a per-publish ttl override used to be written into
        // policy.ttl_ms, leaking the override into subsequent publishes.
        // Stored separately now; do_publish reads policy.ttl_ms unless
        // ttl_override_ms > 0, in which case the override is applied
        // ONLY for the current publish's freshness window.
        int ttl_override_ms = 0;
        /// Last time an automatic fetch was skipped for want of a visible
        /// subscriber (see hidden_skip_count()); 0 = never.
        qint64 hidden_skip_ms = 0;
    };

    struct ErrorSub {
        QPointer<QObject> owner;
        std::function<void(const QString&)> single_slot;                  // subscribe_errors
        std::function<void(const QString&, const QString&)> pattern_slot; // subscribe_pattern_errors
        bool is_pattern = false;
    };

    // ── Helpers ─────────────────────────────────────────────────────────────
    void deliver_initial_value(QObject* owner, const QString& topic, const std::function<void(const QVariant&)>& slot);
    /// Error-side counterpart of deliver_initial_value(): queue the current
    /// error of each topic in `errors` (topic -> (error, last_error_ms)) to
    /// the owner's thread. Exactly one of the two slots is set. Caller has
    /// released mutex_.
    void deliver_initial_errors(QObject* owner, const QHash<QString, QPair<QString, qint64>>& errors,
                                const std::function<void(const QString&)>& single_slot,
                                const std::function<void(const QString&, const QString&)>& pattern_slot);
    /// request() body. `user_initiated` resets the error backoff; otherwise
    /// a topic inside its backoff window is skipped even when `force` is set
    /// (the subscribe() cold-start path uses force without user intent).
    void request_impl(const QStringList& topics, bool force, bool user_initiated);
    void on_owner_destroyed(QObject* owner);
    Producer* find_producer(const QString& topic) const;
    TopicPolicy resolve_policy(const QString& topic) const;
    TopicState& state_for(const QString& topic);
    /// A topic pulled out of coalesce_pending_ before dispatch was never handed
    /// to its producer, so no publish()/publish_error() will ever clear its
    /// in_flight flag. Clear it here, or a re-subscribe inside the coalesce
    /// window (unsubscribe + subscribe, as panels do on refresh) finds the
    /// topic "in flight", skips the cold-start fetch, and the topic stays dark
    /// until refresh_timeout_ms (30 s) expires. Caller holds mutex_.
    void clear_undispatched_in_flight(const QString& topic);
    static bool pattern_matches(const QString& pattern, const QString& topic);
    void emit_to_subscribers(const QString& topic, const QVariant& value);
    void do_publish(const QString& topic, const QVariant& value, std::chrono::milliseconds ttl_override);
    void do_coalesced_flush(const QString& topic);
    void scheduler_body();

    // ── Idle-topic eviction ─────────────────────────────────────────────────
    //
    // `topics_` was a process-lifetime cache: `state_for()` inserts a
    // TopicState retaining the full last-published QVariant on every
    // publish/subscribe/peek, and the only automatic erase sites are gated on
    // `policy.drop_on_idle` — which defaults false and is set true by exactly
    // two producers. `ttl_ms` is a freshness *test*; it never releases the
    // payload. So memory scaled with the total breadth browsed since launch,
    // monotonically, and the heavy families are the wide ones:
    // `market:history:<sym>:<period>:<interval>` holds full OHLC vectors,
    // `news:symbol:<sym>` article bodies, `dbnomics:series:*` whole
    // observation series.
    //
    // The sweep below is the conservative counterpart, modelled on
    // `mcp::ResultStore::sweep_locked()`: it only ever touches topics with
    // **zero live subscribers**, no in-flight refresh, and no armed coalesce
    // flush, and only once they are far past their freshness window. A
    // re-show inside the window is still a cache hit.

    /// True if any concrete or wildcard subscription with a live owner
    /// currently covers `topic`. Caller holds mutex_.
    bool has_live_subscribers(const QString& topic) const;
    /// Erase idle, long-stale, unsubscribed TopicStates. Caller holds mutex_.
    void sweep_idle_topics(qint64 now);

    // ── Fetch gating for topics nobody can see ──────────────────────────────
    /// True if a live concrete or wildcard subscriber of `topic` is "visible
    /// for work": a widget that is shown and whose window is not minimised,
    /// or any non-widget owner. Widgets are only inspected on their own
    /// thread (elsewhere they count as visible). Caller holds mutex_.
    bool has_visible_subscriber(const QString& topic) const;
    /// Count `st`'s skipped fetch in hidden_skip_count_, at most once per
    /// freshness window. Caller holds mutex_.
    void note_hidden_skip(TopicState& st, qint64 now);
    std::atomic<quint64> hidden_skip_count_{0};

    /// A topic is only evictable once it is this many freshness windows
    /// stale. Generous on purpose — the point is to release payloads nobody
    /// came back for, not to fight the cache.
    static constexpr int kIdleEvictTtlMultiple = 10;
    /// Floor on the above, so short-TTL families (1 s quote topics) aren't
    /// evicted seconds after the user tabs away.
    static constexpr qint64 kIdleEvictMinAgeMs = 5 * 60 * 1000;
    /// Sweep cadence in scheduler ticks (1 s each) — the walk is O(topics_)
    /// so it doesn't belong on the per-second path.
    static constexpr int kIdleSweepEveryTicks = 60;
    int sweep_tick_counter_ = 0;

    // ── State ───────────────────────────────────────────────────────────────
    mutable QMutex mutex_;

    // subscriptions_[topic] -> vector of concrete-topic subs
    QHash<QString, QVector<Subscription>> subscriptions_;
    // pattern_subscriptions_[pattern] -> vector of wildcard subs
    QHash<QString, QVector<Subscription>> pattern_subscriptions_;
    // owner -> set of (topic, is_pattern) so we can fast-unsub on destroy
    QHash<QObject*, QSet<QString>> owner_topics_;
    QHash<QObject*, QSet<QString>> owner_patterns_;

    QHash<QString, TopicState> topics_;

    // Per-topic / per-pattern error subscriptions (Gap 7 fix). Symmetric
    // to subscriptions_ / pattern_subscriptions_ but keyed for the error
    // fan-out path inside publish_error().
    QHash<QString, QVector<ErrorSub>> error_subscriptions_;
    QHash<QString, QVector<ErrorSub>> error_pattern_subscriptions_;
    QHash<QObject*, QSet<QString>> error_owner_topics_;
    QHash<QObject*, QSet<QString>> error_owner_patterns_;

    // Policies applied by pattern — lower-priority than explicit set_policy().
    QList<QPair<QString, TopicPolicy>> pattern_policies_;

    // Producers in registration order. Each owns a set of topic patterns.
    QVector<Producer*> producers_;
    // Per-producer: last time we called refresh() on it — for rate pacing.
    QHash<Producer*, qint64> producer_last_refresh_ms_;

    // Prefix index for find_producer() — sorted by prefix length DESC so a
    // longest-match scan (linear in matching entries) picks the most
    // specific pattern. Rebuilt on register/unregister; patterns are
    // `*`-suffix so the prefix is pattern minus trailing '*' (or the full
    // pattern for exact matches). Replaces O(N_producers × N_patterns)
    // iteration with a cheaper cache-friendly scan.
    struct PatternEntry {
        QString prefix;   ///< pattern with trailing '*' stripped, or full pattern
        bool is_wildcard; ///< true if original pattern ended with '*'
        Producer* owner;
    };
    QVector<PatternEntry> pattern_index_;
    void rebuild_pattern_index(); // caller holds mutex_

    QTimer scheduler_;

    // Request coalescing — see DataHub::request(). Any topics queued here
    // have already passed the in_flight / min_interval / rate-limit gates and
    // are guaranteed to dispatch together on the next coalesce flush.
    QHash<Producer*, QStringList> coalesce_pending_;
    QTimer coalesce_timer_;
    static constexpr int kDefaultCoalesceWindowMs = 100;
    int coalesce_window_ms_ = kDefaultCoalesceWindowMs;
    void flush_coalesced_requests();
};

} // namespace fincept::datahub
