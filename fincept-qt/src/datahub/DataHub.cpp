#include "datahub/DataHub.h"

#include "core/logging/Logger.h"

#include <QDateTime>
#include <QMetaObject>
#include <QMutexLocker>
#include <QRandomGenerator>
#include <QThread>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <iterator>

namespace fincept::datahub {

namespace {
qint64 now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}

// Error backoff ladder (ms), indexed by consecutive-failure count - 1 and
// capped at the last rung. A topic whose refresh keeps failing — a delisted
// ticker, Yahoo refusing requests, Python not installed — used to be
// re-requested every min_interval_ms (2 s for quotes) for as long as it had a
// subscriber, which kept the serial Python daemon busy and the upstream
// hammered at exactly the moment it was rate limiting.
constexpr qint64 hub_kErrorBackoffLadderMs[] = {2'000, 5'000, 15'000, 60'000, 300'000};

// Delay before a failed topic may be refetched automatically: the ladder rung
// for `streak`, raised to the producer's `hint_ms` if that is longer, then
// jittered by ±10 % so topics that failed in one batch don't all come due on
// the same scheduler tick.
qint64 hub_backoff_delay_ms(int streak, qint64 hint_ms) {
    constexpr int kRungs = static_cast<int>(std::size(hub_kErrorBackoffLadderMs));
    const int idx = std::clamp(streak - 1, 0, kRungs - 1);
    qint64 delay = std::max(hub_kErrorBackoffLadderMs[idx], hint_ms);
    const qint64 span = delay / 10;
    if (span > 0 && span < (1 << 29))
        delay += QRandomGenerator::global()->bounded(static_cast<int>(2 * span + 1)) - span;
    return delay;
}

// True while a failed topic is inside its backoff window.
template <typename TopicStateT>
inline bool hub_in_backoff(const TopicStateT& st, qint64 t) {
    return st.retry_after_ms > t;
}

// Gap 3 helper: returns the freshness window in ms for a topic state.
// Picks the per-publish override if set; otherwise the policy TTL.
template <typename TopicStateT>
inline int effective_ttl_ms(const TopicStateT& st) {
    return st.ttl_override_ms > 0 ? st.ttl_override_ms : st.policy.ttl_ms;
}

// ── Log-line helpers for topic failures (logging policy B.2.4 / B.8) ───────
// A whole topic family (market:quote:*) usually fails at once for one reason
// (Python not set up, worker shutting down): the per-topic lines are folded per
// family + reason with LOG_*_RL, so the first topic is written and the rest are
// counted ("suppressed=N").

// "market:quote:AAPL" -> "market:quote"
QString hub_topic_family(const QString& topic) {
    const int first = topic.indexOf(QLatin1Char(':'));
    if (first < 0)
        return topic;
    const int second = topic.indexOf(QLatin1Char(':'), first + 1);
    return second < 0 ? topic : topic.left(second);
}

// Reason with the topic's own tail (the symbol) replaced by '*', so
// "No recent closing price for AAPL" and "… for MSFT" share one key.
QString hub_reason_key(const QString& topic, const QString& reason) {
    const QString family = hub_topic_family(topic);
    QString key = reason.left(120);
    if (family.size() + 1 < topic.size()) {
        const QString tail = topic.mid(family.size() + 1);
        if (tail.size() >= 2)
            key.replace(tail, QStringLiteral("*"), Qt::CaseInsensitive);
    }
    return family + QLatin1Char('|') + key;
}

// Reasons that describe a state the user sees and can resolve (or that ends by
// itself), not a fault: logged at INFO instead of WARN.
bool hub_is_expected_refusal(const QString& reason) {
    static const QLatin1StringView kMarkers[] = {
        QLatin1StringView("python is not set up"),  QLatin1StringView("shutting down"),
        QLatin1StringView("sign in"),               QLatin1StringView("credits"),
        QLatin1StringView("key is required"),       QLatin1StringView("key required"),
        QLatin1StringView("not configured"),        QLatin1StringView("environment variable not set"),
        QLatin1StringView("is not installed"),
    };
    for (const auto& m : kMarkers)
        if (reason.contains(m, Qt::CaseInsensitive))
            return true;
    return false;
}

// Set by DataHub::unsubscribe(owner) around its call into on_owner_destroyed().
// thread_local: unsubscribe() may be called from any thread.
thread_local bool g_datahub_explicit_unsubscribe = false;

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
    if (!owner)
        return true;
    auto* widget = qobject_cast<QWidget*>(owner);
    if (!widget)
        return true; // non-widget owner (a service) — always active
    QWidget* top = widget->window();
    if (!top)
        return true;
    const QVariant v = top->property("fincept.active_for_work");
    if (!v.isValid())
        return true; // top isn't a frame that reports — assume active
    return v.toBool();
}

// Fetch gating for pause_when_inactive topics: does this subscriber need the
// data now? A widget does only while it is shown (QWidget::isVisible() — false
// in a hidden tab / stack page / closed panel, and before the first show) and
// its window is not minimised. Every other owner — services, stores, plain
// QObject owners — always counts. Widgets are touched only on their own thread;
// a call from another thread (subscribe() off the GUI thread) treats the owner
// as visible: an extra fetch beats a starved subscriber. Reads Qt state only —
// safe with the hub mutex held.
bool hub_is_owner_visible_for_work(QObject* owner) {
    if (!owner)
        return false;
    if (owner->thread() != QThread::currentThread())
        return true;
    auto* widget = qobject_cast<QWidget*>(owner);
    if (!widget)
        return true;
    return widget->isVisible() && is_owner_active_for_work(owner);
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
    scheduler_.setInterval(1000); // 1 s tick — policies are ms, so this
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

    LOG_DEBUG("DataHub", QString("Initialised (scheduler tick = 1s, coalesce window = %1ms)").arg(coalesce_window_ms_));
}

void DataHub::set_coalesce_window_ms(int ms) {
    if (ms < 0)
        ms = 0;
    coalesce_window_ms_ = ms;
    // Apply on the hub thread so the timer's internal state isn't touched
    // from a foreign thread.
    QMetaObject::invokeMethod(this, [this, ms]() { coalesce_timer_.setInterval(ms); }, Qt::QueuedConnection);
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
    return TopicPolicy{}; // defaults (30s TTL, 5s min interval, pull)
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

void DataHub::clear_undispatched_in_flight(const QString& topic) {
    // Caller holds mutex_.
    if (auto it = topics_.find(topic); it != topics_.end())
        it->in_flight = false;
}

Producer* DataHub::find_producer(const QString& topic) const {
    // Caller holds mutex_.
    // Pattern index is sorted by prefix length DESC so the first entry
    // whose prefix matches is also the most specific (longest-prefix win).
    for (const auto& e : pattern_index_) {
        if (e.is_wildcard) {
            if (topic.size() >= e.prefix.size() && QStringView{topic}.startsWith(e.prefix))
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
              [](const PatternEntry& a, const PatternEntry& b) { return a.prefix.size() > b.prefix.size(); });
}

// ── Subscription ───────────────────────────────────────────────────────────

void DataHub::on_owner_destroyed(QObject* owner) {
    // Two callers reach this function, and they need different matching rules:
    //
    //  * unsubscribe(owner) — the owner is ALIVE, so its entries are the ones whose
    //    QPointer still equals `owner`.
    //  * the queued QObject::destroyed connection — by the time it runs the owner is
    //    gone, so every QPointer that referred to it is already NULL. Matching on
    //    `s.owner.data() == owner` therefore never removed anything: the dead owner's
    //    Subscription objects (and their captured slots) stayed in the buckets
    //    forever, the bucket never emptied so `topic_idle` never fired, and the
    //    scheduler kept refreshing the topic for a subscriber that no longer exists.
    //    Worse, the pointer VALUE may already belong to a new object (allocators
    //    reuse freed addresses), whose live subscriptions must not be removed.
    //
    // So: a null owner is always stale; a pointer-equal owner is removed only on the
    // explicit path.
    const bool owner_alive = g_datahub_explicit_unsubscribe;
    auto is_stale = [owner, owner_alive](const QPointer<QObject>& p) {
        return !p || (owner_alive && p.data() == owner);
    };

    // Drops `owner`'s stale entries from every bucket it registered in and returns
    // the keys of buckets that became empty. On the destroyed path the owner's
    // bookkeeping entry is kept for any key where a live object with the same
    // address has since subscribed.
    auto purge = [&](auto& buckets, auto& owner_map) {
        QStringList emptied;
        auto oit = owner_map.find(owner);
        if (oit == owner_map.end())
            return emptied;
        QSet<QString> keep;
        for (const QString& key : oit.value()) {
            auto bit = buckets.find(key);
            if (bit == buckets.end())
                continue;
            auto& vec = bit.value();
            vec.erase(std::remove_if(vec.begin(), vec.end(), [&](const auto& s) { return is_stale(s.owner); }),
                      vec.end());
            if (!owner_alive && std::any_of(vec.begin(), vec.end(),
                                            [owner](const auto& s) { return s.owner.data() == owner; }))
                keep.insert(key);
            if (vec.isEmpty()) {
                buckets.erase(bit);
                emptied.append(key);
            }
        }
        if (keep.isEmpty())
            owner_map.erase(oit);
        else
            oit.value() = keep;
        return emptied;
    };

    QStringList newly_idle; // emitted outside the lock at the end
    {
        QMutexLocker lock(&mutex_);

        // Gap 1 fix: also drop owner from the per-topic error maps. Done
        // first because the data unsubscribe path uses the same maps for
        // bookkeeping cleanup.
        purge(error_subscriptions_, error_owner_topics_);
        purge(error_pattern_subscriptions_, error_owner_patterns_);

        newly_idle = purge(subscriptions_, owner_topics_);
        for (const auto& topic : newly_idle) {
            // Topic just went idle — drop cached state if policy opted in.
            if (auto ts_it = topics_.find(topic); ts_it != topics_.end() && ts_it->policy.drop_on_idle)
                topics_.erase(ts_it);
        }

        purge(pattern_subscriptions_, owner_patterns_);

        // Gap 4 fix: any topics that just lost their final subscriber must
        // also be removed from coalesce_pending_ — otherwise we'd dispatch
        // a producer refresh() for a topic nobody is listening to anymore.
        if (!newly_idle.isEmpty()) {
            QSet<QString> idle_set;
            for (const auto& t : newly_idle)
                idle_set.insert(t);
            for (auto it = coalesce_pending_.begin(); it != coalesce_pending_.end();) {
                it.value().erase(std::remove_if(it.value().begin(), it.value().end(),
                                                [this, &idle_set](const QString& t) {
                                                    if (!idle_set.contains(t))
                                                        return false;
                                                    clear_undispatched_in_flight(t);
                                                    return true;
                                                }),
                                 it.value().end());
                if (it.value().isEmpty())
                    it = coalesce_pending_.erase(it);
                else
                    ++it;
            }
        }
    }
    for (const auto& topic : newly_idle)
        emit topic_idle(topic);
}

void DataHub::deliver_initial_value(QObject* owner, const QString& topic,
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
            if (policy.push_only || (now_ms() - it->last_publish_ms) < effective_ttl_ms(it.value())) {
                snapshot = it->value;
                have_value = true;
            }
        }
    }
    if (!have_value)
        return;

    QPointer<QObject> safe_owner = owner;
    QMetaObject::invokeMethod(
        owner,
        [safe_owner, slot, snapshot]() {
            if (safe_owner)
                slot(snapshot);
        },
        Qt::QueuedConnection);
}

void DataHub::deliver_initial_errors(QObject* owner, const QHash<QString, QPair<QString, qint64>>& errors,
                                     const std::function<void(const QString&)>& single_slot,
                                     const std::function<void(const QString&, const QString&)>& pattern_slot) {
    // Caller has released mutex_. Queued to the owner's thread like
    // deliver_initial_value(), so the slot never runs re-entrantly inside the
    // caller's subscribe_errors() call. At delivery time the topic must still
    // hold the very error we snapshotted: if it published since, the error is
    // stale; if it failed again, the live fan-out already told this owner.
    // Nor is it replayed while a refetch is in flight — typically the
    // caller's own request(force=true) right after subscribing — because
    // that attempt's outcome (value or live error) is what the caller wants,
    // and a stale error landing first would settle one-shot consumers wrongly.
    if (errors.isEmpty())
        return;
    QPointer<QObject> safe_owner = owner;
    for (auto it = errors.cbegin(); it != errors.cend(); ++it) {
        const QString topic = it.key();
        const QString error = it.value().first;
        const qint64 error_ms = it.value().second;
        QMetaObject::invokeMethod(
            owner,
            [this, safe_owner, topic, error, error_ms, single_slot, pattern_slot]() {
                if (!safe_owner)
                    return;
                {
                    QMutexLocker lock(&mutex_);
                    auto ts = topics_.constFind(topic);
                    if (ts == topics_.constEnd() || ts->last_error.isEmpty() || ts->last_error_ms != error_ms ||
                        ts->in_flight)
                        return;
                }
                if (pattern_slot)
                    pattern_slot(topic, error);
                else if (single_slot)
                    single_slot(error);
            },
            Qt::QueuedConnection);
    }
}

QMetaObject::Connection DataHub::subscribe(QObject* owner, const QString& topic,
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
        if (bucket.isEmpty())
            became_active = true;
        bucket.append(std::move(sub));
        owner_topics_[owner].insert(topic);

        // Make sure the topic state exists with correct policy.
        auto& st = state_for(topic);

        // Cold-start fetch: if we have no cached value AND nothing is in flight,
        // request a forced refresh so the widget gets data on the first scheduler
        // tick rather than waiting for TTL/min_interval to line up. `push_only`
        // topics have no producer-side pull, so skip — they only get data via
        // explicit publish() calls. A topic backing off after a failure is
        // not refetched for a new subscriber either: the failure is already
        // known (subscribe_errors() replays it) and re-asking at once is what
        // made a dead ticker cost a request per screen open.
        const qint64 t = now_ms();
        const bool have_fresh_value =
            st.value.isValid() && st.last_publish_ms > 0 && (t - st.last_publish_ms) < effective_ttl_ms(st);
        if (!have_fresh_value && !st.in_flight && !st.policy.push_only && !hub_in_backoff(st, t)) {
            // A hidden widget does not trigger a fetch of a pause_when_inactive
            // topic: the topic has no fresh value, so the scheduler fetches it
            // on its first tick after a subscriber becomes visible.
            if (st.policy.pause_when_inactive && !has_visible_subscriber(topic))
                note_hidden_skip(st, t);
            else
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
    auto conn = connect(owner, &QObject::destroyed, this, &DataHub::on_owner_destroyed,
                        Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));

    if (became_active) {
        emit topic_active(topic);
    }
    deliver_initial_value(owner, topic, slot);

    // Dispatch the cold-start fetch outside the mutex. force=true bypasses
    // min_interval_ms but per-producer rate limit still applies. Not
    // user-initiated: an error backoff that started meanwhile is respected.
    if (needs_cold_start_fetch) {
        request_impl(QStringList{topic}, /*force=*/true, /*user_initiated=*/false);
    }
    return conn;
}

QMetaObject::Connection DataHub::subscribe_pattern(QObject* owner, const QString& pattern,
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
            if (!pattern_matches(pattern, it.key()))
                continue;
            if (it->policy.push_only)
                continue;
            if (it->in_flight)
                continue;
            if (hub_in_backoff(it.value(), t))
                continue;
            const bool have_fresh = it->value.isValid() && it->last_publish_ms > 0 &&
                                    (t - it->last_publish_ms) < effective_ttl_ms(it.value());
            if (have_fresh)
                continue;
            // Same rule as subscribe(): nobody visible, no fetch.
            if (it->policy.pause_when_inactive && !has_visible_subscriber(it.key()))
                note_hidden_skip(it.value(), t);
            else
                cold_start_topics.append(it.key());
        }
    }
    // Gap 8 fix: bind to the member function (not a fresh lambda) so
    // Qt::UniqueConnection actually deduplicates the connection for
    // owners that re-subscribe repeatedly (show/hide cycles). Lambdas
    // would create a fresh slot pointer per call, defeating dedup.
    // The destroyed signal hands us the dying QObject* — use it directly
    // instead of capturing.
    auto conn = connect(owner, &QObject::destroyed, this, &DataHub::on_owner_destroyed,
                        Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));
    if (!cold_start_topics.isEmpty())
        request_impl(cold_start_topics, /*force=*/true, /*user_initiated=*/false);
    return conn;
}

