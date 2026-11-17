#include "screens/markets/MarketsQuoteStore.h"

#include "core/instrument/VenueSymbols.h"
#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/markets/MarketDataService.h"

#include <QDateTime>
#include <QMap>
#include <QPointer>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens {

namespace {
constexpr qint64 kFlashMs = 1200;         // tick flash fade (the Watchlist constant)
constexpr int kFlashTickMs = 60;
constexpr int kStageWaitMs = 5000;         // a stage starts when the previous resolved, or after this
constexpr qint64 kOffMinAgeMs = 5 * 60'000; // OFF: one cycle on show when the data is older than this
constexpr int kAgeTickMs = 30'000;         // re-evaluate staleness / "as of" while visible
constexpr int kResortMinMs = 5000;         // LIVE: sorted panels re-sort at most this often
constexpr qint64 kQuoteTtlMs = 30'000;     // the market:quote:* policy TTL

qint64 mqs_now() {
    return QDateTime::currentMSecsSinceEpoch();
}

QString mqs_quote_topic(const QString& s) {
    return services::MarketDataService::quote_topic(s);
}

QString mqs_spark_topic(const QString& s) {
    return services::MarketDataService::sparkline_topic(s);
}
} // namespace

MarketsQuoteStore::MarketsQuoteStore(QObject* parent) : QObject(parent) {
    live_owner_ = new QObject(this);
    panel_owner_ = new QObject(this);
    spark_owner_ = new QObject(this);

    stage_timer_ = new QTimer(this);
    stage_timer_->setSingleShot(true);
    connect(stage_timer_, &QTimer::timeout, this, &MarketsQuoteStore::next_stage);

    spark_stage_timer_ = new QTimer(this);
    spark_stage_timer_->setSingleShot(true);
    connect(spark_stage_timer_, &QTimer::timeout, this, &MarketsQuoteStore::next_spark_stage);

    watchdog_timer_ = new QTimer(this);
    watchdog_timer_->setInterval(1000);
    connect(watchdog_timer_, &QTimer::timeout, this, &MarketsQuoteStore::check_watchdog);

    interval_timer_ = new QTimer(this);
    interval_timer_->setSingleShot(true);
    connect(interval_timer_, &QTimer::timeout, this, [this]() { start_cycle(false); });

    flash_timer_ = new QTimer(this);
    flash_timer_->setInterval(kFlashTickMs);
    connect(flash_timer_, &QTimer::timeout, this, &MarketsQuoteStore::on_flash_timer);

    age_timer_ = new QTimer(this);
    age_timer_->setInterval(kAgeTickMs);
    connect(age_timer_, &QTimer::timeout, this, [this]() {
        // Rows turn stale with time alone; repaint them and the status bar.
        for (auto it = rows_.cbegin(); it != rows_.cend(); ++it)
            if (it->has_quote)
                pending_changed_.insert(it.key());
        flush();
    });

    resort_timer_ = new QTimer(this);
    resort_timer_->setSingleShot(true);
    connect(resort_timer_, &QTimer::timeout, this, &MarketsQuoteStore::resort_due);
}

MarketsQuoteStore::~MarketsQuoteStore() {
    // Explicit: the hub's destroyed()-driven cleanup is queued, and its slots capture `this`.
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(live_owner_);
    hub.unsubscribe(panel_owner_);
    hub.unsubscribe(spark_owner_);
}

// ── Rows ────────────────────────────────────────────────────────────────────

MarketRow& MarketsQuoteStore::ensure_row(const QString& symbol) {
    auto it = rows_.find(symbol);
    if (it != rows_.end())
        return it.value();
    MarketRow r;
    r.symbol = symbol;
    r.cls = core::instrument::asset_class_of(symbol);
    warm_start(r);
    return rows_.insert(symbol, r).value();
}

