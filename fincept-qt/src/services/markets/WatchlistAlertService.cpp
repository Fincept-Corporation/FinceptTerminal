#include "services/markets/WatchlistAlertService.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/markets/MarketDataService.h"
#include "services/notifications/NotificationService.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/notifications/NotificationService.h"

#include <QDateTime>
#include <QTimer>

#include <cmath>

namespace fincept::services {

namespace {

constexpr qint64 kWlAlertCooldownSecs = 5 * 60;

QDateTime wl_alert_parse_utc(const QString& iso) {
    if (iso.isEmpty())
        return {};
    QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    if (!dt.isValid())
        dt = QDateTime::fromString(iso, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (dt.isValid() && dt.timeSpec() == Qt::LocalTime)
        dt.setTimeZone(QTimeZone::UTC);
    return dt;
}

QString wl_alert_price_text(const QString& symbol, const QuoteData& q, double v) {
    namespace fmt = fincept::ui::formatting;
    fmt::InstrumentFmt inst = fmt::instrument_for_symbol(symbol, q.currency);
    if (q.price_digits >= 0)
        inst.decimals = q.price_digits;
    return fmt::price(v, inst);
}

} // namespace

WatchlistAlertService& WatchlistAlertService::instance() {
    static WatchlistAlertService s;
    return s;
}

WatchlistAlertService::WatchlistAlertService() : QObject(nullptr) {}

void WatchlistAlertService::start() {
    if (started_)
        return;
    started_ = true;
    // Deferred: callers include startup code that should not wait on SQLite.
    QTimer::singleShot(0, this, [this]() { reload(); });
}

bool WatchlistAlertService::available() const {
    return WatchlistRepository::instance().has_alerts_table();
}

QVector<WatchlistAlert> WatchlistAlertService::alerts_for(const QString& symbol) const {
    QVector<WatchlistAlert> out;
    for (const auto& a : alerts_) {
        if (a.symbol.compare(symbol, Qt::CaseInsensitive) == 0)
            out.append(a);
    }
    return out;
}

void WatchlistAlertService::reload() {
    auto r = WatchlistRepository::instance().list_alerts();
    if (r.is_err()) {
        LOG_WARN("WatchlistAlerts", "Could not load alerts: " + QString::fromStdString(r.error()));
        return;
    }
    alerts_ = r.value();
    // Keep the observed state of alerts that still exist (a cross needs it).
    QSet<qint64> ids;
    for (const auto& a : alerts_)
        ids.insert(a.id);
    for (auto it = last_state_.begin(); it != last_state_.end();) {
        if (ids.contains(it.key()))
            ++it;
        else
            it = last_state_.erase(it);
    }
    resubscribe();
    emit alerts_changed();
}

void WatchlistAlertService::resubscribe() {
    QSet<QString> want;
    for (const auto& a : alerts_) {
        if (a.enabled)
            want.insert(a.symbol.toUpper());
    }
    auto& hub = datahub::DataHub::instance();
    for (const QString& sym : subscribed_) {
        if (!want.contains(sym))
            hub.unsubscribe(this, MarketDataService::quote_topic(sym));
    }
    QStringList fresh;
    for (const QString& sym : want) {
        if (subscribed_.contains(sym))
            continue;
        const QString topic = MarketDataService::quote_topic(sym);
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (v.canConvert<QuoteData>())
                on_quote(sym, v.value<QuoteData>());
        });
        fresh << topic;
    }
    subscribed_ = want;
    if (!fresh.isEmpty())
        hub.request(fresh);
}

Result<qint64> WatchlistAlertService::save(const WatchlistAlert& alert) {
    auto r = WatchlistRepository::instance().save_alert(alert);
    if (r.is_ok()) {
        last_state_.remove(r.value()); // re-armed: judged afresh on the next quote
        reload();
    }
    return r;
}

Result<void> WatchlistAlertService::remove(qint64 id) {
    auto r = WatchlistRepository::instance().remove_alert(id);
    if (r.is_ok())
        reload();
    return r;
}

Result<void> WatchlistAlertService::set_enabled(qint64 id, bool enabled) {
    for (const auto& a : alerts_) {
        if (a.id != id)
            continue;
        WatchlistAlert copy = a;
        copy.enabled = enabled;
        auto r = save(copy);
        return r.is_ok() ? Result<void>::ok() : Result<void>::err(r.error());
    }
    return Result<void>::err("Unknown alert");
}

