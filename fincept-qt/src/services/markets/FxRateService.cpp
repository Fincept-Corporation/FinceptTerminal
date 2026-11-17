// FxRateService.cpp — see FxRateService.h. The leg / cross maths is in FxRateMath.cpp.

#include "services/markets/FxRateService.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/markets/MarketDataService.h"
#include "ui/formatting/CurrencyTable.h"
#include "ui/formatting/FormatSettings.h"

#include <QCoreApplication>
#include <QHash>
#include <QPointer>
#include <QThread>
#include <QTimeZone>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <utility>

namespace fincept::services {

namespace fxsvc_detail {

constexpr int kFxHistoryTimeoutMs = 120'000;
constexpr int kFxMaxFillDays = 5;
constexpr int kFxChangedCoalesceMs = 50;

const QString& fxsvc_usd() {
    static const QString k = QStringLiteral("USD");
    return k;
}

/// The hub's current sample of a Yahoo symbol (any age; FxRateMath judges it).
std::optional<fxmath::FxLegSample> fxsvc_hub_sample(const QString& symbol) {
    if (symbol.isEmpty())
        return std::nullopt;
    auto& hub = datahub::DataHub::instance();
    const QString topic = MarketDataService::quote_topic(symbol);
    const qint64 published = hub.last_publish_ms(topic);
    if (published <= 0)
        return std::nullopt;
    const QVariant v = hub.peek_raw(topic);
    if (!v.canConvert<QuoteData>())
        return std::nullopt;
    const QuoteData q = v.value<QuoteData>();
    if (!std::isfinite(q.price) || q.price <= 0)
        return std::nullopt;
    // Received = when Yahoo answered (a cached quote keeps its fetch time).
    return fxmath::FxLegSample{q.price, MarketDataService::quote_fetched_ms(q, published), q.market_time};
}

// True when every quote the rate can come from is still current in the hub
// (MarketDataService::quote_is_current: inside its session-aware freshness
// window). FX is not polled over the weekend, nor a closed market's pair until
// it reopens, so such a rate grows old because the market is shut, not
// because the feed died — it is not flagged stale. A direct cross, when
// current, is the source a stale-flagged quote came from (fx_combine prefers
// it over stale legs); otherwise both USD legs must be current.
bool fxsvc_rate_current(const QString& from_ccy, const QString& to_ccy) {
    const QString from = ui::formatting::major_currency(from_ccy);
    const QString to = ui::formatting::major_currency(to_ccy);
    if (from.isEmpty() || to.isEmpty())
        return false;
    if (from != fxsvc_usd() && to != fxsvc_usd() && !fxmath::fx_is_crypto_leg(from) && !fxmath::fx_is_crypto_leg(to) &&
        (MarketDataService::quote_is_current(from + to + QStringLiteral("=X")) ||
         MarketDataService::quote_is_current(to + from + QStringLiteral("=X"))))
        return true;
    for (const QString& ccy : {from, to}) {
        const QString leg = fxmath::fx_leg_symbol(ccy);
        if (!leg.isEmpty() && !MarketDataService::quote_is_current(leg))
            return false;
    }
    return true;
}

// Smallest Yahoo period that reaches `days_back` days into the past. A handful
// of canonical periods keeps hub topics shared between callers.
QString fxsvc_history_period(qint64 days_back) {
    if (days_back <= 28)
        return QStringLiteral("1mo");
    if (days_back <= 89)
        return QStringLiteral("3mo");
    if (days_back <= 180)
        return QStringLiteral("6mo");
    if (days_back <= 365)
        return QStringLiteral("1y");
    if (days_back <= 730)
        return QStringLiteral("2y");
    if (days_back <= 1826)
        return QStringLiteral("5y");
    if (days_back <= 3652)
        return QStringLiteral("10y");
    return QStringLiteral("max");
}

// Daily bars -> date -> USD per unit. Yahoo stamps FX daily bars at local
// (London) midnight, which is 23:00 UTC the day before in summer, so the date
// is taken 12 h after the stamp.
QMap<QDate, double> fxsvc_leg_series(const QString& ccy, const QVector<HistoryPoint>& points) {
    QMap<QDate, double> out;
    for (const HistoryPoint& p : points) {
        if (!std::isfinite(p.close) || p.close <= 0 || p.timestamp <= 0)
            continue;
        const QDate d = QDateTime::fromSecsSinceEpoch(p.timestamp + 12 * 3600, QTimeZone::utc()).date();
        out.insert(d, fxmath::fx_usd_per_unit(ccy, p.close));
    }
    return out;
}

// Last observation on or before `d`, at most kFxMaxFillDays old.
bool fxsvc_value_on(const QMap<QDate, double>& series, QDate d, double* value) {
    auto it = series.upperBound(d);
    if (it == series.cbegin())
        return false;
    --it;
    if (it.key().daysTo(d) > kFxMaxFillDays)
        return false;
    *value = it.value();
    return true;
}

QString fxsvc_cache_key(const QString& from_ccy, const QString& to_ccy) {
    return from_ccy.trimmed() + QLatin1Char('|') + to_ccy.trimmed();
}

} // namespace fxsvc_detail

struct FxRateService::HistoryJob {
    QString from;
    QString to;
    QDate start;
    QDate last;
    double scale = 1.0;
    QStringList ccys; // legs to fetch (non-USD)
    QHash<QString, QMap<QDate, double>> legs;
    HistoryCallback cb;
    QPointer<QObject> owner; // subscription lifetime guard
    bool done = false;
};

FxRateService& FxRateService::instance() {
    static FxRateService s;
    return s;
}

FxRateService::FxRateService() {
    // Hub subscriptions and timers belong on the GUI thread even if a worker
    // happens to touch instance() first.
    if (QCoreApplication* app = QCoreApplication::instance(); app && thread() != app->thread())
        moveToThread(app->thread());
    changed_timer_ = new QTimer(this);
    changed_timer_->setSingleShot(true);
    changed_timer_->setInterval(fxsvc_detail::kFxChangedCoalesceMs);
    connect(changed_timer_, &QTimer::timeout, this, &FxRateService::rates_changed);

    // The stale threshold is the user's "FX stale after" setting.
    auto& fs = ui::formatting::FormatSettings::instance();
    stale_after_sec_.store(qint64(fs.fx_stale_minutes()) * 60);
    connect(&fs, &ui::formatting::FormatSettings::changed, this, [this]() {
        const qint64 sec = qint64(ui::formatting::FormatSettings::instance().fx_stale_minutes()) * 60;
        if (stale_after_sec_.exchange(sec) != sec)
            emit rates_changed(); // figures may move into / out of the stale state
    });
}

QString FxRateService::usd_leg_symbol(const QString& ccy) {
    return fxmath::fx_leg_symbol(ccy);
}

bool FxRateService::is_convertible(const QString& ccy) {
    const QString c = ui::formatting::major_currency(ccy);
    return !c.isEmpty() && fxmath::fx_valid_code(c);
}

void FxRateService::set_stale_after(std::chrono::seconds age) {
    stale_after_sec_.store(std::max<qint64>(1, age.count()));
}

std::chrono::seconds FxRateService::stale_after() const {
    return std::chrono::seconds(stale_after_sec_.load());
}

void FxRateService::set_max_age(std::chrono::seconds age) {
    set_stale_after(age);
}

std::chrono::seconds FxRateService::max_age() const {
    return stale_after();
}

// ── Spot ────────────────────────────────────────────────────────────────────

std::optional<FxQuote> FxRateService::quote(const QString& from_ccy, const QString& to_ccy) const {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 stale_after = stale_after_sec_.load();
    const QString key = fxsvc_detail::fxsvc_cache_key(from_ccy, to_ccy);
    {
        QMutexLocker lock(&cache_mutex_);
        const auto it = cache_.constFind(key);
        if (it != cache_.constEnd()) {
            FxQuote q = it.value();
            if (q.received_ms <= 0 || now - q.received_ms <= fxmath::kFxHardMaxAgeSec * 1000) {
                q.stale = q.received_ms > 0 && now - q.received_ms > stale_after * 1000;
                lock.unlock(); // the hub check below takes the hub's own lock
                if (q.stale && fxsvc_detail::fxsvc_rate_current(from_ccy, to_ccy))
                    q.stale = false;
                return q;
            }
            cache_.remove(key);
        }
    }
    fxmath::FxCombine c = fxmath::fx_combine(from_ccy, to_ccy, &fxsvc_detail::fxsvc_hub_sample, now, stale_after);
    if (!c.missing.isEmpty())
        request_missing_legs(c.missing);
    // Same-currency and approximate (peg) quotes are not cached: the first is
    // trivial, the second must give way as soon as the real leg lands.
    if (c.quote && c.quote->received_ms > 0 && !c.quote->approximate) {
        QMutexLocker lock(&cache_mutex_);
        cache_.insert(key, *c.quote);
    }
    if (c.quote && c.quote->stale && fxsvc_detail::fxsvc_rate_current(from_ccy, to_ccy))
        c.quote->stale = false;
    return c.quote;
}

std::optional<double> FxRateService::rate(const QString& from_ccy, const QString& to_ccy) const {
    const auto q = quote(from_ccy, to_ccy);
    if (!q)
        return std::nullopt;
    return q->rate;
}

std::optional<double> FxRateService::convert(double amount, const QString& from_ccy, const QString& to_ccy) const {
    if (!std::isfinite(amount))
        return std::nullopt;
    const auto r = rate(from_ccy, to_ccy);
    if (!r)
        return std::nullopt;
    return amount * *r;
}

QDateTime FxRateService::as_of(const QString& from_ccy, const QString& to_ccy) const {
    const auto q = quote(from_ccy, to_ccy);
    return q ? q->as_of : QDateTime();
}

ui::formatting::ConvertedMoney FxRateService::convert_for_display(double amount, const QString& from_ccy,
                                                                  const QString& to_ccy) const {
    namespace uifmt = ui::formatting;
    uifmt::ConvertedMoney c;
    c.source_amount = amount;
    c.source_currency = from_ccy.trimmed();
    const QString target = uifmt::major_currency(to_ccy.trimmed().isEmpty() ? uifmt::FormatSettings::instance().display_currency()
                                                                            : to_ccy);
    c.currency = target;
    if (!std::isfinite(amount) || c.source_currency.isEmpty() || target.isEmpty())
        return c;
    const QString src_major = uifmt::major_currency(c.source_currency);
    if (src_major == target) {
        c.amount = uifmt::to_major_units(amount, c.source_currency);
        c.rate = 1.0;
        return c;
    }
    // The rate per MAJOR unit of the source ("1 GBP = 1.3183 USD"), applied to
    // the amount in major units, so a pence amount and its note agree.
    const auto q = quote(src_major, target);
    if (!q)
        return c;
    c.rate = q->rate;
    c.amount = uifmt::to_major_units(amount, c.source_currency) * q->rate;
    c.as_of = q->as_of;
    c.converted = true;
    c.stale = q->stale;
    c.triangulated = q->triangulated;
    c.approximate = q->approximate;
    c.approx_reason = q->approx_reason;
    if (!std::isfinite(c.amount))
        c.amount = std::numeric_limits<double>::quiet_NaN();
    return c;
}

// ── Subscriptions ───────────────────────────────────────────────────────────

void FxRateService::request_missing_legs(const QStringList& ccys) const {
    QStringList todo;
    {
        QMutexLocker lock(&subs_mutex_);
        for (const QString& c : ccys)
            if (c != fxsvc_detail::fxsvc_usd() && !subscribed_.contains(c))
                todo << c;
    }
    if (todo.isEmpty())
        return;
    QList<CurrencyPair> pairs;
    for (const QString& c : std::as_const(todo))
        pairs.append({c, fxsvc_detail::fxsvc_usd()});
    auto* self = const_cast<FxRateService*>(this);
    QMetaObject::invokeMethod(self, [self, pairs]() { self->ensure(pairs); }, Qt::QueuedConnection);
}

void FxRateService::ensure(const QList<CurrencyPair>& pairs) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, pairs]() { ensure(pairs); }, Qt::QueuedConnection);
        return;
    }
    QSet<QString> ccys;
    for (const CurrencyPair& p : pairs) {
        const QString from = ui::formatting::major_currency(p.first);
        const QString to = ui::formatting::major_currency(p.second);
        if (!fxmath::fx_valid_code(from) || !fxmath::fx_valid_code(to) || from == to)
            continue;
        ccys.insert(from);
        ccys.insert(to);
    }
    for (const QString& c : std::as_const(ccys))
        subscribe_leg(c);
}

