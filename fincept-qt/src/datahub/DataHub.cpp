#include "datahub/DataHub.h"

#include "core/logging/Logger.h"

#include <QDateTime>
#include <QMetaObject>
#include <QMutexLocker>
#include <QThread>
#include <QVariant>
#include <QWidget>

#include <algorithm>

namespace fincept::datahub {

namespace {
qint64 now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}

// Gap 3 helper: returns the freshness window in ms for a topic state.
// Picks the per-publish override if set; otherwise the policy TTL.
template <typename TopicStateT>
inline int effective_ttl_ms(const TopicStateT& st) {
    return st.ttl_override_ms > 0 ? st.ttl_override_ms : st.policy.ttl_ms;
}

// Phase 8 / decision 9.2: walk up from the owner to its top-level widget
// and read the `fincept.active_for_work` dynamic property that WindowFrame
// sets in its constructor + changeEvent. If the owner is not a QWidget or
// has no top-level ancestor (rare — module-level singletons that subscribe),
// treat as active (better to over-deliver than starve a legitimate consumer).
//
// We look at QWidget::window() rather than walking QObject::parent() chain
// because the latter can stop at an intermediate QObject that isn't a
// widget. window() handles widget-graph reparenting correctly and is what
// Qt itself uses for "the top-level for this widget."
//
// No caching — the lookup is one virtual call + one property read, both
// cheap. Caching adds cache-invalidation work that'd run on every
// subscribe/unsubscribe and on Phase 5 cross-frame panel reparenting.
bool is_owner_active_for_work(QObject* owner) {
    if (!owner) return true;
    auto* widget = qobject_cast<QWidget*>(owner);
    if (!widget) return true; // non-widget owner (a service) — always active
    QWidget* top = widget->window();
    if (!top) return true;
    const QVariant v = top->property("fincept.active_for_work");
    if (!v.isValid()) return true; // top isn't a frame that reports — assume active
    return v.toBool();
}
} // namespace

// ── Singleton ──────────────────────────────────────────────────────────────

DataHub& DataHub::instance() {
    static DataHub s;
    return s;
}

DataHub::DataHub() {
    // Hub lives on whichever thread first touched instance() — almost
    // always the GUI thread at app startup. moveToThread not needed;
    // publish() marshals cross-thread via queued invocation.
    scheduler_.setInterval(1000);  // 1 s tick — policies are ms, so this
                                   // is plenty of resolution for TTLs
                                   // measured in seconds.
    connect(&scheduler_, &QTimer::timeout, this, &DataHub::scheduler_body);
    scheduler_.start();

    // Coalesce timer — single-shot, armed by request(), fires once per window.
    // Default 100ms so widget burst subscriptions (dashboard + markets both
    // opening) collapse into one producer->refresh() per producer. Tunable
    // via set_coalesce_window_ms() — tests commonly use 0 for synchronous
    // dispatch.
    coalesce_timer_.setSingleShot(true);
    coalesce_timer_.setInterval(coalesce_window_ms_);
    connect(&coalesce_timer_, &QTimer::timeout, this, &DataHub::flush_coalesced_requests);

    LOG_INFO("DataHub", QString("Initialised (scheduler tick = 1s, coalesce window = %1ms)")
                            .arg(coalesce_window_ms_));
}

void DataHub::set_coalesce_window_ms(int ms) {
    if (ms < 0) ms = 0;
    coalesce_window_ms_ = ms;
    // Apply on the hub thread so the timer's internal state isn't touched
    // from a foreign thread.
    QMetaObject::invokeMethod(this, [this, ms]() {
        coalesce_timer_.setInterval(ms);
    }, Qt::QueuedConnection);
}

// ── Pattern match ──────────────────────────────────────────────────────────

bool DataHub::pattern_matches(const QString& pattern, const QString& topic) {
    // Only `*`-suffix wildcards are supported per DATAHUB_ARCHITECTURE.md §3.1.
    if (!pattern.endsWith(QLatin1Char('*')))
        return pattern == topic;
    const auto prefix = QStringView{pattern}.left(pattern.size() - 1);
    return topic.size() >= prefix.size() && QStringView{topic}.startsWith(prefix);
}

// ── Policy resolution ──────────────────────────────────────────────────────

TopicPolicy DataHub::resolve_policy(const QString& topic) const {
    // Caller holds mutex_.
    auto it = topics_.find(topic);
    if (it != topics_.end() && it->policy_explicit)
        return it->policy;
    // Pattern policies are searched in registration order; first match wins.
    for (const auto& pp : pattern_policies_)
        if (pattern_matches(pp.first, topic))
            return pp.second;
    return TopicPolicy{};  // defaults (30s TTL, 5s min interval, pull)
}

DataHub::TopicState& DataHub::state_for(const QString& topic) {
    // Caller holds mutex_.
    auto it = topics_.find(topic);
    if (it == topics_.end()) {
        TopicState fresh;
        fresh.policy = resolve_policy(topic);
        it = topics_.insert(topic, fresh);
    }
    return it.value();
}