void MarketsQuoteStore::warm_start(MarketRow& r) {
    // The hub's last value, whatever its age (another screen may have fetched it): the
    // row paints at once and its age says how fresh it is.
    auto& hub = datahub::DataHub::instance();
    const QString topic = mqs_quote_topic(r.symbol);
    const qint64 pub = hub.last_publish_ms(topic);
    if (pub > r.quote_ms) {
        const QVariant v = hub.peek_raw(topic);
        if (v.canConvert<services::QuoteData>()) {
            const auto q = v.value<services::QuoteData>();
            if (q.price > 0.0) {
                r.quote = q;
                r.has_quote = true;
                r.quote_ms = pub;
                if (r.load != MarketRow::Load::Loading)
                    r.load = MarketRow::Load::Ok;
                if (!q.name.trimmed().isEmpty() && q.name != r.symbol)
                    r.name = q.name.trimmed();
                r.cls = core::instrument::asset_class_of(r.symbol, q.quote_type);
            }
        }
    }
    if (r.spark.isEmpty()) {
        const QVariant sv = hub.peek_raw(mqs_spark_topic(r.symbol));
        if (sv.canConvert<QVector<double>>()) {
            r.spark = sv.value<QVector<double>>();
            if (!r.spark.isEmpty())
                r.spark_state = MarketRow::Spark::Ready;
        }
    }
}

QSet<QString> MarketsQuoteStore::live_set() const {
    QSet<QString> s(live_symbols_.cbegin(), live_symbols_.cend());
    if (!detail_symbol_.isEmpty())
        s.insert(detail_symbol_);
    return s;
}

void MarketsQuoteStore::prune_rows() {
    const QSet<QString> live = live_set();
    for (auto it = rows_.begin(); it != rows_.end();) {
        if (panel_set_.contains(it.key()) || live.contains(it.key())) {
            ++it;
            continue;
        }
        flashing_.remove(it.key());
        spark_wanted_.remove(it.key());
        it = rows_.erase(it);
    }
}

const MarketRow* MarketsQuoteStore::row(const QString& symbol) const {
    const auto it = rows_.constFind(symbol);
    return it == rows_.constEnd() ? nullptr : &it.value();
}

markets_logic::RowState MarketsQuoteStore::row_state(const MarketRow& r) const {
    return markets_logic::row_state(r.has_quote, !r.error.isEmpty(),
                                    markets_logic::session_from_state(r.quote.market_state), mqs_now(), r.quote_ms,
                                    interval_ms_);
}

bool MarketsQuoteStore::is_stale(const MarketRow& r) const {
    const auto s = row_state(r);
    return s == markets_logic::RowState::Stale || s == markets_logic::RowState::FailedOld;
}

double MarketsQuoteStore::flash_strength(const MarketRow& r) const {
    if (r.flash_ms <= 0 || r.flash_dir == 0)
        return 0.0;
    const qint64 age = mqs_now() - r.flash_ms;
    if (age < 0 || age >= kFlashMs)
        return 0.0;
    return (1.0 - static_cast<double>(age) / kFlashMs) * r.flash_dir;
}

QString MarketsQuoteStore::display_name(const QString& symbol) const {
    if (const MarketRow* r = row(symbol); r && !r->name.isEmpty())
        return r->name;
    const QString d = core::instrument::display_symbol(symbol);
    return d.isEmpty() ? symbol : d;
}

// ── Sets ────────────────────────────────────────────────────────────────────

void MarketsQuoteStore::set_panel_symbols(const QVector<QStringList>& per_panel_in_reading_order) {
    QStringList all;
    QSet<QString> next;
    for (const QStringList& panel : per_panel_in_reading_order)
        for (const QString& s : panel)
            if (!s.isEmpty() && !next.contains(s)) {
                next.insert(s);
                all.append(s);
            }
    QStringList added;
    for (const QString& s : all)
        if (!panel_set_.contains(s))
            added.append(s);
    const QSet<QString> removed = panel_set_ - next;
    panel_symbols_ = all;
    panel_set_ = next;

    for (const QString& s : all)
        ensure_row(s);
    for (const QString& s : removed) {
        if (panel_subscribed_.remove(s))
            unsubscribe_quote(panel_owner_, s);
        cycle_pending_.remove(s);
        stage_pending_.remove(s);
        cycle_queue_.removeAll(s);
    }
    prune_rows();
    for (const QString& s : added)
        mark_changed(s);
    mark_changed(QString()); // coverage changes even without a row to repaint

    // New symbols load at once: they join a running cycle, or run a cycle of their own
    // (a full one when no cycle has completed yet).
    if (active_ && !added.isEmpty())
        start_cycle(false, (cycle_running_ || first_cycle_done_) ? added : QStringList{});
}