void DataHub::unsubscribe(QObject* owner) {
    // Explicit removal of a LIVE owner: match its entries by identity (see
    // on_owner_destroyed for why the destroyed path cannot).
    const bool prev = g_datahub_explicit_unsubscribe;
    g_datahub_explicit_unsubscribe = true;
    on_owner_destroyed(owner);
    g_datahub_explicit_unsubscribe = prev;
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
                if (it.value().isEmpty())
                    owner_topics_.erase(it);
            }
            return;
        }
        auto& vec = sub_it.value();
        vec.erase(
            std::remove_if(vec.begin(), vec.end(), [owner](const Subscription& s) { return s.owner.data() == owner; }),
            vec.end());
        if (vec.isEmpty()) {
            subscriptions_.erase(sub_it);
            if (auto ts_it = topics_.find(topic); ts_it != topics_.end() && ts_it->policy.drop_on_idle) {
                topics_.erase(ts_it);
            }
            became_idle = true;
            // Gap 4 fix: if the topic was buffered for a coalesced refresh,
            // drop it now — there's no point dispatching a producer call for
            // a topic that has no subscribers left.
            for (auto it = coalesce_pending_.begin(); it != coalesce_pending_.end();) {
                if (it.value().removeAll(topic) > 0)
                    clear_undispatched_in_flight(topic);
                if (it.value().isEmpty())
                    it = coalesce_pending_.erase(it);
                else
                    ++it;
            }
        }
        if (auto it = owner_topics_.find(owner); it != owner_topics_.end()) {
            it.value().remove(topic);
            if (it.value().isEmpty())
                owner_topics_.erase(it);
        }
    }
    if (became_idle)
        emit topic_idle(topic);
}