Producer* DataHub::find_producer(const QString& topic) const {
    // Caller holds mutex_.
    // Pattern index is sorted by prefix length DESC so the first entry
    // whose prefix matches is also the most specific (longest-prefix win).
    for (const auto& e : pattern_index_) {
        if (e.is_wildcard) {
            if (topic.size() >= e.prefix.size() &&
                QStringView{topic}.startsWith(e.prefix))
                return e.owner;
        } else {
            if (topic == e.prefix)
                return e.owner;
        }
    }
    return nullptr;
}

void DataHub::rebuild_pattern_index() {
    // Caller holds mutex_. Called from register_producer / unregister_producer.
    pattern_index_.clear();
    for (Producer* p : producers_) {
        for (const auto& pat : p->topic_patterns()) {
            PatternEntry e;
            e.owner = p;
            if (pat.endsWith(QLatin1Char('*'))) {
                e.prefix = pat.left(pat.size() - 1);
                e.is_wildcard = true;
            } else {
                e.prefix = pat;
                e.is_wildcard = false;
            }
            pattern_index_.append(std::move(e));
        }
    }
    // Sort longest prefix first so iteration in find_producer() returns
    // the most specific match.
    std::sort(pattern_index_.begin(), pattern_index_.end(),
              [](const PatternEntry& a, const PatternEntry& b) {
                  return a.prefix.size() > b.prefix.size();
              });
}

// ── Subscription ───────────────────────────────────────────────────────────

void DataHub::on_owner_destroyed(QObject* owner) {
    // Note: owner is being destroyed, do NOT dereference it as QObject*.
    // We only use the pointer value as a map key.
    QStringList newly_idle;  // emitted outside the lock at the end
    {
        QMutexLocker lock(&mutex_);

        // Gap 1 fix: also drop owner from the per-topic error map. Done
        // first because the data unsubscribe path uses the same maps for
        // bookkeeping cleanup.
        if (auto eit = error_owner_topics_.find(owner); eit != error_owner_topics_.end()) {
            for (const auto& topic : eit.value()) {
                auto bucket = error_subscriptions_.find(topic);
                if (bucket == error_subscriptions_.end()) continue;
                bucket.value().erase(std::remove_if(bucket.value().begin(), bucket.value().end(),
                    [owner](const ErrorSub& e) { return e.owner.data() == owner; }), bucket.value().end());
                if (bucket.value().isEmpty()) error_subscriptions_.erase(bucket);
            }
            error_owner_topics_.erase(eit);
        }
        if (auto epit = error_owner_patterns_.find(owner); epit != error_owner_patterns_.end()) {
            for (const auto& pattern : epit.value()) {
                auto bucket = error_pattern_subscriptions_.find(pattern);
                if (bucket == error_pattern_subscriptions_.end()) continue;
                bucket.value().erase(std::remove_if(bucket.value().begin(), bucket.value().end(),
                    [owner](const ErrorSub& e) { return e.owner.data() == owner; }), bucket.value().end());
                if (bucket.value().isEmpty()) error_pattern_subscriptions_.erase(bucket);
            }
            error_owner_patterns_.erase(epit);
        }

        auto topics_it = owner_topics_.find(owner);
        if (topics_it != owner_topics_.end()) {
            for (const auto& topic : topics_it.value()) {
                auto sub_it = subscriptions_.find(topic);
                if (sub_it == subscriptions_.end()) continue;
                auto& vec = sub_it.value();
                vec.erase(std::remove_if(vec.begin(), vec.end(),
                             [owner](const Subscription& s) { return s.owner.data() == owner; }),
                          vec.end());
                if (vec.isEmpty()) {
                    subscriptions_.erase(sub_it);
                    // Topic just went idle — drop cached state if policy opted in.
                    if (auto ts_it = topics_.find(topic);
                        ts_it != topics_.end() && ts_it->policy.drop_on_idle) {
                        topics_.erase(ts_it);
                    }
                    newly_idle.append(topic);
                }
            }
            owner_topics_.erase(topics_it);
        }

        auto pats_it = owner_patterns_.find(owner);
        if (pats_it != owner_patterns_.end()) {
            for (const auto& pattern : pats_it.value()) {
                auto sub_it = pattern_subscriptions_.find(pattern);
                if (sub_it == pattern_subscriptions_.end()) continue;
                auto& vec = sub_it.value();
                vec.erase(std::remove_if(vec.begin(), vec.end(),
                             [owner](const Subscription& s) { return s.owner.data() == owner; }),
                          vec.end());
                if (vec.isEmpty())
                    pattern_subscriptions_.erase(sub_it);
            }
            owner_patterns_.erase(pats_it);
        }

        // Gap 4 fix: any topics that just lost their final subscriber must
        // also be removed from coalesce_pending_ — otherwise we'd dispatch
        // a producer refresh() for a topic nobody is listening to anymore.
        if (!newly_idle.isEmpty()) {
            QSet<QString> idle_set;
            for (const auto& t : newly_idle) idle_set.insert(t);
            for (auto it = coalesce_pending_.begin(); it != coalesce_pending_.end(); ) {
                it.value().erase(std::remove_if(it.value().begin(), it.value().end(),
                    [&idle_set](const QString& t) { return idle_set.contains(t); }),
                    it.value().end());
                if (it.value().isEmpty()) it = coalesce_pending_.erase(it);
                else ++it;
            }
        }
    }
    for (const auto& topic : newly_idle) emit topic_idle(topic);
}