void MarketsQuoteStore::set_live_symbols(const QStringList& ribbon) {
    if (live_symbols_ == ribbon)
        return;
    live_symbols_ = ribbon;
    for (const QString& s : ribbon)
        ensure_row(s);
    sync_live_subscriptions();
    prune_rows();
}

void MarketsQuoteStore::set_detail_symbol(const QString& symbol) {
    if (detail_symbol_ == symbol)
        return;
    detail_symbol_ = symbol;
    if (!symbol.isEmpty())
        ensure_row(symbol);
    sync_live_subscriptions();
    prune_rows();
}

void MarketsQuoteStore::sync_live_subscriptions() {
    if (!active_)
        return;
    const QSet<QString> wanted = live_set();
    const QSet<QString> drop = live_subscribed_ - wanted;
    for (const QString& s : drop) {
        unsubscribe_quote(live_owner_, s);
        auto& hub = datahub::DataHub::instance();
        hub.unsubscribe(live_owner_, mqs_spark_topic(s));
        hub.unsubscribe_errors(live_owner_, mqs_spark_topic(s));
        live_subscribed_.remove(s);
    }
    const qint64 now = mqs_now();
    auto& hub = datahub::DataHub::instance();
    for (const QString& s : wanted) {
        if (live_subscribed_.contains(s))
            continue;
        MarketRow& r = ensure_row(s);
        if (!r.has_quote || now - r.quote_ms >= kQuoteTtlMs) {
            r.load = MarketRow::Load::Loading;
            r.requested_ms = now;
            mark_changed(s);
        }
        subscribe_quote(live_owner_, s);
        const QString st = mqs_spark_topic(s);
        hub.subscribe(live_owner_, st, [this, s](const QVariant& v) { on_spark(s, v); });
        hub.subscribe_errors(live_owner_, st, [this, s](const QString& e) { on_spark_error(s, e); });
        live_subscribed_.insert(s);
    }
    if (!watchdog_timer_->isActive())
        watchdog_timer_->start();
}

void MarketsQuoteStore::subscribe_quote(QObject* owner, const QString& symbol) {
    auto& hub = datahub::DataHub::instance();
    const QString topic = mqs_quote_topic(symbol);
    hub.subscribe(owner, topic, [this, symbol](const QVariant& v) { on_quote(symbol, v); });
    hub.subscribe_errors(owner, topic, [this, symbol](const QString& e) { on_quote_error(symbol, e); });
}

void MarketsQuoteStore::unsubscribe_quote(QObject* owner, const QString& symbol) {
    auto& hub = datahub::DataHub::instance();
    const QString topic = mqs_quote_topic(symbol);
    hub.unsubscribe(owner, topic);
    hub.unsubscribe_errors(owner, topic);
}

// ── Deliveries ──────────────────────────────────────────────────────────────

void MarketsQuoteStore::on_quote(const QString& symbol, const QVariant& value) {
    auto it = rows_.find(symbol);
    if (it == rows_.end() || !value.canConvert<services::QuoteData>())
        return;
    const auto q = value.value<services::QuoteData>();
    if (!(q.price > 0.0)) {
        on_quote_error(symbol, tr("The data source returned no price"));
        return;
    }
    MarketRow& r = it.value();
    const qint64 now = mqs_now();
    qint64 pub = datahub::DataHub::instance().last_publish_ms(mqs_quote_topic(symbol));
    if (pub <= 0)
        pub = now;
    // The same publish reaches both the live and the panel owner: apply it once.
    const bool duplicate = r.has_quote && r.quote_ms == pub && r.error.isEmpty() && r.quote.price == q.price;
    if (!duplicate) {
        // Flash on a price move — not on the first fill, not for a closed / extended session.
        const auto session = markets_logic::session_from_state(q.market_state);
        if (r.has_quote && std::isfinite(r.quote.price) && q.price != r.quote.price &&
            session != markets_logic::Session::Closed && session != markets_logic::Session::Extended) {
            r.flash_ms = now;
            r.flash_dir = q.price > r.quote.price ? 1 : -1;
            flashing_.insert(symbol);
            if (!flash_timer_->isActive())
                flash_timer_->start();
        }
        r.quote = q;
        r.has_quote = true;
        r.quote_ms = pub;
        if (!q.name.trimmed().isEmpty() && q.name != symbol)
            r.name = q.name.trimmed();
        r.cls = core::instrument::asset_class_of(symbol, q.quote_type);
    }
    r.error.clear();
    r.timed_out = false;
    r.load = MarketRow::Load::Ok;
    r.requested_ms = 0;
    cycle_pending_.remove(symbol);
    stage_pending_.remove(symbol);
    mark_changed(symbol);
}