void FxRateService::ensure_currencies(const QStringList& currencies, const QString& to_ccy) {
    QList<CurrencyPair> pairs;
    pairs.reserve(currencies.size());
    for (const QString& c : currencies)
        pairs.append({c, to_ccy});
    ensure(pairs);
}

void FxRateService::subscribe_leg(const QString& ccy) {
    if (ccy == fxsvc_detail::fxsvc_usd())
        return;
    const QString symbol = usd_leg_symbol(ccy);
    if (symbol.isEmpty())
        return;
    {
        QMutexLocker lock(&subs_mutex_);
        if (subscribed_.contains(ccy))
            return;
        subscribed_.insert(ccy);
    }
    // The hub keeps a subscribed quote topic refreshed (quote TTL ~30 s) through
    // MarketDataService's batched quote fetch, and delivers the cached value
    // first if it is fresh. A publish invalidates every cached pair.
    datahub::DataHub::instance().subscribe(this, MarketDataService::quote_topic(symbol), [this](const QVariant&) {
        {
            QMutexLocker lock(&cache_mutex_);
            cache_.clear();
        }
        if (!changed_timer_->isActive())
            changed_timer_->start();
    });
    LOG_DEBUG("FxRate", QStringLiteral("Subscribed FX leg %1").arg(symbol));
}