void DataHub::deliver_initial_value(
    QObject* owner, const QString& topic,
    const std::function<void(const QVariant&)>& slot) {
    // Caller has released mutex_. We snapshot the value under the lock,
    // then deliver the slot on the owner's thread via queued invocation.
    QVariant snapshot;
    bool have_value = false;
    {
        QMutexLocker lock(&mutex_);
        auto it = topics_.find(topic);
        if (it != topics_.end() && it->value.isValid()) {
            const auto policy = it->policy;
            if (policy.push_only ||
                (now_ms() - it->last_publish_ms) < effective_ttl_ms(it.value())) {
                snapshot = it->value;
                have_value = true;
            }
        }
    }
    if (!have_value) return;

    QPointer<QObject> safe_owner = owner;
    QMetaObject::invokeMethod(owner, [safe_owner, slot, snapshot]() {
        if (safe_owner)
            slot(snapshot);
    }, Qt::QueuedConnection);
}

QMetaObject::Connection DataHub::subscribe(
    QObject* owner, const QString& topic,
    std::function<void(const QVariant&)> slot) {
    Q_ASSERT(owner && "subscribe requires a non-null owner for lifetime tracking");

    bool became_active = false;
    bool needs_cold_start_fetch = false;
    {
        QMutexLocker lock(&mutex_);
        Subscription sub;
        sub.owner = owner;
        sub.slot = slot;
        sub.is_pattern = false;

        auto& bucket = subscriptions_[topic];
        if (bucket.isEmpty()) became_active = true;
        bucket.append(std::move(sub));
        owner_topics_[owner].insert(topic);

        // Make sure the topic state exists with correct policy.
        auto& st = state_for(topic);

        // Cold-start fetch: if we have no cached value AND nothing is in flight,
        // request a forced refresh so the widget gets data on the first scheduler
        // tick rather than waiting for TTL/min_interval to line up. `push_only`
        // topics have no producer-side pull, so skip — they only get data via
        // explicit publish() calls.
        const bool have_fresh_value = st.value.isValid() && st.last_publish_ms > 0 &&
                                       (now_ms() - st.last_publish_ms) < effective_ttl_ms(st);
        if (!have_fresh_value && !st.in_flight && !st.policy.push_only) {
            needs_cold_start_fetch = true;
        }
    }

    // Auto-cleanup on owner destruction. The QObject::destroyed signal
    // fires on the owner's thread — marshal to ours via QueuedConnection
    // so mutex_ is acquired from the hub thread.
    // Gap 8 fix: bind to the member function (not a fresh lambda) so
    // Qt::UniqueConnection actually deduplicates the connection for
    // owners that re-subscribe repeatedly (show/hide cycles). Lambdas
    // would create a fresh slot pointer per call, defeating dedup.
    // The destroyed signal hands us the dying QObject* — use it directly
    // instead of capturing.
    auto conn = connect(owner, &QObject::destroyed, this,
        &DataHub::on_owner_destroyed,
        Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));

    if (became_active) {
        emit topic_active(topic);
    }
    deliver_initial_value(owner, topic, slot);

    // Dispatch the cold-start fetch outside the mutex. force=true bypasses
    // min_interval_ms but per-producer rate limit still applies.
    if (needs_cold_start_fetch) {
        request(topic, /*force=*/true);
    }
    return conn;
}

QMetaObject::Connection DataHub::subscribe_pattern(
    QObject* owner, const QString& pattern,
    std::function<void(const QString&, const QVariant&)> slot) {
    Q_ASSERT(owner && "subscribe_pattern requires a non-null owner");
    QStringList cold_start_topics;
    {
        QMutexLocker lock(&mutex_);
        Subscription sub;
        sub.owner = owner;
        sub.pattern_slot = slot;
        sub.is_pattern = true;
        pattern_subscriptions_[pattern].append(std::move(sub));
        owner_patterns_[owner].insert(pattern);

        // Cold-start parity with subscribe(): for every already-known topic
        // that matches this pattern and has no fresh cached value, queue a
        // forced refresh. Pattern-only subscribers (e.g. AgentErrorsWidget
        // listening to `agent:error:*`) otherwise have to wait for TTL
        // expiry + a scheduler tick before seeing anything.
        const qint64 t = now_ms();
        for (auto it = topics_.begin(); it != topics_.end(); ++it) {
            if (!pattern_matches(pattern, it.key())) continue;
            if (it->policy.push_only) continue;
            if (it->in_flight) continue;
            const bool have_fresh =
                it->value.isValid() && it->last_publish_ms > 0 &&
                (t - it->last_publish_ms) < effective_ttl_ms(it.value());
            if (!have_fresh) cold_start_topics.append(it.key());
        }
    }
    // Gap 8 fix: bind to the member function (not a fresh lambda) so
    // Qt::UniqueConnection actually deduplicates the connection for
    // owners that re-subscribe repeatedly (show/hide cycles). Lambdas
    // would create a fresh slot pointer per call, defeating dedup.
    // The destroyed signal hands us the dying QObject* — use it directly
    // instead of capturing.
    auto conn = connect(owner, &QObject::destroyed, this,
        &DataHub::on_owner_destroyed,
        Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));
    if (!cold_start_topics.isEmpty())
        request(cold_start_topics, /*force=*/true);
    return conn;
}