void MarketsQuoteStore::on_quote_error(const QString& symbol, const QString& error) {
    auto it = rows_.find(symbol);
    if (it == rows_.end())
        return;
    MarketRow& r = it.value();
    const QString reason = error.trimmed();
    r.error = reason.isEmpty() ? tr("Failed to load market data") : reason;
    r.error_ms = mqs_now();
    r.load = MarketRow::Load::Failed;
    r.requested_ms = 0;
    r.timed_out = false;
    cycle_pending_.remove(symbol);
    stage_pending_.remove(symbol);
    mark_changed(symbol);
}

void MarketsQuoteStore::on_spark(const QString& symbol, const QVariant& value) {
    auto it = rows_.find(symbol);
    spark_stage_pending_.remove(symbol);
    if (it == rows_.end())
        return;
    const QVector<double> pts = value.value<QVector<double>>();
    if (pts.isEmpty()) {
        on_spark_error(symbol, QString());
        return;
    }
    it->spark = pts;
    it->spark_state = MarketRow::Spark::Ready;
    mark_changed(symbol);
}

void MarketsQuoteStore::on_spark_error(const QString& symbol, const QString& /*error*/) {
    spark_stage_pending_.remove(symbol);
    auto it = rows_.find(symbol);
    if (it == rows_.end())
        return;
    if (it->spark.isEmpty())
        it->spark_state = MarketRow::Spark::Failed;
    mark_changed(symbol);
}

// ── Cycles ──────────────────────────────────────────────────────────────────

void MarketsQuoteStore::start_cycle(bool forced, const QStringList& only) {
    if (!active_)
        return;
    const QSet<QString> live = live_set();
    auto& hub = datahub::DataHub::instance();
    if (cycle_running_) {
        if (!only.isEmpty()) { // symbols added while a cycle runs join it
            for (const QString& s : only)
                if (!live.contains(s) && !cycle_queue_.contains(s) && !cycle_pending_.contains(s))
                    cycle_queue_.append(s);
            if (stage_pending_.isEmpty())
                next_stage();
            return;
        }
        if (forced && !cycle_forced_) { // F5 during an automatic cycle: force what is outstanding
            cycle_forced_ = true;
            QStringList topics;
            for (const QString& s : std::as_const(cycle_pending_))
                topics << mqs_quote_topic(s);
            if (!topics.isEmpty())
                hub.request(topics, true);
        }
        return; // F5 during a forced cycle is ignored
    }
    interval_timer_->stop();
    cycle_running_ = true;
    cycle_forced_ = forced;
    cycle_started_ms_ = mqs_now();
    cycle_pending_.clear();
    stage_pending_.clear();
    cycle_queue_.clear();
    for (const QString& s : (only.isEmpty() ? panel_symbols_ : only))
        if (!live.contains(s))
            cycle_queue_.append(s);

    if (forced) { // stage 0: the live set, forced (resets its backoff too)
        QStringList topics;
        const qint64 now = mqs_now();
        for (const QString& s : live) {
            topics << mqs_quote_topic(s);
            MarketRow& r = ensure_row(s);
            r.load = MarketRow::Load::Loading;
            r.requested_ms = now;
            mark_changed(s);
        }
        if (!topics.isEmpty())
            hub.request(topics, true);
    }
    emit cycle_started();
    next_stage();
    mark_changed(QString());
}

