// src/algo_engine/ScanMonitor.cpp
#include "algo_engine/ScanMonitor.h"

#include "algo_engine/CandleDataFetcher.h"
#include "algo_engine/ConditionEvaluator.h"
#include "algo_engine/RealtimeScanRunner.h"
#include "core/logging/Logger.h"
#include "services/algo_trading/AlgoTradingTypes.h"
#include "services/notifications/NotificationService.h"
#include "storage/repositories/ScanEventRepository.h"
#include "trading/instruments/InstrumentRepository.h"
#include "ui/notifications/NotificationService.h"

#include <QDateTime>
#include <QJsonObject>
#include <QStringList>
#include <QThread>
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
    for (const auto& w : r.value()) {
        if (w.mode == QStringLiteral("realtime"))
            continue; // realtime universe scans are started on demand from the Universe tab
        start_watch(w);
    }
    LOG_INFO("ScanMonitor", QString("started %1 active watch(es)").arg(r.value().size()));
}

void ScanMonitor::start_watch(const ScanWatch& w) {
    stop_watch(w.id); // idempotent
    if (w.mode == QStringLiteral("realtime")) {
        start_realtime(w);
        return;
    }
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
        if (run->rt) {
            QMetaObject::invokeMethod(run->rt, "stop", Qt::QueuedConnection);
            run->rt->deleteLater(); // deleted on the scan thread after stop() runs
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

QStringList ScanMonitor::resolve_universe(const ScanWatch& w) {
    const QString u = w.universe;
    if (u == QStringLiteral("NSE_EQ") || u == QStringLiteral("BSE_EQ")) {
        const QString exch = (u == QStringLiteral("NSE_EQ")) ? QStringLiteral("NSE")
                                                             : QStringLiteral("BSE");
        const auto rows = fincept::trading::InstrumentRepository::instance().list(
            exch, w.broker_id, fincept::trading::InstrumentType::EQ);
        QStringList out;
        out.reserve(rows.size());
        for (const auto& inst : rows)
            out.append(inst.symbol);
        return out;
    }
    if (u == QStringLiteral("NIFTY50"))
        return fincept::services::algo::nifty50_symbols();
    return w.symbols; // '' / CUSTOM
}

void ScanMonitor::start_realtime(const ScanWatch& w) {
    if (w.conditions.isEmpty())
        return;

    const QStringList syms = resolve_universe(w);
    if (syms.isEmpty()) {
        ScanWatchRepository::instance().touch_status(w.id, QStringLiteral("error"),
                                                     QDateTime::currentMSecsSinceEpoch());
        emit watch_status_changed(w.id, QStringLiteral("error"));
        LOG_WARN("ScanMonitor",
                 QString("realtime watch %1: empty universe (instrument master loaded for %2?)")
                     .arg(w.id, w.broker_id));
        return;
    }

    if (!scan_thread_) {
        scan_thread_ = new QThread(this);
        scan_thread_->setObjectName(QStringLiteral("scan-rt"));
        scan_thread_->start();
    }

    const int sweep_ms = w.actions.value(QStringLiteral("sweep_ms")).toInt(500);
    auto* rt = new RealtimeScanRunner(w.id, syms, w.conditions, w.logic, w.timeframe,
                                      w.broker_id, w.account_id, w.data_source,
                                      sweep_ms, w.cooldown_min);
    rt->moveToThread(scan_thread_);
    connect(rt, &RealtimeScanRunner::match_found, this, &ScanMonitor::on_realtime_match);
    connect(rt, &RealtimeScanRunner::status_changed, this,
            [this](const QString& id, const QString& s) {
                ScanWatchRepository::instance().touch_status(
                    id, s, QDateTime::currentMSecsSinceEpoch());
                emit watch_status_changed(id, s);
            });

    auto* run = new Runner();
    run->watch = w;
    run->rt = rt;
    runners_.insert(w.id, run);

    QMetaObject::invokeMethod(rt, "start", Qt::QueuedConnection);

    // Warm-up: fetch history on the MAIN thread (CandleDataFetcher owns a QNAM with
    // main-thread affinity), then forward each symbol's candles to the runner. The
    // forward is guarded by a runners_ lookup (main-thread-only map) so a stop()
    // mid-fetch is a no-op, and uses the receiver-context invoke so it is cancelled
    // if the runner is deleted before delivery.
    const QString tf = (w.timeframe == QStringLiteral("live")) ? QStringLiteral("1m")
                                                               : w.timeframe;
    const int lookback = w.lookback_days > 0
                             ? w.lookback_days
                             : fincept::services::algo::algo_default_lookback_days(tf);
    const DataSource src = data_source_from_string(w.data_source);
    const QString id = w.id, broker = w.broker_id, acct = w.account_id;
    constexpr int kChunk = 50; // throttle: batch the fetch to respect broker rate limits
    for (int s = 0; s < syms.size(); s += kChunk) {
        const QStringList batch = syms.mid(s, kChunk);
        CandleDataFetcher::instance().fetch_multi(
            batch, tf, lookback, src, broker, acct,
            [this, id](const QHash<QString, QVector<OhlcvCandle>>& data, const QStringList&) {
                auto it = runners_.find(id);
                if (it == runners_.end() || !it.value()->rt)
                    return; // watch stopped while fetching
                RealtimeScanRunner* rt = it.value()->rt;
                for (auto d = data.begin(); d != data.end(); ++d) {
                    const QString sym = d.key();
                    const QVector<OhlcvCandle> candles = d.value();
                    QMetaObject::invokeMethod(rt, [rt, sym, candles]() {
                        rt->warm(sym, candles);
                    }, Qt::QueuedConnection);
                }
            });
    }

    ScanWatchRepository::instance().touch_status(w.id, QStringLiteral("warming"),
                                                 QDateTime::currentMSecsSinceEpoch());
    emit watch_status_changed(w.id, QStringLiteral("warming"));
    LOG_INFO("ScanMonitor",
             QString("realtime watch %1 started: %2 symbols").arg(w.id).arg(syms.size()));
}

void ScanMonitor::on_realtime_match(const QString& watch_id, const QString& symbol,
                                    double price, const QString& detail) {
    auto it = runners_.find(watch_id);
    if (it == runners_.end())
        return;
    // Continuous: the runner already applied edge + cooldown gating, so just
    // dispatch (toast / providers / scan_watch_events / watch_fired). Unlike the
    // poll path, do NOT remove the watch — it keeps scanning.
    dispatch(it.value()->watch, symbol, detail);
    // Rich signal for the Universe matches table (carries the live price).
    emit realtime_match(watch_id, symbol, price, detail);
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