// ── History ─────────────────────────────────────────────────────────────────

void FxRateService::historical_rates(const QString& from_ccy, const QString& to_ccy, QDate start, QDate end,
                                     HistoryCallback cb) {
    if (!cb)
        return;
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(
            this, [this, from_ccy, to_ccy, start, end, cb]() { historical_rates(from_ccy, to_ccy, start, end, cb); },
            Qt::QueuedConnection);
        return;
    }
    const auto reply_later = [this, cb](bool ok, const QMap<QDate, double>& series, const QString& error) {
        QMetaObject::invokeMethod(this, [cb, ok, series, error]() { cb(ok, series, error); }, Qt::QueuedConnection);
    };

    const QString from = ui::formatting::major_currency(from_ccy);
    const QString to = ui::formatting::major_currency(to_ccy);
    if (!fxmath::fx_valid_code(from) || !fxmath::fx_valid_code(to)) {
        reply_later(false, {}, tr("Unknown currency pair %1/%2.").arg(from_ccy, to_ccy));
        return;
    }
    const QDate today = QDateTime::currentDateTimeUtc().date();
    const QDate last = end.isValid() ? std::min(end, today) : today;
    if (!start.isValid() || start > last) {
        reply_later(false, {}, tr("Invalid date range for %1/%2 FX history.").arg(from, to));
        return;
    }
    const double scale = double(ui::formatting::minor_unit_divisor(to_ccy)) /
                         double(ui::formatting::minor_unit_divisor(from_ccy));
    if (from == to) {
        QMap<QDate, double> series;
        for (QDate d = start; d <= last; d = d.addDays(1))
            series.insert(d, scale);
        reply_later(true, series, {});
        return;
    }

    auto job = std::make_shared<HistoryJob>();
    job->from = from;
    job->to = to;
    job->start = start;
    job->last = last;
    job->scale = scale;
    job->cb = std::move(cb);
    if (from != fxsvc_detail::fxsvc_usd())
        job->ccys << from;
    if (to != fxsvc_detail::fxsvc_usd())
        job->ccys << to;

    // Reach back far enough to seed the forward fill at `start`.
    const QString period = fxsvc_detail::fxsvc_history_period(start.daysTo(today) + fxsvc_detail::kFxMaxFillDays + 2);
    auto* owner = new QObject(this);
    job->owner = owner;
    auto& hub = datahub::DataHub::instance();
    for (const QString& c : std::as_const(job->ccys)) {
        const QString topic = MarketDataService::history_topic(usd_leg_symbol(c), period, QStringLiteral("1d"));
        hub.subscribe(owner, topic, [this, job, c](const QVariant& v) { on_history_leg(job, c, v); });
        hub.subscribe_errors(owner, topic,
                             [this, job, c, topic](const QString& err) { on_history_error(job, c, topic, err); });
    }
    QTimer::singleShot(fxsvc_detail::kFxHistoryTimeoutMs, owner, [this, job]() {
        finish_history(job, false, tr("Timed out fetching %1/%2 FX history.").arg(job->from, job->to));
    });
}