void DataHub::unsubscribe(QObject* owner) {
    on_owner_destroyed(owner);
}

void DataHub::unsubscribe(QObject* owner, const QString& topic) {
    bool became_idle = false;
    {
        QMutexLocker lock(&mutex_);
        auto sub_it = subscriptions_.find(topic);
        if (sub_it == subscriptions_.end()) {
            // Still need to clean owner_topics_ in case of bookkeeping skew.
            if (auto it = owner_topics_.find(owner); it != owner_topics_.end()) {
                it.value().remove(topic);
                if (it.value().isEmpty()) owner_topics_.erase(it);
            }
            return;
        }
        auto& vec = sub_it.value();
        vec.erase(std::remove_if(vec.begin(), vec.end(),
                     [owner](const Subscription& s) { return s.owner.data() == owner; }),
                  vec.end());
        if (vec.isEmpty()) {
            subscriptions_.erase(sub_it);
            if (auto ts_it = topics_.find(topic);
                ts_it != topics_.end() && ts_it->policy.drop_on_idle) {
                topics_.erase(ts_it);
            }
            became_idle = true;
            // Gap 4 fix: if the topic was buffered for a coalesced refresh,
            // drop it now — there's no point dispatching a producer call for
            // a topic that has no subscribers left.
            for (auto it = coalesce_pending_.begin(); it != coalesce_pending_.end(); ) {
                it.value().removeAll(topic);
                if (it.value().isEmpty()) it = coalesce_pending_.erase(it);
                else ++it;
            }
        }
        if (auto it = owner_topics_.find(owner); it != owner_topics_.end()) {
            it.value().remove(topic);
            if (it.value().isEmpty()) owner_topics_.erase(it);
        }
    }
    if (became_idle) emit topic_idle(topic);
}

void DataHub::unsubscribe_pattern(QObject* owner, const QString& pattern) {
    QMutexLocker lock(&mutex_);
    auto sub_it = pattern_subscriptions_.find(pattern);
    if (sub_it != pattern_subscriptions_.end()) {
        auto& vec = sub_it.value();
        vec.erase(std::remove_if(vec.begin(), vec.end(),
                     [owner](const Subscription& s) { return s.owner.data() == owner; }),
                  vec.end());
        if (vec.isEmpty()) pattern_subscriptions_.erase(sub_it);
    }
    if (auto it = owner_patterns_.find(owner); it != owner_patterns_.end()) {
        it.value().remove(pattern);
        if (it.value().isEmpty()) owner_patterns_.erase(it);
    }
}

// ── Error subscriptions (Gap 7 fix) ────────────────────────────────────────

QMetaObject::Connection DataHub::subscribe_errors(
    QObject* owner, const QString& topic,
    std::function<void(const QString&)> slot) {
    Q_ASSERT(owner && "subscribe_errors requires a non-null owner");
    {
        QMutexLocker lock(&mutex_);
        ErrorSub e;
        e.owner = owner;
        e.single_slot = std::move(slot);
        e.is_pattern = false;
        error_subscriptions_[topic].append(std::move(e));
        error_owner_topics_[owner].insert(topic);
    }
    return connect(owner, &QObject::destroyed, this,
        &DataHub::on_owner_destroyed,
        Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));
}

QMetaObject::Connection DataHub::subscribe_pattern_errors(
    QObject* owner, const QString& pattern,
    std::function<void(const QString&, const QString&)> slot) {
    Q_ASSERT(owner && "subscribe_pattern_errors requires a non-null owner");
    {
        QMutexLocker lock(&mutex_);
        ErrorSub e;
        e.owner = owner;
        e.pattern_slot = std::move(slot);
        e.is_pattern = true;
        error_pattern_subscriptions_[pattern].append(std::move(e));
        error_owner_patterns_[owner].insert(pattern);
    }
    return connect(owner, &QObject::destroyed, this,
        &DataHub::on_owner_destroyed,
        Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));
}

void DataHub::unsubscribe_errors(QObject* owner, const QString& topic) {
    QMutexLocker lock(&mutex_);
    if (auto it = error_subscriptions_.find(topic); it != error_subscriptions_.end()) {
        auto& vec = it.value();
        vec.erase(std::remove_if(vec.begin(), vec.end(),
                     [owner](const ErrorSub& e) { return e.owner.data() == owner; }),
                  vec.end());
        if (vec.isEmpty()) error_subscriptions_.erase(it);
    }
    if (auto it = error_owner_topics_.find(owner); it != error_owner_topics_.end()) {
        it.value().remove(topic);
        if (it.value().isEmpty()) error_owner_topics_.erase(it);
    }
}

