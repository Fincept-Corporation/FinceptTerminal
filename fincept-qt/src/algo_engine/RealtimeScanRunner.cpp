// src/algo_engine/RealtimeScanRunner.cpp
#include "algo_engine/RealtimeScanRunner.h"

#include "algo_engine/ConditionEvaluator.h"
#include "core/logging/Logger.h"
#include "trading/DataStreamManager.h"
#include "trading/TradingTypes.h" // BrokerQuote

#include <QDateTime>
#include <QJsonObject>
#include <QStringList>
#include <QTimer>

#include <functional>

namespace fincept::algo {

RealtimeScanRunner::RealtimeScanRunner(QString watch_id, QStringList universe,
                                       QJsonArray conditions, QString logic, QString timeframe,
                                       QString broker_id, QString account_id, QString data_source,
                                       int sweep_ms, int cooldown_min, QObject* parent)
    : QObject(parent)
    , watch_id_(std::move(watch_id))
    , universe_(std::move(universe))
    , conditions_(std::move(conditions))
    , logic_(std::move(logic))
    , timeframe_(std::move(timeframe))
    , tf_enum_(timeframe_from_string(timeframe_ == QStringLiteral("live") ? QStringLiteral("1m")
                                                                          : timeframe_))
    , broker_id_(std::move(broker_id))
    , account_id_(std::move(account_id))
    , data_source_(std::move(data_source))
    , sweep_ms_(sweep_ms)
    , cooldown_min_(cooldown_min)
    , required_bars_(required_bars(conditions_)) {}

RealtimeScanRunner::~RealtimeScanRunner() {
    qDeleteAll(states_);
    states_.clear();
}

int RealtimeScanRunner::required_bars(const QJsonArray& conditions) {
    int max_need = 2;
    std::function<void(const QJsonArray&)> walk = [&](const QJsonArray& arr) {
        for (const auto& v : arr) {
            const QJsonObject o = v.toObject();
            if (o.contains(QStringLiteral("children"))) {
                walk(o.value(QStringLiteral("children")).toArray());
                continue;
            }
            const int period  = o.value(QStringLiteral("params")).toObject()
                                     .value(QStringLiteral("period")).toInt(0);
            const int cperiod = o.value(QStringLiteral("compare_params")).toObject()
                                     .value(QStringLiteral("period")).toInt(0);
            const int offset  = o.value(QStringLiteral("offset")).toInt(0);
            const int coffset = o.value(QStringLiteral("compare_offset")).toInt(0);
            const int need = qMax(period, cperiod) + qMax(offset, coffset) + 2;
            if (need > max_need)
                max_need = need;
        }
    };
    walk(conditions);
    return max_need;
}

QString RealtimeScanRunner::consumer_id(const QString& symbol) const {
    return QStringLiteral("uscan:") + watch_id_ + QLatin1Char(':') + symbol;
}

void RealtimeScanRunner::start() {
    const int buf = qMax(required_bars_ + 50, 60);
    for (const QString& sym : universe_) {
        auto* st = new SymbolState();
        st->agg = std::make_unique<CandleAggregator>(sym, tf_enum_, buf);
        states_.insert(sym, st);
    }

    // Live feed. open_quote_feed is thread-safe (mutates streams/DataHub on the main
    // thread internally) and delivers the callback on this (the scan) thread because
    // `this` is the owner.
    auto& dsm = fincept::trading::DataStreamManager::instance();
    for (const QString& sym : universe_) {
        dsm.open_quote_feed(this, consumer_id(sym), account_id_, sym,
                            [this, sym](const fincept::trading::BrokerQuote& q) { on_quote(sym, q); });
    }

    sweep_timer_ = new QTimer(this);
    sweep_timer_->setInterval(qMax(100, sweep_ms_));
    connect(sweep_timer_, &QTimer::timeout, this, &RealtimeScanRunner::on_sweep);
    sweep_timer_->start();

    emit status_changed(watch_id_, QStringLiteral("running"));
    LOG_INFO("RealtimeScanRunner",
             QString("watch=%1 started: %2 symbols, sweep=%3ms, req_bars=%4")
                 .arg(watch_id_).arg(universe_.size()).arg(sweep_ms_).arg(required_bars_));
}

void RealtimeScanRunner::stop() {
    if (sweep_timer_) {
        sweep_timer_->stop();
        sweep_timer_->deleteLater();
        sweep_timer_ = nullptr;
    }
    auto& dsm = fincept::trading::DataStreamManager::instance();
    for (const QString& sym : universe_)
        dsm.close_quote_feed(consumer_id(sym), account_id_);
    LOG_INFO("RealtimeScanRunner", QString("watch=%1 stopped").arg(watch_id_));
}

void RealtimeScanRunner::warm(const QString& symbol, const QVector<OhlcvCandle>& candles) {
    auto it = states_.find(symbol);
    if (it == states_.end() || candles.isEmpty())
        return;
    SymbolState* st = it.value();
    st->agg->warm_from(candles);
    st->warmed = true;
    st->dirty = true; // evaluate on the next sweep
}

void RealtimeScanRunner::on_quote(const QString& symbol, const fincept::trading::BrokerQuote& q) {
    auto it = states_.find(symbol);
    if (it == states_.end())
        return;
    SymbolState* st = it.value();
    const int64_t ts = q.timestamp > 0 ? q.timestamp : QDateTime::currentMSecsSinceEpoch();
    st->agg->on_tick(q.ltp, q.volume, ts);
    if (st->last_price > 0)
        st->prev_price = st->last_price;
    st->last_price = q.ltp;
    st->dirty = true;
}

QVector<OhlcvCandle> RealtimeScanRunner::eval_window(const SymbolState& st) const {
    // Mirror DeploymentRunner::live_eval_window — append the previous and current
    // ticks as the last two closed bars so tick-to-tick crossovers are detected.
    QVector<OhlcvCandle> candles = st.agg->closed_candles();
    const int64_t now = QDateTime::currentMSecsSinceEpoch();
    if (st.prev_price > 0) {
        OhlcvCandle prev;
        prev.open = prev.high = prev.low = prev.close = st.prev_price;
        prev.open_time = prev.close_time = now - 1;
        prev.volume = 0;
        prev.is_closed = true;
        candles.append(prev);
    }
    OhlcvCandle cur;
    cur.open = cur.high = cur.low = cur.close = st.last_price;
    cur.open_time = cur.close_time = now;
    cur.volume = 0;
    cur.is_closed = true;
    candles.append(cur);
    return candles;
}

void RealtimeScanRunner::on_sweep() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 cooldown_ms = static_cast<qint64>(qMax(0, cooldown_min_)) * 60000;

    for (auto it = states_.begin(); it != states_.end(); ++it) {
        SymbolState* st = it.value();
        if (!st->warmed || !st->dirty || st->last_price <= 0)
            continue;
        st->dirty = false;

        const QVector<OhlcvCandle> window = eval_window(*st);
        if (window.size() < 2)
            continue;

        const auto eval = ConditionEvaluator::evaluate_group(conditions_, logic_, window);
        if (eval.triggered) {
            if (st->armed && now - st->last_fired_ms >= cooldown_ms) {
                QStringList parts;
                for (const auto& d : eval.details)
                    parts << QString("%1 %2 %3").arg(d.indicator, d.op,
                                                      QString::number(d.target_value, 'f', 2));
                emit match_found(watch_id_, it.key(), st->last_price, parts.join(" & "));
                st->last_fired_ms = now;
                st->armed = false;
            }
        } else {
            st->armed = true;
        }
    }
}

} // namespace fincept::algo