QStringList WatchlistAlertService::kinds() {
    return {QStringLiteral("price_above"),    QStringLiteral("price_below"),    QStringLiteral("change_up"),
            QStringLiteral("change_down"),    QStringLiteral("volume_spike"),   QStringLiteral("high_52w"),
            QStringLiteral("low_52w"),        QStringLiteral("ma_cross_above"), QStringLiteral("ma_cross_below")};
}

QString WatchlistAlertService::kind_label(const QString& kind) {
    if (kind == QLatin1String("price_above"))
        return tr("Price rises to or above");
    if (kind == QLatin1String("price_below"))
        return tr("Price falls to or below");
    if (kind == QLatin1String("change_up"))
        return tr("Day change % at least +");
    if (kind == QLatin1String("change_down"))
        return tr("Day change % at most −");
    if (kind == QLatin1String("volume_spike"))
        return tr("Volume spike (x average)");
    if (kind == QLatin1String("high_52w"))
        return tr("New 52-week high");
    if (kind == QLatin1String("low_52w"))
        return tr("New 52-week low");
    if (kind == QLatin1String("ma_cross_above"))
        return tr("Crosses above moving average");
    if (kind == QLatin1String("ma_cross_below"))
        return tr("Crosses below moving average");
    return kind;
}

bool WatchlistAlertService::kind_has_threshold(const QString& kind) {
    return kind == QLatin1String("price_above") || kind == QLatin1String("price_below") ||
           kind == QLatin1String("change_up") || kind == QLatin1String("change_down") ||
           kind == QLatin1String("volume_spike");
}

QString WatchlistAlertService::threshold_label(const QString& kind) {
    if (kind == QLatin1String("price_above") || kind == QLatin1String("price_below"))
        return tr("Price");
    if (kind == QLatin1String("change_up") || kind == QLatin1String("change_down"))
        return tr("Percent");
    if (kind == QLatin1String("volume_spike"))
        return tr("Times the 3-month average");
    return {};
}

bool WatchlistAlertService::kind_is_ma_cross(const QString& kind) {
    return kind == QLatin1String("ma_cross_above") || kind == QLatin1String("ma_cross_below");
}

QString WatchlistAlertService::describe(const WatchlistAlert& a) {
    namespace fmt = fincept::ui::formatting;
    const QString& k = a.kind;
    if (k == QLatin1String("price_above"))
        return tr("Price at or above %1").arg(fmt::number(a.threshold, fmt::price_decimals(a.threshold)));
    if (k == QLatin1String("price_below"))
        return tr("Price at or below %1").arg(fmt::number(a.threshold, fmt::price_decimals(a.threshold)));
    if (k == QLatin1String("change_up"))
        return tr("Up %1 on the day").arg(fmt::pct(std::abs(a.threshold), 2, false));
    if (k == QLatin1String("change_down"))
        return tr("Down %1 on the day").arg(fmt::pct(std::abs(a.threshold), 2, false));
    if (k == QLatin1String("volume_spike"))
        return tr("Volume %1 the average").arg(fmt::ratio(a.threshold, 1));
    if (k == QLatin1String("high_52w"))
        return tr("New 52-week high");
    if (k == QLatin1String("low_52w"))
        return tr("New 52-week low");
    if (k == QLatin1String("ma_cross_above"))
        return tr("Crosses above the %1-day average").arg(a.ma_period == 200 ? 200 : 50);
    if (k == QLatin1String("ma_cross_below"))
        return tr("Crosses below the %1-day average").arg(a.ma_period == 200 ? 200 : 50);
    return k;
}

bool WatchlistAlertService::fired_today(const WatchlistAlert& a) {
    const QDateTime dt = wl_alert_parse_utc(a.last_triggered_at);
    return dt.isValid() && dt.toLocalTime().date() == QDate::currentDate();
}