void MarketsQuoteStore::next_stage() {
    stage_timer_->stop();
    if (!cycle_running_)
        return;
    const qint64 now = mqs_now();
    QStringList topics;
    while (!cycle_queue_.isEmpty() && topics.size() < markets_logic::kTopicsPerRequest) {
        const QString s = cycle_queue_.takeFirst();
        if (!panel_set_.contains(s))
            continue;
        MarketRow& r = ensure_row(s);
        const bool subscribed = panel_subscribed_.contains(s);
        // Already subscribed with a value the hub still holds as fresh: nothing to wait for.
        if (subscribed && !cycle_forced_ && r.has_quote && r.error.isEmpty() && now - r.quote_ms < kQuoteTtlMs)
            continue;
        r.load = MarketRow::Load::Loading;
        r.requested_ms = now;
        cycle_pending_.insert(s);
        stage_pending_.insert(s);
        if (!subscribed) {
            // A new subscription fetches by itself when the hub has no fresh value, and
            // delivers the fresh one (queued) when it has.
            subscribe_quote(panel_owner_, s);
            panel_subscribed_.insert(s);
        }
        topics << mqs_quote_topic(s);
        mark_changed(s);
    }
    if (topics.isEmpty() && cycle_queue_.isEmpty()) {
        maybe_finish_cycle();
        return;
    }
    if (cycle_forced_ && !topics.isEmpty())
        datahub::DataHub::instance().request(topics, true);
    if (!watchdog_timer_->isActive())
        watchdog_timer_->start();
    stage_timer_->start(kStageWaitMs);
}

void MarketsQuoteStore::maybe_finish_cycle() {
    if (cycle_running_ && cycle_queue_.isEmpty() && cycle_pending_.isEmpty())
        finish_cycle();
}

void MarketsQuoteStore::finish_cycle() {
    cycle_running_ = false;
    const bool was_forced = cycle_forced_;
    cycle_forced_ = false;
    stage_timer_->stop();
    last_cycle_end_ms_ = mqs_now();
    first_cycle_done_ = true;

    const Coverage c = coverage();
    last_cycle_all_failed_ = c.total > 0 && c.failed >= c.total;
    // One DEBUG line per completed cycle; one WARN per reason when symbols newly fail.
    LOG_DEBUG("Markets", QString("coverage: %1/%2 priced, %3 failed, %4 timed out, cycle %5 s%6")
                            .arg(c.priced)
                            .arg(c.total)
                            .arg(c.failed)
                            .arg(c.timed_out)
                            .arg((last_cycle_end_ms_ - cycle_started_ms_) / 1000.0, 0, 'f', 1)
                            .arg(was_forced ? QStringLiteral(" (forced)") : QString()));
    QSet<QString> failed_now;
    QMap<QString, QStringList> newly_failed_by_reason;
    for (const QString& s : std::as_const(panel_symbols_)) {
        const MarketRow* r = row(s);
        if (!r || r->error.isEmpty())
            continue;
        failed_now.insert(s);
        if (!logged_failures_.contains(s))
            newly_failed_by_reason[r->error].append(s);
    }
    logged_failures_ = failed_now;
    for (auto it = newly_failed_by_reason.cbegin(); it != newly_failed_by_reason.cend(); ++it)
        LOG_WARN("Markets", logf("Quotes unavailable", {{"count", it.value().size()},
                                                        {"symbols", it.value().mid(0, 8).join(QLatin1Char(','))},
                                                        {"total", c.total},
                                                        {"error", it.key()}}));

    // Interval mode: the panel set is dropped between cycles (rows keep their data and
    // their "as of"); LIVE keeps it subscribed and the hub refreshes it at the TTL.
    if (interval_ms_ > 0 || !auto_) {
        for (const QString& s : std::as_const(panel_subscribed_))
            unsubscribe_quote(panel_owner_, s);
        panel_subscribed_.clear();
    }
    request_names();
    emit cycle_finished();
    emit resort_due();
    schedule_next_cycle();
    if (!spark_queue_.isEmpty() && spark_stage_pending_.isEmpty())
        next_spark_stage();
    mark_changed(QString());
}