void DataHub::unsubscribe_pattern(QObject* owner, const QString& pattern) {
    QMutexLocker lock(&mutex_);
    auto sub_it = pattern_subscriptions_.find(pattern);
    if (sub_it != pattern_subscriptions_.end()) {
        auto& vec = sub_it.value();
        vec.erase(
            std::remove_if(vec.begin(), vec.end(), [owner](const Subscription& s) { return s.owner.data() == owner; }),
            vec.end());
        if (vec.isEmpty())
            pattern_subscriptions_.erase(sub_it);
    }
    if (auto it = owner_patterns_.find(owner); it != owner_patterns_.end()) {
        it.value().remove(pattern);
        if (it.value().isEmpty())
            owner_patterns_.erase(it);
    }
}

// ── Error subscriptions (Gap 7 fix) ────────────────────────────────────────

QMetaObject::Connection DataHub::subscribe_errors(QObject* owner, const QString& topic,
                                                  std::function<void(const QString&)> slot) {
    Q_ASSERT(owner && "subscribe_errors requires a non-null owner");
    // A late subscriber to a topic that is already failing is told the current
    // error, mirroring how subscribe() hands over a cached value. Without it a
    // widget opened after the failure would wait for the next retry — which,
    // with error backoff, can be minutes away.
    QHash<QString, QPair<QString, qint64>> current;
    {
        QMutexLocker lock(&mutex_);
        ErrorSub e;
        e.owner = owner;
        e.single_slot = slot;
        e.is_pattern = false;
        error_subscriptions_[topic].append(std::move(e));
        error_owner_topics_[owner].insert(topic);
        if (auto it = topics_.constFind(topic); it != topics_.constEnd() && !it->last_error.isEmpty())
            current.insert(topic, {it->last_error, it->last_error_ms});
    }
    auto conn = connect(owner, &QObject::destroyed, this, &DataHub::on_owner_destroyed,
                        Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));
    deliver_initial_errors(owner, current, slot, {});
    return conn;
}