std::optional<bool> WatchlistAlertService::condition(const WatchlistAlert& a, const QuoteData& q) {
    if (!(q.price > 0))
        return std::nullopt;
    const QString& k = a.kind;
    if (k == QLatin1String("price_above"))
        return q.price >= a.threshold;
    if (k == QLatin1String("price_below"))
        return q.price <= a.threshold;
    if (k == QLatin1String("change_up"))
        return q.change_pct >= std::abs(a.threshold);
    if (k == QLatin1String("change_down"))
        return q.change_pct <= -std::abs(a.threshold);
    if (k == QLatin1String("volume_spike")) {
        if (!(q.volume > 0) || !std::isfinite(q.average_volume) || !(q.average_volume > 0))
            return std::nullopt;
        return q.volume / q.average_volume >= a.threshold;
    }
    if (k == QLatin1String("high_52w")) {
        if (!std::isfinite(q.fifty_two_week_high) || !(q.high > 0))
            return std::nullopt;
        return q.high >= q.fifty_two_week_high;
    }
    if (k == QLatin1String("low_52w")) {
        if (!std::isfinite(q.fifty_two_week_low) || !(q.low > 0))
            return std::nullopt;
        return q.low <= q.fifty_two_week_low;
    }
    if (kind_is_ma_cross(k)) {
        const double ma = a.ma_period == 200 ? q.two_hundred_day_average : q.fifty_day_average;
        if (!std::isfinite(ma) || !(ma > 0))
            return std::nullopt;
        const bool above = q.price > ma;
        return k == QLatin1String("ma_cross_above") ? above : !above;
    }
    return std::nullopt;
}

void WatchlistAlertService::on_quote(const QString& symbol, const QuoteData& q) {
    bool changed = false;
    for (auto& a : alerts_) {
        if (!a.enabled || a.symbol.compare(symbol, Qt::CaseInsensitive) != 0)
            continue;
        const auto c = condition(a, q);
        if (!c)
            continue;
        const int prev = last_state_.value(a.id, -1);
        const int now = *c ? 1 : 0;
        last_state_.insert(a.id, now);
        bool fire_now = false;
        if (kind_is_ma_cross(a.kind)) {
            fire_now = prev == 0 && now == 1; // an observed change of side only
        } else if (now == 1) {
            if (prev == 0)
                fire_now = true;
            else if (prev == -1)
                fire_now = !(a.repeat && fired_today(a)); // no repeat on every restart
        }
        if (fire_now && a.repeat) {
            const QDateTime last = wl_alert_parse_utc(a.last_triggered_at);
            if (last.isValid() && last.secsTo(QDateTime::currentDateTimeUtc()) < kWlAlertCooldownSecs)
                fire_now = false;
        }
        if (fire_now) {
            fire(a, q);
            changed = true;
        }
    }
    if (changed) {
        // A one-shot alert that fired may leave its symbol unwatched. Deferred:
        // this runs inside the hub's delivery for that very topic.
        QTimer::singleShot(0, this, [this]() { resubscribe(); });
        emit alerts_changed();
    }
}

void WatchlistAlertService::fire(WatchlistAlert& a, const QuoteData& q) {
    const QString when = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    auto r = WatchlistRepository::instance().mark_alert_triggered(a.id, when, !a.repeat);
    if (r.is_err())
        LOG_WARN("WatchlistAlerts", "Could not record a fired alert: " + QString::fromStdString(r.error()));
    a.last_triggered_at = when;
    ++a.trigger_count;
    if (!a.repeat)
        a.enabled = false;

    const QString msg = tr("%1: %2 — last %3 (%4)")
                            .arg(a.symbol, describe(a), wl_alert_price_text(a.symbol, q, q.price),
                                 fincept::ui::formatting::pct(q.change_pct));
    const QString full = a.note.isEmpty() ? msg : msg + QStringLiteral("\n") + a.note;

    fincept::ui::ToastService::instance().post(fincept::ui::ToastService::Severity::Warning, full,
                                               QStringLiteral("scan:watchlist-alert:%1").arg(a.id));
    notifications::NotificationRequest req;
    req.title = tr("Watchlist alert: %1").arg(a.symbol);
    req.message = full;
    req.level = notifications::NotifLevel::Alert;
    req.trigger = notifications::NotifTrigger::PriceAlert;
    notifications::NotificationService::instance().send(req);

    // Not the user's note (free text): the alert, its symbol and the trigger text.
    LOG_INFO("WatchlistAlerts", logf("Watchlist alert fired", {{"alert", a.id}, {"symbol", a.symbol}, {"trigger", msg}}));
    emit alert_fired(a.id, a.symbol, msg);
}

} // namespace fincept::services