void DataHub::unsubscribe_pattern_errors(QObject* owner, const QString& pattern) {
    QMutexLocker lock(&mutex_);
    if (auto it = error_pattern_subscriptions_.find(pattern); it != error_pattern_subscriptions_.end()) {
        auto& vec = it.value();
        vec.erase(std::remove_if(vec.begin(), vec.end(),
                     [owner](const ErrorSub& e) { return e.owner.data() == owner; }),
                  vec.end());
        if (vec.isEmpty()) error_pattern_subscriptions_.erase(it);
    }
    if (auto it = error_owner_patterns_.find(owner); it != error_owner_patterns_.end()) {
        it.value().remove(pattern);
        if (it.value().isEmpty()) error_owner_patterns_.erase(it);
    }
}

// ── Publishing ─────────────────────────────────────────────────────────────

void DataHub::emit_to_subscribers(const QString& topic, const QVariant& value) {
    // Snapshot slots + their owners under the lock; invoke without.
    // Phase 8: pause_when_inactive policy filters fanout to subscribers
    // whose top-level window reports `fincept.active_for_work == false`.
    // The cached value is already updated (do_publish writes it before
    // calling this method), so when the user brings the window forward
    // the next subscribe / peek immediately sees the latest.
    struct Pending {
        QPointer<QObject> owner;
        std::function<void(const QVariant&)> slot;
    };
    struct PendingPattern {
        QPointer<QObject> owner;
        std::function<void(const QString&, const QVariant&)> slot;
    };
    QVector<Pending> direct;
    QVector<PendingPattern> pattern;
    bool pause_when_inactive = false;
    {
        QMutexLocker lock(&mutex_);
        // Topic policy: explicit overrides win; otherwise check pattern
        // policies via the existing resolver. Cheap lookup since we're
        // already holding the lock for the subscription scan.
        if (auto it = topics_.find(topic); it != topics_.end())
            pause_when_inactive = it.value().policy.pause_when_inactive;

        if (auto it = subscriptions_.find(topic); it != subscriptions_.end()) {
            for (const auto& s : it.value())
                if (s.owner) direct.append({s.owner, s.slot});
        }
        for (auto it = pattern_subscriptions_.begin();
             it != pattern_subscriptions_.end(); ++it) {
            if (!pattern_matches(it.key(), topic)) continue;
            for (const auto& s : it.value())
                if (s.owner) pattern.append({s.owner, s.pattern_slot});
        }
    }

    int suppressed = 0;
    for (const auto& p : direct) {
        if (!p.owner) continue; // owner died between snapshot and invoke
        if (pause_when_inactive && !is_owner_active_for_work(p.owner.data())) {
            ++suppressed;
            continue;
        }
        p.slot(value);
    }
    for (const auto& p : pattern) {
        if (!p.owner) continue;
        if (pause_when_inactive && !is_owner_active_for_work(p.owner.data())) {
            ++suppressed;
            continue;
        }
        p.slot(topic, value);
    }

    // Topic-level signal mirrors fanout: fire only if at least one active
    // subscriber received the value (or pause is off). Avoids waking
    // global topic_updated listeners when the only consumers are
    // inactive-frame panels.
    const bool any_delivered =
        !pause_when_inactive ||
        (suppressed < direct.size() + pattern.size());
    if (any_delivered)
        emit topic_updated(topic, value);

    if (suppressed > 0) {
        LOG_DEBUG("DataHub", QString("Suppressed %1 inactive-frame fanout(s) for topic '%2'")
                                 .arg(suppressed).arg(topic));
    }
}

void DataHub::do_publish(const QString& topic, const QVariant& value,
                         std::chrono::milliseconds ttl_override) {
    int coalesce_ms = 0;
    bool defer = false;
    {
        QMutexLocker lock(&mutex_);
        auto& st = state_for(topic);
        st.value = value;
        st.last_publish_ms = now_ms();
        st.total_publishes += 1;
        st.in_flight = false;
        st.last_error.clear();  // successful publish clears any stored error
        // Gap 3 fix: store the per-publish TTL separately so the next
        // override-less publish reverts to policy.ttl_ms. Zero = no
        // override, fall back to policy.ttl_ms.
        st.ttl_override_ms = ttl_override.count() > 0
                                 ? static_cast<int>(ttl_override.count())
                                 : 0;
        coalesce_ms = st.policy.coalesce_within_ms;
        if (coalesce_ms > 0) {
            st.coalesce_latest = value;
            if (!st.coalesce_pending) {
                st.coalesce_pending = true;
                defer = true;  // we must arm the timer outside the lock
            }
            // else: a timer is already armed; it will pick up the new latest value
        }
    }
    if (coalesce_ms > 0) {
        if (defer) {
            // Arm a one-shot timer. Runs on hub thread (we're already there
            // inside do_publish, but be explicit for callers who invoke via
            // QueuedConnection).
            QTimer::singleShot(coalesce_ms, this, [this, topic]() {
                do_coalesced_flush(topic);
            });
        }
        // Fan-out is deferred until the timer fires.
        return;
    }
    emit_to_subscribers(topic, value);
}