QMetaObject::Connection DataHub::subscribe_pattern_errors(QObject* owner, const QString& pattern,
                                                          std::function<void(const QString&, const QString&)> slot) {
    Q_ASSERT(owner && "subscribe_pattern_errors requires a non-null owner");
    QHash<QString, QPair<QString, qint64>> current; // replayed below, see subscribe_errors()
    {
        QMutexLocker lock(&mutex_);
        ErrorSub e;
        e.owner = owner;
        e.pattern_slot = slot;
        e.is_pattern = true;
        error_pattern_subscriptions_[pattern].append(std::move(e));
        error_owner_patterns_[owner].insert(pattern);
        for (auto it = topics_.constBegin(); it != topics_.constEnd(); ++it) {
            if (!it->last_error.isEmpty() && pattern_matches(pattern, it.key()))
                current.insert(it.key(), {it->last_error, it->last_error_ms});
        }
    }
    auto conn = connect(owner, &QObject::destroyed, this, &DataHub::on_owner_destroyed,
                        Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));
    deliver_initial_errors(owner, current, {}, slot);
    return conn;
}

void DataHub::unsubscribe_errors(QObject* owner, const QString& topic) {
    QMutexLocker lock(&mutex_);
    if (auto it = error_subscriptions_.find(topic); it != error_subscriptions_.end()) {
        auto& vec = it.value();
        vec.erase(
            std::remove_if(vec.begin(), vec.end(), [owner](const ErrorSub& e) { return e.owner.data() == owner; }),
            vec.end());
        if (vec.isEmpty())
            error_subscriptions_.erase(it);
    }
    if (auto it = error_owner_topics_.find(owner); it != error_owner_topics_.end()) {
        it.value().remove(topic);
        if (it.value().isEmpty())
            error_owner_topics_.erase(it);
    }
}

void DataHub::unsubscribe_pattern_errors(QObject* owner, const QString& pattern) {
    QMutexLocker lock(&mutex_);
    if (auto it = error_pattern_subscriptions_.find(pattern); it != error_pattern_subscriptions_.end()) {
        auto& vec = it.value();
        vec.erase(
            std::remove_if(vec.begin(), vec.end(), [owner](const ErrorSub& e) { return e.owner.data() == owner; }),
            vec.end());
        if (vec.isEmpty())
            error_pattern_subscriptions_.erase(it);
    }
    if (auto it = error_owner_patterns_.find(owner); it != error_owner_patterns_.end()) {
        it.value().remove(pattern);
        if (it.value().isEmpty())
            error_owner_patterns_.erase(it);
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
                if (s.owner)
                    direct.append({s.owner, s.slot});
        }
        for (auto it = pattern_subscriptions_.begin(); it != pattern_subscriptions_.end(); ++it) {
            if (!pattern_matches(it.key(), topic))
                continue;
            for (const auto& s : it.value())
                if (s.owner)
                    pattern.append({s.owner, s.pattern_slot});
        }
    }

    int suppressed = 0;
    for (const auto& p : direct) {
        if (!p.owner)
            continue; // owner died between snapshot and invoke
        if (pause_when_inactive && !is_owner_active_for_work(p.owner.data())) {
            ++suppressed;
            continue;
        }
        p.slot(value);
    }
    for (const auto& p : pattern) {
        if (!p.owner)
            continue;
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
    const bool any_delivered = !pause_when_inactive || (suppressed < direct.size() + pattern.size());
    if (any_delivered)
        emit topic_updated(topic, value);

    if (suppressed > 0) {
        LOG_DEBUG("DataHub",
                  QString("Suppressed %1 inactive-frame fanout(s) for topic '%2'").arg(suppressed).arg(topic));
    }
}

