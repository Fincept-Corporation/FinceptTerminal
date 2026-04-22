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

#include <chrono>
#include <functional>

namespace fincept::datahub {

/// Lightweight snapshot of one topic's state — surfaced by `stats()` and
/// the DataHub inspector screen.
struct TopicStats {
    QString topic;
    int subscriber_count = 0;
    qint64 last_publish_ms = 0;          ///< QDateTime::currentMSecsSinceEpoch at last publish
    qint64 last_refresh_request_ms = 0;  ///< last time scheduler asked producer to refresh
    qint64 last_error_ms = 0;            ///< last time a refresh error was reported (0 = never)
    int total_publishes = 0;
    int total_errors = 0;
    bool in_flight = false;              ///< producer has an outstanding refresh()
    bool push_only = false;
    QString last_error;                  ///< most recent error string, empty if none or cleared
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
    QMetaObject::Connection subscribe(
        QObject* owner,
        const QString& topic,
        std::function<void(const QVariant&)> slot);

    /// Typed variant — unwraps `QVariant` to `T` automatically. `T` must
    /// be registered via `Q_DECLARE_METATYPE` + `qRegisterMetaType<T>()`.
    template <typename T>
    QMetaObject::Connection subscribe(
        QObject* owner,
        const QString& topic,
        std::function<void(const T&)> slot) {
        static_assert(QMetaTypeId2<T>::Defined,
                      "T must be registered with Q_DECLARE_METATYPE");
        return subscribe(owner, topic, [slot](const QVariant& v) {
            if (v.canConvert<T>())
                slot(v.value<T>());
        });
    }

    /// Subscribe to a pattern (`*`-suffix wildcard, e.g. `"market:quote:*"`).
    /// The slot receives `(topic, value)` for every matching publish.
    QMetaObject::Connection subscribe_pattern(
        QObject* owner,
        const QString& pattern,
        std::function<void(const QString&, const QVariant&)> slot);

    /// Remove every subscription owned by `owner`.
    void unsubscribe(QObject* owner);
    /// Remove only the subscription for `owner` on `topic`.
    void unsubscribe(QObject* owner, const QString& topic);

    // ── Publishing (producer side) ──────────────────────────────────────────

    /// Store `value` under `topic` and fan out to every subscriber.
    /// Uses the topic's registered policy TTL for the in-memory copy.
    /// Thread-safe.
    void publish(const QString& topic, const QVariant& value);

    /// Per-publish TTL override (used by push-only producers that want
    /// values to age out even though the scheduler ignores them).
    void publish(const QString& topic, const QVariant& value,
                 std::chrono::milliseconds ttl);

    /// Signal that a refresh() for `topic` has failed. Clears the
    /// `in_flight` flag so the scheduler can retry on the next pass and
    /// emits `topic_error(topic, error)` so subscribers can surface the
    /// failure in the UI. Does NOT update the cached value — last-known-
    /// good stays visible. Thread-safe; marshals to the hub thread.
    void publish_error(const QString& topic, const QString& error);

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
    void request(const QString& topic, bool force = false);
    void request(const QStringList& topics, bool force = false);

    // ── Introspection ───────────────────────────────────────────────────────

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
        std::function<void(const QVariant&)> slot;          // single-topic
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
        bool policy_explicit = false;  ///< set_policy() was called
        QString last_error;            ///< most recent error string; cleared on successful publish
        // Coalesce state — non-zero policy.coalesce_within_ms only.
        bool coalesce_pending = false;   ///< a deferred publish is armed
        QVariant coalesce_latest;        ///< most recent value inside window
    };

    // ── Helpers ─────────────────────────────────────────────────────────────
    void deliver_initial_value(QObject* owner, const QString& topic,
                               const std::function<void(const QVariant&)>& slot);
    void on_owner_destroyed(QObject* owner);
    Producer* find_producer(const QString& topic) const;
    TopicPolicy resolve_policy(const QString& topic) const;
    TopicState& state_for(const QString& topic);
    static bool pattern_matches(const QString& pattern, const QString& topic);
    void emit_to_subscribers(const QString& topic, const QVariant& value);
    void do_publish(const QString& topic, const QVariant& value,
                    std::chrono::milliseconds ttl_override);
    void do_coalesced_flush(const QString& topic);
    void scheduler_body();

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
        QString prefix;    ///< pattern with trailing '*' stripped, or full pattern
        bool is_wildcard;  ///< true if original pattern ended with '*'
        Producer* owner;
    };
    QVector<PatternEntry> pattern_index_;
    void rebuild_pattern_index();  // caller holds mutex_

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