void FxRateService::on_history_leg(const std::shared_ptr<HistoryJob>& job, const QString& ccy,
                                   const QVariant& value) {
    if (job->done)
        return;
    const auto points = value.value<QVector<HistoryPoint>>();
    const QMap<QDate, double> series = fxsvc_detail::fxsvc_leg_series(ccy, points);
    if (series.isEmpty()) {
        finish_history(job, false, tr("No %1 FX history returned.").arg(usd_leg_symbol(ccy)));
        return;
    }
    job->legs.insert(ccy, series);
    for (const QString& c : std::as_const(job->ccys))
        if (!job->legs.contains(c))
            return; // wait for the other leg
    finish_history(job, true, {});
}

void FxRateService::on_history_error(const std::shared_ptr<HistoryJob>& job, const QString& ccy,
                                     const QString& topic, const QString& error) {
    if (job->done || job->legs.contains(ccy))
        return;
    // Past daily closes do not change: an earlier copy of the series in the hub
    // is still correct for every date it covers.
    const QVariant cached = datahub::DataHub::instance().peek_raw(topic);
    if (cached.isValid()) {
        const QMap<QDate, double> series =
            fxsvc_detail::fxsvc_leg_series(ccy, cached.value<QVector<HistoryPoint>>());
        if (!series.isEmpty()) {
            on_history_leg(job, ccy, cached);
            return;
        }
    }
    finish_history(job, false, error.isEmpty() ? tr("FX history for %1 is unavailable.").arg(usd_leg_symbol(ccy))
                                               : error);
}