void DataHub::do_publish(const QString& topic, const QVariant& value, std::chrono::milliseconds ttl_override) {
    int coalesce_ms = 0;
    bool defer = false;
    int recovered_after = 0; // failures this publish ended, for the recovery log line
    {
        QMutexLocker lock(&mutex_);
        auto& st = state_for(topic);
        st.value = value;
        st.last_publish_ms = now_ms();
        st.total_publishes += 1;
        st.in_flight = false;
        if (!st.last_error.isEmpty())
            recovered_after = std::max(st.error_streak, 1);
        st.last_error.clear(); // successful publish clears any stored error
        st.error_streak = 0;   // ... and the error backoff
        st.retry_after_ms = 0;
        // Gap 3 fix: store the per-publish TTL separately so the next
        // override-less publish reverts to policy.ttl_ms. Zero = no
        // override, fall back to policy.ttl_ms.
        st.ttl_override_ms = ttl_override.count() > 0 ? static_cast<int>(ttl_override.count()) : 0;
        coalesce_ms = st.policy.coalesce_within_ms;
        if (coalesce_ms > 0) {
            st.coalesce_latest = value;
            if (!st.coalesce_pending) {
                st.coalesce_pending = true;
                defer = true; // we must arm the timer outside the lock
            }
            // else: a timer is already armed; it will pick up the new latest value
        }
    }
    // One line per topic family within 10 s; the other topics of the family are counted.
    if (recovered_after > 0)
        LOG_INFO_RL("DataHub", QStringLiteral("recovered|") + hub_topic_family(topic), 10000,
                    logf("Topic recovered", {{"topic", topic}, {"failed_refreshes", recovered_after}}));
    if (coalesce_ms > 0) {
        if (defer) {
            // Arm a one-shot timer. Runs on hub thread (we're already there
            // inside do_publish, but be explicit for callers who invoke via
            // QueuedConnection).
            QTimer::singleShot(coalesce_ms, this, [this, topic]() { do_coalesced_flush(topic); });
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
        if (it == topics_.end() || !it->coalesce_pending)
            return;
        value = it->coalesce_latest;
        it->coalesce_pending = false;
        it->coalesce_latest.clear();
    }
    emit_to_subscribers(topic, value);
}

void DataHub::publish(const QString& topic, const QVariant& value) {
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(
            this, [this, topic, value]() { do_publish(topic, value, std::chrono::milliseconds(0)); },
            Qt::QueuedConnection);
        return;
    }
    do_publish(topic, value, std::chrono::milliseconds(0));
}

void DataHub::publish(const QString& topic, const QVariant& value, std::chrono::milliseconds ttl) {
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(
            this, [this, topic, value, ttl]() { do_publish(topic, value, ttl); }, Qt::QueuedConnection);
        return;
    }
    do_publish(topic, value, ttl);
}

void DataHub::publish_error(const QString& topic, const QString& error) {
    publish_error(topic, error, std::chrono::milliseconds(0));
}

void DataHub::publish_error(const QString& topic, const QString& error_in, std::chrono::milliseconds retry_after) {
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(
            this, [this, topic, error_in, retry_after]() { publish_error(topic, error_in, retry_after); },
            Qt::QueuedConnection);
        return;
    }

    // An empty string would read as "no error" everywhere last_error is
    // consulted (replay to late subscribers, last_error(), stats).
    const QString error = error_in.trimmed().isEmpty() ? QStringLiteral("refresh failed") : error_in;

    // Snapshot per-topic error subscribers under the lock so we invoke
    // their slots outside it (same pattern as emit_to_subscribers).
    struct PendingErr {
        QPointer<QObject> owner;
        std::function<void(const QString&)> single_slot;
        std::function<void(const QString&, const QString&)> pattern_slot;
        bool is_pattern = false;
    };
    QVector<PendingErr> error_targets;
    bool was_failing = false;
    bool reason_changed = false;
    int streak = 0;
    qint64 delay_ms = 0;
    {
        QMutexLocker lock(&mutex_);
        auto& st = state_for(topic);
        const qint64 t = now_ms();
        was_failing = !st.last_error.isEmpty();
        reason_changed = st.last_error != error;
        st.in_flight = false;
        st.last_error = error;
        st.last_error_ms = t;
        st.total_errors += 1;

        // Back off before the next automatic refetch. Updated before any slot
        // runs so a slot that calls request() sees the new window.
        st.error_streak += 1;
        streak = st.error_streak;
        delay_ms = hub_backoff_delay_ms(streak, std::max<qint64>(0, retry_after.count()));
        st.retry_after_ms = t + delay_ms;

        if (auto it = error_subscriptions_.find(topic); it != error_subscriptions_.end()) {
            for (const auto& e : it.value())
                if (e.owner)
                    error_targets.append({e.owner, e.single_slot, {}, false});
        }
        for (auto it = error_pattern_subscriptions_.begin(); it != error_pattern_subscriptions_.end(); ++it) {
            if (!pattern_matches(it.key(), topic))
                continue;
            for (const auto& e : it.value())
                if (e.owner)
                    error_targets.append({e.owner, {}, e.pattern_slot, true});
        }
    }
    for (const auto& t : error_targets) {
        if (!t.owner)
            continue;
        if (t.is_pattern)
            t.pattern_slot(topic, error);
        else
            t.single_slot(error);
    }

    // State changes only: one line when a topic starts failing and one when the
    // reason changes; repeats of the same failure on later retries go to DEBUG.
    // A family failing for one reason at once (every quote topic when Python is
    // not set up) writes its first topic and counts the rest (10 s per family +
    // reason). Expected refusals (setup, sign-in, credits, missing key, shutdown)
    // are INFO, not WARN.
    const qint64 retry_in_s = (delay_ms + 500) / 1000;
    if (reason_changed) {
        const QString rl_key = (was_failing ? QStringLiteral("changed|") : QStringLiteral("failing|")) +
                               hub_reason_key(topic, error);
        const char* sentence = was_failing ? "Topic failure reason changed" : "Topic failing";
        if (hub_is_expected_refusal(error)) {
            LOG_INFO_RL("DataHub", rl_key, 10000,
                        logf(QLatin1StringView(sentence), {{"topic", topic},
                                                           {"reason", error},
                                                           {"attempt", streak},
                                                           {"retry_in_s", retry_in_s}}));
        } else {
            LOG_WARN_RL("DataHub", rl_key, 10000,
                        logf(QLatin1StringView(sentence), {{"topic", topic},
                                                           {"reason", error},
                                                           {"attempt", streak},
                                                           {"retry_in_s", retry_in_s}}));
        }
    } else {
        LOG_DEBUG("DataHub", logf("Topic still failing", {{"topic", topic}, {"attempt", streak},
                                                          {"retry_in_s", retry_in_s}}));
    }
    emit topic_error(topic, error);
}