void DataHub::do_coalesced_flush(const QString& topic) {
    QVariant value;
    {
        QMutexLocker lock(&mutex_);
        auto it = topics_.find(topic);
        if (it == topics_.end() || !it->coalesce_pending) return;
        value = it->coalesce_latest;
        it->coalesce_pending = false;
        it->coalesce_latest.clear();
    }
    emit_to_subscribers(topic, value);
}

void DataHub::publish(const QString& topic, const QVariant& value) {
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, [this, topic, value]() {
            do_publish(topic, value, std::chrono::milliseconds(0));
        }, Qt::QueuedConnection);
        return;
    }
    do_publish(topic, value, std::chrono::milliseconds(0));
}

void DataHub::publish(const QString& topic, const QVariant& value,
                      std::chrono::milliseconds ttl) {
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, [this, topic, value, ttl]() {
            do_publish(topic, value, ttl);
        }, Qt::QueuedConnection);
        return;
    }
    do_publish(topic, value, ttl);
}

void DataHub::publish_error(const QString& topic, const QString& error) {
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, [this, topic, error]() {
            publish_error(topic, error);
        }, Qt::QueuedConnection);
        return;
    }

    // Snapshot per-topic error subscribers under the lock so we invoke
    // their slots outside it (same pattern as emit_to_subscribers).
    struct PendingErr {
        QPointer<QObject> owner;
        std::function<void(const QString&)> single_slot;
        std::function<void(const QString&, const QString&)> pattern_slot;
        bool is_pattern = false;
    };
    QVector<PendingErr> error_targets;
    {
        QMutexLocker lock(&mutex_);
        auto& st = state_for(topic);
        st.in_flight = false;
        st.last_error = error;
        st.last_error_ms = now_ms();
        st.total_errors += 1;

        if (auto it = error_subscriptions_.find(topic); it != error_subscriptions_.end()) {
            for (const auto& e : it.value())
                if (e.owner) error_targets.append({e.owner, e.single_slot, {}, false});
        }
        for (auto it = error_pattern_subscriptions_.begin();
             it != error_pattern_subscriptions_.end(); ++it) {
            if (!pattern_matches(it.key(), topic)) continue;
            for (const auto& e : it.value())
                if (e.owner) error_targets.append({e.owner, {}, e.pattern_slot, true});
        }
    }
    for (const auto& t : error_targets) {
        if (!t.owner) continue;
        if (t.is_pattern) t.pattern_slot(topic, error);
        else              t.single_slot(error);
    }

    LOG_WARN("DataHub", QString("publish_error topic='%1' error='%2'").arg(topic).arg(error.left(200)));
    emit topic_error(topic, error);
}

// ── Producer registration ──────────────────────────────────────────────────

void DataHub::register_producer(Producer* producer) {
    Q_ASSERT(producer);
    QMutexLocker lock(&mutex_);
    if (!producers_.contains(producer)) {
        producers_.append(producer);
        rebuild_pattern_index();
    }
}

void DataHub::unregister_producer(Producer* producer) {
    QMutexLocker lock(&mutex_);
    producers_.removeAll(producer);
    producer_last_refresh_ms_.remove(producer);
    rebuild_pattern_index();
    // Drop any pending coalesced refresh requests targeting this producer —
    // dispatching them after unregister would call refresh() on a dangling
    // pointer. The topics involved have their `in_flight` flag cleared so
    // the next find_producer() on the scheduler pass routes them to a new
    // owner (if one registers) or leaves them alone.
    if (auto it = coalesce_pending_.find(producer); it != coalesce_pending_.end()) {
        for (const QString& topic : it.value()) {
            if (auto ts_it = topics_.find(topic); ts_it != topics_.end())
                ts_it->in_flight = false;
        }
        coalesce_pending_.erase(it);
    }
}

void DataHub::set_policy(const QString& topic, const TopicPolicy& policy) {
    QMutexLocker lock(&mutex_);
    auto& st = state_for(topic);
    st.policy = policy;
    st.policy_explicit = true;
}

void DataHub::set_policy_pattern(const QString& pattern, const TopicPolicy& policy) {
    QMutexLocker lock(&mutex_);
    pattern_policies_.append({pattern, policy});
    // Apply retroactively to any already-known topics whose state has no
    // explicit policy yet.
    for (auto it = topics_.begin(); it != topics_.end(); ++it) {
        if (!it->policy_explicit && pattern_matches(pattern, it.key()))
            it->policy = policy;
    }
}