void MarketsQuoteStore::schedule_next_cycle() {
    interval_timer_->stop();
    if (!active_ || !auto_ || interval_ms_ <= 0)
        return;
    interval_timer_->start(std::max(60'000, interval_ms_));
}

void MarketsQuoteStore::check_watchdog() {
    const qint64 now = mqs_now();
    bool any_loading = false;
    for (auto it = rows_.begin(); it != rows_.end(); ++it) {
        MarketRow& r = it.value();
        if (r.load != MarketRow::Load::Loading || r.requested_ms <= 0)
            continue;
        if (now - r.requested_ms <= markets_logic::kWatchdogMs) {
            any_loading = true;
            continue;
        }
        // The hub's own refresh timeout is 30 s: by now this request has been lost.
        r.error = tr("No response from the data source yet");
        r.error_ms = now;
        r.timed_out = true;
        r.load = MarketRow::Load::Failed;
        r.requested_ms = 0;
        cycle_pending_.remove(it.key());
        stage_pending_.remove(it.key());
        datahub::DataHub::instance().request(mqs_quote_topic(it.key())); // non-forced: honours the backoff
        mark_changed(it.key());
    }
    if (!any_loading && !cycle_running_)
        watchdog_timer_->stop();
}

void MarketsQuoteStore::set_interval_ms(int ms) {
    ms = ms <= 0 ? 0 : std::clamp(ms, 60'000, 3'600'000);
    if (ms == interval_ms_)
        return;
    const bool was_live = interval_ms_ == 0;
    interval_ms_ = ms;
    if (!active_)
        return;
    if (ms == 0) {
        if (auto_ && !cycle_running_)
            start_cycle(false); // subscribes the panel set and keeps it
    } else if (!cycle_running_) {
        if (was_live) {
            for (const QString& s : std::as_const(panel_subscribed_))
                unsubscribe_quote(panel_owner_, s);
            panel_subscribed_.clear();
        }
        schedule_next_cycle();
    }
    mark_changed(QString());
}

void MarketsQuoteStore::set_auto(bool on) {
    if (auto_ == on)
        return;
    auto_ = on;
    if (!active_)
        return;
    if (!on) {
        interval_timer_->stop();
        if (!cycle_running_) {
            for (const QString& s : std::as_const(panel_subscribed_))
                unsubscribe_quote(panel_owner_, s);
            panel_subscribed_.clear();
        }
    } else if (!cycle_running_) {
        const qint64 age = last_cycle_end_ms_ > 0 ? mqs_now() - last_cycle_end_ms_ : std::numeric_limits<qint64>::max();
        if (interval_ms_ == 0 || age >= interval_ms_)
            start_cycle(false);
        else
            interval_timer_->start(static_cast<int>(interval_ms_ - age));
    }
    mark_changed(QString());
}

void MarketsQuoteStore::set_active(bool active) {
    if (active_ == active)
        return;
    active_ = active;
    auto& hub = datahub::DataHub::instance();
    if (!active) {
        hub.unsubscribe(live_owner_);
        hub.unsubscribe(panel_owner_);
        hub.unsubscribe(spark_owner_);
        live_subscribed_.clear();
        panel_subscribed_.clear();
        spark_subscribed_.clear();
        for (QTimer* t : {stage_timer_, spark_stage_timer_, watchdog_timer_, interval_timer_, flash_timer_, age_timer_,
                          resort_timer_})
            t->stop();
        cycle_running_ = false;
        cycle_forced_ = false;
        cycle_queue_.clear();
        cycle_pending_.clear();
        stage_pending_.clear();
        spark_queue_.clear();
        spark_stage_pending_.clear();
        for (auto it = rows_.begin(); it != rows_.end(); ++it) {
            MarketRow& r = it.value();
            if (r.load == MarketRow::Load::Loading)
                r.load = r.has_quote ? MarketRow::Load::Ok
                                     : (r.error.isEmpty() ? MarketRow::Load::Idle : MarketRow::Load::Failed);
            r.requested_ms = 0;
            r.flash_ms = 0;
            if (r.spark_state == MarketRow::Spark::Loading)
                r.spark_state = r.spark.isEmpty() ? MarketRow::Spark::None : MarketRow::Spark::Ready;
        }
        flashing_.clear();
        return;
    }

    // Warm start from whatever the hub has (another screen may have refreshed meanwhile).
    for (auto it = rows_.begin(); it != rows_.end(); ++it) {
        warm_start(it.value());
        pending_changed_.insert(it.key());
    }
    sync_live_subscriptions();
    const qint64 age = last_cycle_end_ms_ > 0 ? mqs_now() - last_cycle_end_ms_ : std::numeric_limits<qint64>::max();
    if (auto_) {
        if (interval_ms_ == 0 || !first_cycle_done_ || age >= interval_ms_)
            start_cycle(false);
        else
            interval_timer_->start(static_cast<int>(interval_ms_ - age));
    } else if (!first_cycle_done_ || age >= kOffMinAgeMs) {
        start_cycle(false);
    }
    age_timer_->start();
    if (!spark_wanted_.isEmpty() && first_cycle_done_) {
        spark_queue_ = QStringList(spark_wanted_.cbegin(), spark_wanted_.cend());
        next_spark_stage();
    }
    flush();
}

void MarketsQuoteStore::refresh_now() {
    if (active_)
        start_cycle(true);
}

// ── Sparklines ──────────────────────────────────────────────────────────────

void MarketsQuoteStore::want_sparklines(const QStringList& symbols) {
    QSet<QString> wanted;
    for (const QString& s : symbols)
        if (rows_.contains(s))
            wanted.insert(s);
    spark_wanted_ = wanted;
    if (!active_)
        return;
    auto& hub = datahub::DataHub::instance();
    const QSet<QString> drop = spark_subscribed_ - wanted;
    for (const QString& s : drop) {
        hub.unsubscribe(spark_owner_, mqs_spark_topic(s));
        hub.unsubscribe_errors(spark_owner_, mqs_spark_topic(s));
        spark_subscribed_.remove(s);
        spark_stage_pending_.remove(s);
    }
    spark_queue_.clear();
    for (const QString& s : symbols)
        if (wanted.contains(s) && !spark_subscribed_.contains(s) && !live_subscribed_.contains(s) &&
            !spark_queue_.contains(s))
            spark_queue_.append(s);
    // Never behind the first quote cycle: 120 sparklines cost ~17 s in one batch.
    if (first_cycle_done_ && spark_stage_pending_.isEmpty())
        next_spark_stage();
}

void MarketsQuoteStore::next_spark_stage() {
    spark_stage_timer_->stop();
    if (!active_ || !first_cycle_done_)
        return;
    auto& hub = datahub::DataHub::instance();
    int n = 0;
    while (!spark_queue_.isEmpty() && n < markets_logic::kTopicsPerRequest) {
        const QString s = spark_queue_.takeFirst();
        if (!spark_wanted_.contains(s) || spark_subscribed_.contains(s) || !rows_.contains(s))
            continue;
        MarketRow& r = rows_[s];
        if (r.spark_state == MarketRow::Spark::None || r.spark_state == MarketRow::Spark::Failed)
            r.spark_state = MarketRow::Spark::Loading;
        const QString topic = mqs_spark_topic(s);
        hub.subscribe(spark_owner_, topic, [this, s](const QVariant& v) { on_spark(s, v); });
        hub.subscribe_errors(spark_owner_, topic, [this, s](const QString& e) { on_spark_error(s, e); });
        spark_subscribed_.insert(s);
        spark_stage_pending_.insert(s);
        mark_changed(s);
        ++n;
    }
    if (n > 0)
        spark_stage_timer_->start(kStageWaitMs);
}

// ── Coverage ────────────────────────────────────────────────────────────────

MarketsQuoteStore::Coverage MarketsQuoteStore::coverage() const {
    Coverage c;
    c.total = static_cast<int>(panel_symbols_.size());
    c.cycle_running = cycle_running_;
    c.cycle_forced = cycle_forced_;
    for (const QString& s : panel_symbols_) {
        const MarketRow* r = row(s);
        if (!r)
            continue;
        if (r->has_quote) {
            ++c.priced;
            switch (markets_logic::direction_of(r->quote.change)) {
                case 1:
                    ++c.up;
                    break;
                case -1:
                    ++c.down;
                    break;
                default:
                    ++c.flat;
                    break;
            }
            c.newest_quote_ms = std::max(c.newest_quote_ms, r->quote_ms);
            c.max_delay_min = std::max(c.max_delay_min, r->quote.exchange_delay_min);
        }
        if (!r->error.isEmpty())
            ++c.failed;
        if (r->load == MarketRow::Load::Loading || (!r->has_quote && r->error.isEmpty()))
            ++c.loading;
        if (r->timed_out)
            ++c.timed_out;
    }
    c.all_failed = c.total > 0 && c.failed >= c.total;
    return c;
}

QStringList MarketsQuoteStore::failed_lines() const {
    QStringList out;
    for (const QString& s : panel_symbols_) {
        const MarketRow* r = row(s);
        if (r && !r->error.isEmpty())
            out << QStringLiteral("%1: %2").arg(display_name(s), r->error);
    }
    return out;
}

QString MarketsQuoteStore::shared_failure_reason() const {
    QString reason;
    for (const QString& s : panel_symbols_) {
        const MarketRow* r = row(s);
        if (!r || r->error.isEmpty())
            continue;
        // Per-symbol wording names the symbol; compare with it taken out.
        QString e = r->error;
        e.replace(s, QStringLiteral("%1"));
        if (reason.isEmpty())
            reason = e;
        else if (reason != e)
            return {};
    }
    return reason.contains(QStringLiteral("%1")) ? QString() : reason;
}

void MarketsQuoteStore::set_linked_symbol(const QString& symbol) {
    if (linked_ == symbol)
        return;
    linked_ = symbol;
    emit linked_symbol_changed(symbol);
}

// ── Coalescing ──────────────────────────────────────────────────────────────

void MarketsQuoteStore::mark_changed(const QString& symbol) {
    if (!symbol.isEmpty())
        pending_changed_.insert(symbol);
    if (flush_scheduled_)
        return;
    flush_scheduled_ = true;
    QMetaObject::invokeMethod(this, &MarketsQuoteStore::flush, Qt::QueuedConnection);
}

void MarketsQuoteStore::flush() {
    flush_scheduled_ = false;
    QSet<QString> changed;
    changed.swap(pending_changed_);
    if (!changed.isEmpty())
        emit rows_changed(changed);
    emit coverage_changed();
    if (cycle_running_) {
        if (stage_pending_.isEmpty() && !cycle_queue_.isEmpty())
            next_stage();
        maybe_finish_cycle();
    }
    if (active_ && first_cycle_done_ && spark_stage_pending_.isEmpty() && !spark_queue_.isEmpty())
        next_spark_stage();
    if (active_ && interval_ms_ == 0 && auto_ && !changed.isEmpty() && !resort_timer_->isActive())
        resort_timer_->start(kResortMinMs);
}

void MarketsQuoteStore::on_flash_timer() {
    const qint64 now = mqs_now();
    QSet<QString> tick = flashing_;
    for (auto it = flashing_.begin(); it != flashing_.end();) {
        auto r = rows_.find(*it);
        if (r == rows_.end() || now - r->flash_ms >= kFlashMs) {
            if (r != rows_.end()) {
                r->flash_ms = 0;
                r->flash_dir = 0;
            }
            it = flashing_.erase(it);
        } else {
            ++it;
        }
    }
    if (!tick.isEmpty())
        emit flash_tick(tick);
    if (flashing_.isEmpty())
        flash_timer_->stop();
}

void MarketsQuoteStore::request_names() {
    // Names normally arrive on the quote rows; ask resolve_names once per symbol for the
    // few that have none (e.g. CT=F).
    QStringList nameless;
    const QSet<QString> live = live_set();
    QStringList candidates = panel_symbols_;
    for (const QString& s : live)
        if (!panel_set_.contains(s))
            candidates << s;
    for (const QString& s : std::as_const(candidates)) {
        const MarketRow* r = row(s);
        if (r && r->has_quote && r->name.isEmpty() && !names_asked_.contains(s))
            nameless << s;
    }
    if (nameless.isEmpty())
        return;
    for (const QString& s : std::as_const(nameless))
        names_asked_.insert(s);
    QPointer<MarketsQuoteStore> self = this;
    services::MarketDataService::instance().resolve_names(nameless, [self](const QHash<QString, QString>& m) {
        if (!self)
            return;
        for (auto it = m.constBegin(); it != m.constEnd(); ++it) {
            auto r = self->rows_.find(it.key());
            if (r != self->rows_.end() && r->name.isEmpty() && !it.value().trimmed().isEmpty()) {
                r->name = it.value().trimmed();
                self->mark_changed(it.key());
            }
        }
    });
}

} // namespace fincept::screens