void DataHub::clear_backoff(const QString& pattern) {
    int cleared = 0;
    {
        QMutexLocker lock(&mutex_);
        for (auto it = topics_.begin(); it != topics_.end(); ++it) {
            if (it->retry_after_ms == 0 && it->error_streak == 0)
                continue;
            if (!pattern_matches(pattern, it.key()))
                continue;
            it->retry_after_ms = 0;
            it->error_streak = 0;
            ++cleared;
        }
    }
    if (cleared > 0)
        LOG_INFO("DataHub", QString("Cleared error backoff on %1 topic(s) matching '%2'").arg(cleared).arg(pattern));
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
    for (auto it = pattern_policies_.begin(); it != pattern_policies_.end();) {
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
        return {}; // stale
    return it->value;
}

QVariant DataHub::peek_raw(const QString& topic) const {
    QMutexLocker lock(&mutex_);
    auto it = topics_.find(topic);
    return it == topics_.end() ? QVariant{} : it->value;
}

qint64 DataHub::last_publish_ms(const QString& topic) const {
    QMutexLocker lock(&mutex_);
    auto it = topics_.find(topic);
    return it == topics_.end() ? 0 : it->last_publish_ms;
}

qint64 DataHub::age_ms(const QString& topic) const {
    QMutexLocker lock(&mutex_);
    auto it = topics_.find(topic);
    if (it == topics_.end() || it->last_publish_ms <= 0)
        return -1;
    return now_ms() - it->last_publish_ms;
}

QString DataHub::last_error(const QString& topic) const {
    QMutexLocker lock(&mutex_);
    auto it = topics_.constFind(topic);
    return it == topics_.constEnd() ? QString() : it->last_error;
}

void DataHub::request(const QString& topic, bool force) {
    request_impl(QStringList{topic}, force, /*user_initiated=*/force);
}

void DataHub::request(const QStringList& topics, bool force) {
    // Public force=true is the "user asked for it" path (refresh buttons,
    // F5, a symbol-list edit): it bypasses and resets the error backoff.
    request_impl(topics, force, /*user_initiated=*/force);
}

void DataHub::request_impl(const QStringList& topics, bool force, bool user_initiated) {
    // Apply per-topic gates under the lock, then buffer into
    // coalesce_pending_. The coalesce timer (kCoalesceWindowMs) merges
    // bursts of request() calls — e.g., dashboard + markets subscribing
    // simultaneously — into one producer->refresh() per producer.
    //
    // Per-topic gates applied here (intent-to-fetch):
    //   - in_flight: always skip; the producer is already working.
    //   - error backoff: skip while it runs, unless `user_initiated`, which
    //     resets it (a user retry starts the ladder again from the bottom).
    //   - min_interval_ms: skipped when `force=true` (user-driven refresh).
    //
    // Per-producer rate limit (`max_requests_per_sec`) is enforced at
    // dispatch in flush_coalesced_requests(), NOT here. A single
    // coalesce flush issues exactly one refresh() call per producer
    // regardless of how many topics it batches, so the rate limit only
    // needs to gate dispatch. Gating at intake silently dropped every
    // topic past the first when a widget cold-started N subscriptions
    // back-to-back within the rate window — only the first cleared, the
    // rest fell off the floor and spun in their loading state until the
    // next 1 s scheduler tick (which had the same per-topic bug).
    int accepted = 0;
    bool need_arm = false;
    QStringList orphan_topics;
    {
        QMutexLocker lock(&mutex_);
        const qint64 t = now_ms();
        for (const auto& topic : topics) {
            auto& st = state_for(topic);
            if (st.in_flight)
                continue;
            if (user_initiated) {
                st.error_streak = 0;
                st.retry_after_ms = 0;
            } else if (hub_in_backoff(st, t)) {
                continue;
            }
            if (!force && st.policy.min_interval_ms > 0 && st.last_refresh_request_ms > 0 &&
                (t - st.last_refresh_request_ms) < st.policy.min_interval_ms) {
                continue;
            }
            Producer* p = find_producer(topic);
            if (!p) {
                orphan_topics.append(topic);
                continue;
            }
            st.in_flight = true;
            st.last_refresh_request_ms = t;
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
        LOG_ERROR("DataHub", QString("ORPHAN TOPIC(S) — %1 topic(s) have no registered producer. "
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
        QMetaObject::invokeMethod(
            this,
            [this]() {
                if (!coalesce_timer_.isActive())
                    coalesce_timer_.start();
            },
            Qt::QueuedConnection);
    }
}

void DataHub::flush_coalesced_requests() {
    QHash<Producer*, QStringList> work;
    bool defer_remaining = false;
    {
        QMutexLocker lock(&mutex_);
        if (coalesce_pending_.isEmpty())
            return;
        const qint64 t = now_ms();
        // Per-producer rate limit enforced HERE (at the dispatch boundary)
        // rather than per-topic at intake. One refresh() per producer per
        // flush, so checking once per producer is the correct granularity.
        // Producers caught inside the rate window keep their topics in
        // coalesce_pending_ and the timer is re-armed below.
        for (auto it = coalesce_pending_.begin(); it != coalesce_pending_.end();) {
            Producer* p = it.key();
            const int rps = p->max_requests_per_sec();
            if (rps > 0) {
                const qint64 last = producer_last_refresh_ms_.value(p, 0);
                const qint64 min_gap = 1000 / std::max(1, rps);
                if (last > 0 && (t - last) < min_gap) {
                    defer_remaining = true;
                    ++it;
                    continue;
                }
            }
            work.insert(p, std::move(it.value()));
            producer_last_refresh_ms_[p] = t;
            it = coalesce_pending_.erase(it);
        }
    }

    if (defer_remaining) {
        // Re-arm the timer on the hub thread so the deferred batch flushes
        // after another coalesce window. Producer rate limits this short
        // (10 rps → 100 ms) almost always clear inside one window.
        QMetaObject::invokeMethod(
            this,
            [this]() {
                if (!coalesce_timer_.isActive())
                    coalesce_timer_.start();
            },
            Qt::QueuedConnection);
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
        LOG_DEBUG("DataHub", QString("coalesce flushed %1 topics across %2 producers").arg(total).arg(work.size()));
    }
}

// ── Scheduler ──────────────────────────────────────────────────────────────

void DataHub::scheduler_body() {
    // One pass: collect refresh work per producer, respecting TTL, min
    // interval, in-flight, and per-producer rate limits.
    QHash<Producer*, QStringList> work;
    QStringList timed_out_topics;
    QVector<int> timed_out_streaks; // parallel to timed_out_topics
    {
        QMutexLocker lock(&mutex_);
        const qint64 t = now_ms();

        // Pre-pass: clear `in_flight` on topics whose producer missed the
        // refresh_timeout_ms window. Without this a crashed / hung producer
        // pins the topic forever and subscribers never see fresh data.
        for (auto it = topics_.begin(); it != topics_.end(); ++it) {
            if (!it->in_flight)
                continue;
            if (it->policy.refresh_timeout_ms <= 0)
                continue;
            if (it->last_refresh_request_ms <= 0)
                continue;
            if ((t - it->last_refresh_request_ms) >= it->policy.refresh_timeout_ms) {
                it->in_flight = false;
                // A producer that did not answer in time is not asked again
                // on the very next tick either — re-asking a stuck upstream
                // only queues more work behind the stuck request. A late
                // publish() still lands and resets this.
                it->error_streak += 1;
                it->retry_after_ms = t + hub_backoff_delay_ms(it->error_streak, 0);
                timed_out_topics.append(it.key());
                timed_out_streaks.append(it->error_streak);
            }
        }

        // Drop empty subscription buckets left behind by unsubscribe churn.
        // The scheduler would otherwise skip them forever while still paying
        // the hash iteration cost on every tick.
        for (auto it = subscriptions_.begin(); it != subscriptions_.end();) {
            if (it.value().isEmpty())
                it = subscriptions_.erase(it);
            else
                ++it;
        }

        // Release cached payloads for topics nobody is watching any more.
        // Runs on its own cadence — see kIdleSweepEveryTicks. Placed here so
        // it can never race the in_flight marking done in phase 2 below.
        if (++sweep_tick_counter_ >= kIdleSweepEveryTicks) {
            sweep_tick_counter_ = 0;
            sweep_idle_topics(t);
        }

        // Phase 1 — collect eligible topics by producer (no rate-limit
        // gate yet). The per-producer rate limit applies to refresh()
        // *calls*, not to topic enrolment; one refresh() carries the
        // whole batch.
        QHash<Producer*, QStringList> candidates;
        for (auto it = subscriptions_.begin(); it != subscriptions_.end(); ++it) {
            const QString& topic = it.key();
            if (it.value().isEmpty())
                continue;
            // A destroyed owner leaves a null QPointer until its cleanup runs; a
            // topic watched only by such ghosts must not be polled.
            const auto& subs = it.value();
            if (std::none_of(subs.cbegin(), subs.cend(), [](const Subscription& s) { return !s.owner.isNull(); }))
                continue;

            auto& st = state_for(topic);
            if (st.policy.push_only)
                continue;
            if (st.in_flight)
                continue;
            // Last refresh failed: wait out the error backoff (publish_error).
            if (hub_in_backoff(st, t))
                continue;

            const bool have_value = st.value.isValid() && st.last_publish_ms > 0;
            const bool ttl_expired = !have_value || (t - st.last_publish_ms) >= effective_ttl_ms(st);
            const bool min_interval_ok = (t - st.last_refresh_request_ms) >= st.policy.min_interval_ms;
            if (!ttl_expired || !min_interval_ok)
                continue;
            // pause_when_inactive used to stop only the fan-out: a quote watched
            // solely by hidden widgets / minimised windows was still fetched every
            // TTL. Now it is not fetched until a subscriber is visible again; the
            // first tick after that finds it expired and fetches it.
            if (st.policy.pause_when_inactive && !has_visible_subscriber(topic)) {
                note_hidden_skip(st, t);
                continue;
            }

            Producer* p = find_producer(topic);
            if (!p)
                continue;
            candidates[p].append(topic);
        }

        // Phase 2 — apply per-producer rate limit ONCE, then promote
        // the producer's whole batch into `work`. If a producer is
        // inside its rate window, skip it this tick; its topics stay
        // subscribed and will be picked up on the next tick (1 s away,
        // well past any reasonable min_gap).
        for (auto it = candidates.begin(); it != candidates.end(); ++it) {
            Producer* p = it.key();
            const int rps = p->max_requests_per_sec();
            if (rps > 0) {
                const qint64 last = producer_last_refresh_ms_.value(p, 0);
                const qint64 min_gap = 1000 / std::max(1, rps);
                if (last > 0 && (t - last) < min_gap)
                    continue;
            }
            QStringList& batch = work[p];
            batch.reserve(batch.size() + it.value().size());
            for (const QString& topic : it.value()) {
                auto& st = state_for(topic);
                st.in_flight = true;
                st.last_refresh_request_ms = t;
                batch.append(topic);
            }
            producer_last_refresh_ms_[p] = t;
        }
    }

    for (int i = 0; i < timed_out_topics.size(); ++i) {
        const QString& topic = timed_out_topics.at(i);
        // WARN on the first failure of a streak only; repeats are DEBUG.
        if (timed_out_streaks.value(i) <= 1) {
            LOG_WARN("DataHub", QString("Producer refresh timed out for topic '%1' — clearing in_flight. "
                                        "The producer failed to publish() or publish_error() within "
                                        "refresh_timeout_ms; hub will retry after the error backoff.")
                                    .arg(topic));
        } else {
            LOG_DEBUG("DataHub", QString("Producer refresh timed out again for topic '%1' (attempt %2)")
                                     .arg(topic)
                                     .arg(timed_out_streaks.value(i)));
        }
        emit topic_error(topic, QStringLiteral("refresh_timeout"));
    }

    for (auto it = work.begin(); it != work.end(); ++it) {
        LOG_DEBUG("DataHub", QString("refresh(): producer=%1 topics=%2")
                                 .arg(reinterpret_cast<quintptr>(it.key()), 0, 16)
                                 .arg(it.value().join(QLatin1Char(','))));
        it.key()->refresh(it.value());
    }
}

bool DataHub::has_live_subscribers(const QString& topic) const {
    // Caller holds mutex_. A destroyed owner leaves a null QPointer behind
    // until on_owner_destroyed() runs, so presence in the vector is not
    // enough — the owner has to still be alive.
    if (auto it = subscriptions_.constFind(topic); it != subscriptions_.constEnd()) {
        for (const auto& sub : it.value())
            if (sub.owner)
                return true;
    }
    // Wildcard subscribers (e.g. "option:chain:*", "ws:kraken:*") never appear
    // under the concrete key, so they must be matched explicitly. Missing this
    // would evict state out from under a live pattern consumer.
    for (auto it = pattern_subscriptions_.constBegin(); it != pattern_subscriptions_.constEnd(); ++it) {
        if (!pattern_matches(it.key(), topic))
            continue;
        for (const auto& sub : it.value())
            if (sub.owner)
                return true;
    }
    // Error-only consumers keep the topic alive too — they're cheap to check
    // and a topic being watched for failures is not an idle topic.
    if (auto it = error_subscriptions_.constFind(topic); it != error_subscriptions_.constEnd()) {
        for (const auto& sub : it.value())
            if (sub.owner)
                return true;
    }
    for (auto it = error_pattern_subscriptions_.constBegin(); it != error_pattern_subscriptions_.constEnd(); ++it) {
        if (!pattern_matches(it.key(), topic))
            continue;
        for (const auto& sub : it.value())
            if (sub.owner)
                return true;
    }
    return false;
}

bool DataHub::has_visible_subscriber(const QString& topic) const {
    // Caller holds mutex_. hub_is_owner_visible_for_work() only reads Qt state
    // and never calls back into the hub. Error-only subscribers do not count:
    // they show no data.
    if (auto it = subscriptions_.constFind(topic); it != subscriptions_.constEnd()) {
        for (const auto& sub : it.value())
            if (sub.owner && hub_is_owner_visible_for_work(sub.owner.data()))
                return true;
    }
    for (auto it = pattern_subscriptions_.constBegin(); it != pattern_subscriptions_.constEnd(); ++it) {
        if (!pattern_matches(it.key(), topic))
            continue;
        for (const auto& sub : it.value())
            if (sub.owner && hub_is_owner_visible_for_work(sub.owner.data()))
                return true;
    }
    return false;
}

void DataHub::note_hidden_skip(TopicState& st, qint64 now) {
    // Caller holds mutex_. The scheduler looks at a due topic every tick, but
    // only one fetch per freshness window would have been made.
    const qint64 window = std::max<qint64>(effective_ttl_ms(st), st.policy.min_interval_ms);
    if (st.hidden_skip_ms > 0 && (now - st.hidden_skip_ms) < window)
        return;
    st.hidden_skip_ms = now;
    hidden_skip_count_.fetch_add(1, std::memory_order_relaxed);
}

void DataHub::sweep_idle_topics(qint64 now) {
    // Caller holds mutex_. Conservative by construction — every one of these
    // gates must pass before a TopicState is dropped.
    int evicted = 0;
    for (auto it = topics_.begin(); it != topics_.end();) {
        const auto& st = it.value();

        // A producer has an outstanding refresh() for this topic; its
        // publish() would resurrect the state anyway and the in_flight /
        // last_refresh_request_ms bookkeeping still matters for the timeout
        // pre-pass.
        if (st.in_flight) {
            ++it;
            continue;
        }
        // A deferred fan-out is armed — do_coalesced_flush() still needs
        // coalesce_latest.
        if (st.coalesce_pending) {
            ++it;
            continue;
        }
        // Never evict a topic somebody is still watching.
        if (has_live_subscribers(it.key())) {
            ++it;
            continue;
        }
        // Still serving an error backoff / negative cache: evicting it would
        // let the next subscriber's cold start refetch a known-dead topic.
        if (hub_in_backoff(st, now)) {
            ++it;
            continue;
        }

        // Age is measured from the last time the topic was touched at all, so
        // a state created by a peek() or a subscribe that never received data
        // ages out too. A state that has never been touched (both zero) is
        // left alone rather than guessed at.
        const qint64 last_touch = std::max(st.last_publish_ms, st.last_refresh_request_ms);
        if (last_touch <= 0) {
            ++it;
            continue;
        }

        // push_only families ignore ttl_ms for scheduling, but the value is
        // still a sane "how often does this normally move" figure, which is
        // exactly what we want to scale the idle window by. A per-publish TTL
        // only ever shortens it: market data published with a session-length
        // TTL (a closed market's history, fresh until the venue reopens) must
        // not keep an unwatched payload for ten sessions.
        const qint64 ttl = std::min(effective_ttl_ms(st), st.policy.ttl_ms);
        const qint64 window = std::max(ttl * kIdleEvictTtlMultiple, kIdleEvictMinAgeMs);
        if ((now - last_touch) < window) {
            ++it;
            continue;
        }

        // set_policy() stores the explicit per-topic policy *inside* the
        // TopicState, so a full erase would silently downgrade the topic to
        // its pattern policy — or to the 30s/5s defaults — the next time it
        // was touched. Those states release their payload instead and keep
        // their configuration and counters. The QVariant is the memory; the
        // rest of the struct is a few dozen bytes.
        if (it->policy_explicit) {
            if (it->value.isValid()) {
                it->value = QVariant();
                it->coalesce_latest = QVariant();
                ++evicted;
            }
            ++it;
            continue;
        }

        it = topics_.erase(it);
        ++evicted;
    }
    if (evicted > 0)
        LOG_DEBUG("DataHub", QString("Evicted %1 idle topic state(s); %2 retained").arg(evicted).arg(topics_.size()));
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
        s.error_streak = it->error_streak;
        s.retry_after_ms = it->retry_after_ms;
        if (auto sub_it = subscriptions_.find(it.key()); sub_it != subscriptions_.end()) {
            for (const auto& sub : sub_it.value())
                if (sub.owner)
                    ++s.subscriber_count; // ghosts of destroyed owners are not subscribers
        }
        out.append(std::move(s));
    }
    std::sort(out.begin(), out.end(), [](const TopicStats& a, const TopicStats& b) { return a.topic < b.topic; });
    return out;
}

QList<QObject*> DataHub::subscribers(const QString& topic) const {
    QMutexLocker lock(&mutex_);
    QList<QObject*> out;
    auto it = subscriptions_.find(topic);
    if (it == subscriptions_.end())
        return out;
    out.reserve(it.value().size());
    for (const auto& s : it.value()) {
        if (s.owner)
            out.append(s.owner.data());
    }
    return out;
}

} // namespace fincept::datahub