int DataHub::clear_policy_pattern(const QString& pattern) {
    QMutexLocker lock(&mutex_);
    int removed = 0;
    for (auto it = pattern_policies_.begin(); it != pattern_policies_.end(); ) {
        if (it->first == pattern) {
            it = pattern_policies_.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}

// ── Pull-through ───────────────────────────────────────────────────────────

QVariant DataHub::peek(const QString& topic) const {
    QMutexLocker lock(&mutex_);
    auto it = topics_.find(topic);
    if (it == topics_.end() || !it->value.isValid())
        return {};
    // Push-only topics don't age — always return last published value.
    if (it->policy.push_only)
        return it->value;
    if (it->last_publish_ms <= 0)
        return {};
    if ((now_ms() - it->last_publish_ms) >= effective_ttl_ms(it.value()))
        return {};  // stale
    return it->value;
}

QVariant DataHub::peek_raw(const QString& topic) const {
    QMutexLocker lock(&mutex_);
    auto it = topics_.find(topic);
    return it == topics_.end() ? QVariant{} : it->value;
}

void DataHub::request(const QString& topic, bool force) {
    request(QStringList{topic}, force);
}

void DataHub::request(const QStringList& topics, bool force) {
    // Apply gates under the lock, then buffer into coalesce_pending_. The
    // coalesce timer (kCoalesceWindowMs) merges bursts of request() calls —
    // e.g., dashboard + markets subscribing simultaneously — into one
    // producer->refresh() per producer.
    //
    // Gates applied here (same as before — gating is at buffer time, not
    // flush time, because in_flight/min_interval reflect intent to fetch):
    //   - in_flight: always skip; the producer is already working.
    //   - min_interval_ms: skipped when `force=true` (user-driven refresh).
    //   - max_requests_per_sec: always honoured — force doesn't open the
    //     rate-limit floodgate.
    int accepted = 0;
    bool need_arm = false;
    QStringList orphan_topics;
    {
        QMutexLocker lock(&mutex_);
        const qint64 t = now_ms();
        for (const auto& topic : topics) {
            auto& st = state_for(topic);
            if (st.in_flight) continue;
            if (!force && st.policy.min_interval_ms > 0 &&
                st.last_refresh_request_ms > 0 &&
                (t - st.last_refresh_request_ms) < st.policy.min_interval_ms) {
                continue;
            }
            Producer* p = find_producer(topic);
            if (!p) {
                orphan_topics.append(topic);
                continue;
            }
            const int rps = p->max_requests_per_sec();
            if (rps > 0) {
                const qint64 last = producer_last_refresh_ms_.value(p, 0);
                const qint64 min_gap = 1000 / std::max(1, rps);
                if (last > 0 && (t - last) < min_gap) continue;
            }
            st.in_flight = true;
            st.last_refresh_request_ms = t;
            producer_last_refresh_ms_[p] = t;
            // Buffer instead of dispatching — duplicate topics within the
            // window are naturally prevented by the in_flight gate above.
            coalesce_pending_[p].append(topic);
            ++accepted;
        }
        need_arm = accepted > 0;
    }

    if (!orphan_topics.isEmpty()) {
        // Topic requested by a consumer but no producer claims a matching
        // pattern. This is almost always a missing
        // ensure_registered_with_hub() call in main.cpp (the producer
        // didn't run before the consumer subscribed) or a typo in the
        // topic string. Surface it loudly so the miss is obvious during
        // development — silent orphans cause "subscribe but never get
        // data" bugs that are hard to trace.
        LOG_ERROR("DataHub",
                  QString("ORPHAN TOPIC(S) — %1 topic(s) have no registered producer. "
                          "Check that the owning service ran ensure_registered_with_hub() "
                          "in main.cpp BEFORE any consumer subscribed, and verify the "
                          "topic string spelling. First few: %2")
                      .arg(orphan_topics.size())
                      .arg(orphan_topics.mid(0, 5).join(QStringLiteral(", "))));
    }
    if (need_arm) {
        // The coalesce timer lives on the hub thread; arming from another
        // thread requires a queued call. isSingleShot + not-active means
        // restart resets the window, which is the behaviour we want: a
        // second request() inside the window extends coalescing, it
        // doesn't flush early.
        QMetaObject::invokeMethod(this, [this]() {
            if (!coalesce_timer_.isActive())
                coalesce_timer_.start();
        }, Qt::QueuedConnection);
    }
}

void DataHub::flush_coalesced_requests() {
    QHash<Producer*, QStringList> work;
    {
        QMutexLocker lock(&mutex_);
        work.swap(coalesce_pending_);
    }
    if (work.isEmpty())
        return;

    int total = 0;
    for (auto it = work.begin(); it != work.end(); ++it) {
        total += it.value().size();
        LOG_DEBUG("DataHub", QString("coalesce flush: producer=%1 topics=%2")
                                 .arg(reinterpret_cast<quintptr>(it.key()), 0, 16)
                                 .arg(it.value().size()));
        it.key()->refresh(it.value());
    }
    if (total > 0) {
        LOG_DEBUG("DataHub", QString("coalesce flushed %1 topics across %2 producers")
                                 .arg(total).arg(work.size()));
    }
}

// ── Scheduler ──────────────────────────────────────────────────────────────

void DataHub::scheduler_body() {
    // One pass: collect refresh work per producer, respecting TTL, min
    // interval, in-flight, and per-producer rate limits.
    QHash<Producer*, QStringList> work;
    QStringList timed_out_topics;
    {
        QMutexLocker lock(&mutex_);
        const qint64 t = now_ms();

        // Pre-pass: clear `in_flight` on topics whose producer missed the
        // refresh_timeout_ms window. Without this a crashed / hung producer
        // pins the topic forever and subscribers never see fresh data.
        for (auto it = topics_.begin(); it != topics_.end(); ++it) {
            if (!it->in_flight) continue;
            if (it->policy.refresh_timeout_ms <= 0) continue;
            if (it->last_refresh_request_ms <= 0) continue;
            if ((t - it->last_refresh_request_ms) >= it->policy.refresh_timeout_ms) {
                it->in_flight = false;
                timed_out_topics.append(it.key());
            }
        }

        // Drop empty subscription buckets left behind by unsubscribe churn.
        // The scheduler would otherwise skip them forever while still paying
        // the hash iteration cost on every tick.
        for (auto it = subscriptions_.begin(); it != subscriptions_.end(); ) {
            if (it.value().isEmpty())
                it = subscriptions_.erase(it);
            else
                ++it;
        }

        for (auto it = subscriptions_.begin(); it != subscriptions_.end(); ++it) {
            const QString& topic = it.key();
            if (it.value().isEmpty()) continue;

            auto& st = state_for(topic);
            if (st.policy.push_only) continue;
            if (st.in_flight) continue;

            const bool have_value = st.value.isValid() && st.last_publish_ms > 0;
            const bool ttl_expired = !have_value ||
                (t - st.last_publish_ms) >= effective_ttl_ms(st);
            const bool min_interval_ok =
                (t - st.last_refresh_request_ms) >= st.policy.min_interval_ms;
            if (!ttl_expired || !min_interval_ok) continue;

            Producer* p = find_producer(topic);
            if (!p) continue;

            // Per-producer rate limit: if producer caps at N req/sec,
            // ensure at least (1000/N) ms between refresh() calls to it.
            const int rps = p->max_requests_per_sec();
            if (rps > 0) {
                const qint64 last = producer_last_refresh_ms_.value(p, 0);
                const qint64 min_gap = 1000 / std::max(1, rps);
                if (last > 0 && (t - last) < min_gap) continue;
            }

            st.in_flight = true;
            st.last_refresh_request_ms = t;
            producer_last_refresh_ms_[p] = t;
            work[p].append(topic);
        }
    }

    for (const QString& topic : timed_out_topics) {
        LOG_WARN("DataHub", QString("Producer refresh timed out for topic '%1' — clearing in_flight. "
                                     "The producer failed to publish() or publish_error() within "
                                     "refresh_timeout_ms; hub will retry on the next scheduler pass.").arg(topic));
        emit topic_error(topic, QStringLiteral("refresh_timeout"));
    }

    for (auto it = work.begin(); it != work.end(); ++it) {
        LOG_DEBUG("DataHub", QString("refresh(): producer=%1 topics=%2")
                                 .arg(reinterpret_cast<quintptr>(it.key()), 0, 16)
                                 .arg(it.value().join(QLatin1Char(','))));
        it.key()->refresh(it.value());
    }
}

void DataHub::tick_scheduler() {
    scheduler_body();
}

void DataHub::retire_topic(const QString& topic) {
    QMutexLocker lock(&mutex_);
    topics_.remove(topic);
}

// ── Stats ──────────────────────────────────────────────────────────────────

QVector<TopicStats> DataHub::stats() const {
    QMutexLocker lock(&mutex_);
    QVector<TopicStats> out;
    out.reserve(topics_.size());
    for (auto it = topics_.begin(); it != topics_.end(); ++it) {
        TopicStats s;
        s.topic = it.key();
        s.last_publish_ms = it->last_publish_ms;
        s.last_refresh_request_ms = it->last_refresh_request_ms;
        s.last_error_ms = it->last_error_ms;
        s.total_publishes = it->total_publishes;
        s.total_errors = it->total_errors;
        s.in_flight = it->in_flight;
        s.push_only = it->policy.push_only;
        s.last_error = it->last_error;
        if (auto sub_it = subscriptions_.find(it.key()); sub_it != subscriptions_.end())
            s.subscriber_count = sub_it.value().size();
        out.append(std::move(s));
    }
    std::sort(out.begin(), out.end(),
              [](const TopicStats& a, const TopicStats& b) { return a.topic < b.topic; });
    return out;
}

QList<QObject*> DataHub::subscribers(const QString& topic) const {
    QMutexLocker lock(&mutex_);
    QList<QObject*> out;
    auto it = subscriptions_.find(topic);
    if (it == subscriptions_.end()) return out;
    out.reserve(it.value().size());
    for (const auto& s : it.value()) {
        if (s.owner) out.append(s.owner.data());
    }
    return out;
}

} // namespace fincept::datahub