void FxRateService::finish_history(const std::shared_ptr<HistoryJob>& job, bool ok, const QString& error) {
    if (job->done)
        return;
    job->done = true;
    if (job->owner)
        job->owner->deleteLater(); // drops the hub subscriptions

    QMap<QDate, double> out;
    QString err = error;
    if (ok) {
        auto value_of = [&job](const QString& c, QDate d, double* v) -> bool {
            if (c == fxsvc_detail::fxsvc_usd()) {
                *v = 1.0;
                return true;
            }
            const auto it = job->legs.constFind(c);
            return it != job->legs.constEnd() && fxsvc_detail::fxsvc_value_on(it.value(), d, v);
        };
        for (QDate d = job->start; d <= job->last; d = d.addDays(1)) {
            double uf = 0;
            double ut = 0;
            if (value_of(job->from, d, &uf) && value_of(job->to, d, &ut) && ut > 0) {
                const double r = uf / ut * job->scale;
                if (std::isfinite(r) && r > 0)
                    out.insert(d, r);
            }
        }
        if (out.isEmpty()) {
            ok = false;
            err = tr("No %1/%2 FX history between %3 and %4.")
                      .arg(job->from, job->to, job->start.toString(Qt::ISODate), job->last.toString(Qt::ISODate));
        }
    }
    if (!ok)
        LOG_WARN("FxRate", QStringLiteral("FX history %1/%2 failed: %3").arg(job->from, job->to, err));
    const HistoryCallback cb = job->cb;
    job->cb = nullptr;
    if (cb)
        cb(ok, out, ok ? QString() : err);
}

} // namespace fincept::services
