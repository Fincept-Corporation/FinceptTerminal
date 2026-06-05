// src/algo_engine/ScanMonitor.cpp
#include "algo_engine/ScanMonitor.h"

#include "algo_engine/CandleDataFetcher.h"
#include "algo_engine/ConditionEvaluator.h"
#include "core/logging/Logger.h"
#include "services/notifications/NotificationService.h"
#include "storage/repositories/ScanEventRepository.h"
#include "ui/notifications/NotificationService.h"

#include <QDateTime>
#include <QJsonObject>
#include <QStringList>
#include <QTimer>

namespace fincept::algo {

using fincept::ui::ToastService;
using fincept::notifications::NotificationService;
using fincept::notifications::NotificationRequest;
using fincept::notifications::NotifLevel;
using fincept::notifications::NotifTrigger;

ScanMonitor& ScanMonitor::instance() {
    static ScanMonitor s;
    return s;
}

void ScanMonitor::start() {
    auto r = ScanWatchRepository::instance().list_active();
    if (r.is_err()) {
        LOG_ERROR("ScanMonitor", QString("load active watches: %1")
                                     .arg(QString::fromStdString(r.error())));
        return;
    }
    for (const auto& w : r.value())
        start_watch(w);
    LOG_INFO("ScanMonitor", QString("started %1 active watch(es)").arg(r.value().size()));
}

void ScanMonitor::start_watch(const ScanWatch& w) {
    stop_watch(w.id); // idempotent
    auto* run = new Runner();
    run->watch = w;
    run->timer = new QTimer(this);
    const int interval_ms = qMax(30, w.interval_sec) * 1000; // floor 30s (rate limits)
    run->timer->setInterval(interval_ms);
    const QString id = w.id;
    connect(run->timer, &QTimer::timeout, this, [this, id]() { poll(id); });
    runners_.insert(id, run);
    run->timer->start();
    poll(id); // evaluate immediately on (re)start
}

void ScanMonitor::stop_watch(const QString& id) {
    if (auto* run = runners_.take(id)) {
        if (run->timer) {
            run->timer->stop();
            run->timer->deleteLater();
        }
        delete run;
    }
}

void ScanMonitor::reload(const QString& id) {
    auto r = ScanWatchRepository::instance().get(id);
    if (r.is_err()) { stop_watch(id); return; }
    const ScanWatch w = r.value();
    if (!w.active) { stop_watch(id); return; }
    start_watch(w);
}

void ScanMonitor::poll(const QString& id) {
    auto it = runners_.find(id);
    if (it == runners_.end()) return;
    const ScanWatch& w = it.value()->watch;
    if (w.symbols.isEmpty() || w.conditions.isEmpty()) return;

    const DataSource src = data_source_from_string(w.data_source);
    CandleDataFetcher::instance().fetch_multi(
        w.symbols, w.timeframe, w.lookback_days, src, w.broker_id, w.account_id,
        [this, id](const QHash<QString, QVector<OhlcvCandle>>& data, const QStringList& errors) {
            Q_UNUSED(errors);
            on_candles(id, data);
        });
}

void ScanMonitor::on_candles(const QString& id,
                             const QHash<QString, QVector<OhlcvCandle>>& data) {
    auto it = runners_.find(id);
    if (it == runners_.end()) return; // watch removed mid-fetch
    Runner* run = it.value();
    const ScanWatch& w = run->watch;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 cooldown_ms = static_cast<qint64>(qMax(0, w.cooldown_min)) * 60000;
    bool any_warming = false, any_fired = false;

    for (auto sit = data.begin(); sit != data.end(); ++sit) {
        const QString& sym = sit.key();
        const auto& candles = sit.value();
        SymState& st = run->states[sym];

        if (candles.size() < 2) { any_warming = true; continue; }
        auto eval = ConditionEvaluator::evaluate_group(w.conditions, w.logic, candles);

        if (eval.triggered) {
            if (st.armed && now - st.last_fired_ms >= cooldown_ms) {
                QStringList parts;
                for (const auto& d : eval.details)
                    parts << QString("%1 %2 %3").arg(d.indicator, d.op,
                                                      QString::number(d.target_value, 'f', 2));
                dispatch(w, sym, parts.join(" & "));
                st.last_fired_ms = now;
                st.armed = false;
                any_fired = true;
            }
        } else {
            st.armed = true;
        }
    }

    if (any_fired) {
        // One-shot: once a watch triggers it leaves LIVE WATCHES. The fired event
        // is already persisted in scan_watch_events (kept for ALERT HISTORY), so
        // removing the watch row does not erase the history.
        ScanWatchRepository::instance().remove(id);
        emit watch_status_changed(id, QStringLiteral("triggered"));
        QTimer::singleShot(0, this, [this, id]() { stop_watch(id); }); // stop runner after we return
        return;
    }
    const QString status = any_warming ? QStringLiteral("warming") : QStringLiteral("watching");
    ScanWatchRepository::instance().touch_status(id, status, now);
    emit watch_status_changed(id, status);
}

void ScanMonitor::dispatch(const ScanWatch& w, const QString& symbol, const QString& detail) {
    const QString msg = QString("%1 — %2").arg(symbol, detail);
    const QJsonObject actions = w.actions;

    if (actions.value("toast").toBool(true))
        ToastService::instance().post(ToastService::Severity::Warning, msg, "scan:" + w.id);

    if (actions.value("providers").toBool(false)) {
        NotificationRequest req;
        req.title   = QString("Alert: %1").arg(w.name);
        req.message = msg;
        req.level   = NotifLevel::Warning;
        req.trigger = NotifTrigger::PriceAlert;
        NotificationService::instance().send(req);
    }

    ScanWatchRepository::instance().touch_fired(w.id, QDateTime::currentMSecsSinceEpoch());
    fincept::ScanEventRepository::instance().record(w.id, symbol, detail,
                                                    QDateTime::currentMSecsSinceEpoch());
    LOG_INFO("ScanMonitor", QString("FIRE %1 / %2: %3").arg(w.name, symbol, detail));
    emit watch_fired(w.id, symbol, detail);
}

void ScanMonitor::test_fire(const QString& id, const QString& symbol) {
    auto r = ScanWatchRepository::instance().get(id);
    if (r.is_err()) return;
    dispatch(r.value(), symbol.isEmpty() ? QStringLiteral("TEST") : symbol,
             QStringLiteral("test fire"));
}

} // namespace fincept::algo
