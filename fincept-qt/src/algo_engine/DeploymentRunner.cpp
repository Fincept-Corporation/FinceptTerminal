// src/algo_engine/DeploymentRunner.cpp
#include "algo_engine/DeploymentRunner.h"
#include "algo_engine/CandleDataFetcher.h"
#include "algo_engine/ConditionEvaluator.h"
#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "storage/sqlite/Database.h"

using fincept::Database;
#include "trading/AccountManager.h"
#include "trading/BrokerInterface.h"
#include "trading/BrokerRegistry.h"
#include "trading/BrokerTopic.h"
#include "trading/TradingTypes.h"

#include <QDateTime>
#include <QPointer>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>
#include <QtConcurrent>

namespace fincept::algo {

DeploymentRunner::DeploymentRunner(const services::algo::AlgoDeployment& deployment,
                                   const services::algo::AlgoStrategy& strategy,
                                   QObject* parent)
    : QObject(parent)
    , deployment_(deployment)
    , strategy_(strategy)
    , timeframe_(timeframe_from_string(deployment.timeframe.isEmpty() ? strategy.timeframe : deployment.timeframe)) {

    // "live" timeframe → evaluate entry/exit on every quote (tick), not on candle
    // close. (timeframe_ itself falls back to M5 for aggregation/warm-up history.)
    const QString tf_str = deployment.timeframe.isEmpty() ? strategy.timeframe : deployment.timeframe;
    live_mode_ = (tf_str.compare(QStringLiteral("live"), Qt::CaseInsensitive) == 0);

    // Capacity 500 so long indicators (SMA200) + a crossover lookback fit.
    aggregator_ = std::make_unique<CandleAggregator>(deployment.symbol, timeframe_, 500, this);
    connect(aggregator_.get(), &CandleAggregator::candle_closed,
            this, &DeploymentRunner::on_candle_closed);

    position_mgr_ = std::make_unique<PositionManager>(
        deployment.id,
        strategy.stop_loss, strategy.take_profit, strategy.trailing_stop,
        deployment.max_order_value, deployment.max_daily_loss);

    heartbeat_timer_ = new QTimer(this);
    heartbeat_timer_->setInterval(5000);
    connect(heartbeat_timer_, &QTimer::timeout, this, &DeploymentRunner::on_heartbeat);

    // Poll the broker for a fresh quote every 3s while running. Lives on the engine
    // thread (the runner is moved there before start()), so the timer fires there.
    quote_timer_ = new QTimer(this);
    quote_timer_->setInterval(3000);
    connect(quote_timer_, &QTimer::timeout, this, &DeploymentRunner::poll_quote);
}

DeploymentRunner::~DeploymentRunner() {
    if (running_.load())
        stop();
}

void DeploymentRunner::start() {
    if (running_.load()) return;

    running_ = true;
    paused_ = false;
    last_heartbeat_ms_ = QDateTime::currentMSecsSinceEpoch();

    // Resume any open position + cumulative metrics persisted before a restart.
    restore_state_from_db();

    // Warm the aggregator with historical candles before going live, so indicators
    // are valid immediately. Without this the buffer starts empty and the strategy
    // can't trigger until hundreds of live bars accumulate. Live ticks are
    // subscribed only after the backfill attempt (warm_from replaces the buffer).
    const QString tf = timeframe_to_string(timeframe_);
    const int lookback = services::algo::algo_default_lookback_days(tf);
    const DataSource src = deployment_.broker_id.isEmpty() ? DataSource::YFinance : DataSource::Auto;
    QPointer<DeploymentRunner> self = this;
    CandleDataFetcher::instance().fetch(
        deployment_.symbol, tf, lookback, src, deployment_.broker_id, deployment_.broker_account_id,
        [self](bool ok, const QVector<OhlcvCandle>& candles, const QString& err) {
            if (!self || !self->running_.load())
                return;
            if (ok && !candles.isEmpty()) {
                self->aggregator_->warm_from(candles);
                LOG_INFO("AlgoEngine", QString("Deployment %1 warmed with %2 historical candles")
                                           .arg(self->deployment_.id)
                                           .arg(candles.size()));
            } else {
                LOG_WARN("AlgoEngine",
                         QString("Deployment %1 backfill failed (%2) — warming from live ticks only")
                             .arg(self->deployment_.id, err));
            }
            self->start_market_data();
        });

    heartbeat_timer_->start();

    update_deployment_status(QStringLiteral("running"));
    emit status_changed(deployment_.id, QStringLiteral("running"));
    LOG_INFO("AlgoEngine", QString("Deployment %1 started: %2 on %3")
             .arg(deployment_.id, strategy_.name, deployment_.symbol));
}

void DeploymentRunner::pause() {
    paused_ = true;
    emit status_changed(deployment_.id, QStringLiteral("paused"));
    LOG_INFO("AlgoEngine", QString("Deployment %1 paused").arg(deployment_.id));
}

void DeploymentRunner::resume() {
    paused_ = false;
    emit status_changed(deployment_.id, QStringLiteral("running"));
    LOG_INFO("AlgoEngine", QString("Deployment %1 resumed").arg(deployment_.id));
}

void DeploymentRunner::stop() {
    if (!running_.load()) return;

    running_ = false;
    paused_ = false;
    heartbeat_timer_->stop();
    stop_market_data();
    persist_metrics();
    update_deployment_status(QStringLiteral("stopped"));
    emit status_changed(deployment_.id, QStringLiteral("stopped"));
    LOG_INFO("AlgoEngine", QString("Deployment %1 stopped").arg(deployment_.id));
}

void DeploymentRunner::start_market_data() {
    if (deployment_.broker_id.isEmpty() || deployment_.broker_account_id.isEmpty()) {
        LOG_WARN("AlgoEngine",
                 QString("Deployment %1: no broker account attached — cannot source live "
                         "market data (deploy again and pick a connected broker).")
                     .arg(deployment_.id));
        return;
    }
    LOG_INFO("AlgoEngine", QString("Deployment %1: polling %2 quotes from broker '%3' every 3s")
                               .arg(deployment_.id, deployment_.symbol, deployment_.broker_id));
    quote_timer_->start();
    poll_quote(); // fire the first one immediately rather than waiting 3s
}

void DeploymentRunner::stop_market_data() {
    if (quote_timer_)
        quote_timer_->stop();
}

void DeploymentRunner::poll_quote() {
    if (!running_.load() || paused_.load())
        return;
    const QString broker_id = deployment_.broker_id;
    const QString account_id = deployment_.broker_account_id;
    const QString symbol = deployment_.symbol;
    if (broker_id.isEmpty() || account_id.isEmpty())
        return;

    // Hit the broker's quote REST endpoint off-thread; deliver the result back on
    // the engine thread via on_tick_data (same entry point a WS tick would use).
    QPointer<DeploymentRunner> self = this;
    QtConcurrent::run([self, broker_id, account_id, symbol]() {
        auto* broker = trading::BrokerRegistry::instance().get(broker_id);
        if (!broker)
            return;
        auto creds = trading::AccountManager::instance().load_credentials(account_id);
        auto resp = broker->get_quotes(creds, {symbol});
        if (!resp.success || !resp.data.has_value() || resp.data->isEmpty())
            return;
        const trading::BrokerQuote quote = resp.data->first();
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, quote]() {
                if (self && self->running_.load())
                    self->on_tick_data(QVariant::fromValue(quote));
            },
            Qt::QueuedConnection);
    });
}

void DeploymentRunner::on_tick_data(const QVariant& data) {
    if (!running_.load() || paused_.load()) return;

    last_heartbeat_ms_ = QDateTime::currentMSecsSinceEpoch();

    double price = 0;
    double volume = 0;
    int64_t timestamp = QDateTime::currentMSecsSinceEpoch();

    if (data.canConvert<fincept::trading::BrokerQuote>()) {
        auto quote = data.value<fincept::trading::BrokerQuote>();
        price = quote.ltp;
        volume = quote.volume;
        timestamp = quote.timestamp > 0 ? quote.timestamp : timestamp;
    } else if (data.canConvert<QVariantMap>()) {
        auto map = data.toMap();
        price = map.value("ltp", map.value("price")).toDouble();
        volume = map.value("volume").toDouble();
    } else {
        return;
    }

    if (price <= 0) return;

    if (!first_tick_logged_) {
        first_tick_logged_ = true;
        LOG_INFO("AlgoEngine", QString("Deployment %1: receiving live quotes (%2 = %3) from '%4'")
                                   .arg(deployment_.id, deployment_.symbol)
                                   .arg(price)
                                   .arg(deployment_.broker_id));
    }

    position_mgr_->update_price(price);

    auto risk_signal = position_mgr_->check_risk(price);
    if (risk_signal) {
        risk_signal->account_id = deployment_.broker_account_id;
        risk_signal->symbol = deployment_.symbol;
        risk_signal->exchange = deployment_.exchange;
        risk_signal->product_type = deployment_.product_type;
        risk_signal->price = price; // current market price for the simulated/real exit
        emit_order_signal(*risk_signal);
    }

    aggregator_->on_tick(price, volume, timestamp);

    // Live timeframe: evaluate entry/exit on every tick against the live price.
    if (live_mode_)
        evaluate_live(price);

    // Always push a real-time snapshot to the Dashboard (throttled inside).
    emit_live_snapshot(price, QString());
    last_tick_price_ = price;
}

QVector<OhlcvCandle> DeploymentRunner::live_eval_window(double price) const {
    QVector<OhlcvCandle> candles = aggregator_->closed_candles();
    const int64_t now = QDateTime::currentMSecsSinceEpoch();
    // Append previous + current tick as the last two bars so a crossover is detected
    // tick-to-tick against the live price (prev < target ≤ current).
    if (last_tick_price_ > 0) {
        OhlcvCandle prev;
        prev.open = prev.high = prev.low = prev.close = last_tick_price_;
        prev.open_time = prev.close_time = now - 1;
        prev.volume = 0;
        prev.is_closed = true;
        candles.append(prev);
    }
    OhlcvCandle cur;
    cur.open = cur.high = cur.low = cur.close = price;
    cur.open_time = cur.close_time = now;
    cur.volume = 0;
    cur.is_closed = true;
    candles.append(cur);
    return candles;
}

void DeploymentRunner::evaluate_live(double price) {
    if (position_mgr_->is_paused())
        return;
    auto candles = live_eval_window(price);
    if (candles.size() < 2)
        return;
    if (position_mgr_->has_position())
        evaluate_exit(candles);
    else
        evaluate_entry(candles);
}

namespace {
QString pretty_op(const QString& op) {
    if (op == "crosses_above") return QStringLiteral("crosses above");
    if (op == "crosses_below") return QStringLiteral("crosses below");
    if (op == "rising")        return QStringLiteral("rising");
    if (op == "falling")       return QStringLiteral("falling");
    if (op == "between")       return QStringLiteral("between");
    return op; // >, <, >=, <=, ==
}
} // namespace

void DeploymentRunner::emit_live_snapshot(double price, const QString& note) {
    const int64_t now = QDateTime::currentMSecsSinceEpoch();
    // Throttle the periodic stream to ~1/s; event notes (fills, signals) bypass it.
    if (note.isEmpty() && now - last_emit_ms_ < 1000)
        return;
    last_emit_ms_ = now;

    const auto candles = live_eval_window(price);

    AlgoLiveSnapshot snap;
    snap.deployment_id = deployment_.id;
    snap.current_price = price;
    snap.last_update_ms = now;
    auto m = position_mgr_->metrics();
    m.current_price = price;
    snap.metrics = m;
    snap.note = note;

    auto add_rows = [&](const QJsonArray& conds, const QString& logic, const QString& section) {
        if (conds.isEmpty())
            return;
        const auto res = ConditionEvaluator::evaluate_group(conds, logic, candles);
        for (const auto& d : res.details) {
            AlgoConditionStatus cs;
            cs.section = section;
            cs.op = d.op;
            cs.computed = d.computed_value;
            cs.target = d.target_value;
            cs.met = d.met;
            const QString fld = (d.field.isEmpty() || d.field == "value") ? QString()
                                                                          : (QStringLiteral(".") + d.field);
            cs.label = QStringLiteral("%1%2 %3 %4")
                           .arg(d.indicator, fld, pretty_op(d.op))
                           .arg(d.target_value, 0, 'f', 2);
            snap.conditions.append(cs);
        }
    };
    add_rows(strategy_.entry_conditions, strategy_.entry_logic, QStringLiteral("entry"));
    add_rows(strategy_.exit_conditions, strategy_.exit_logic, QStringLiteral("exit"));

    emit live_update(deployment_.id, snap);
}

void DeploymentRunner::on_candle_closed(const OhlcvCandle& /*candle*/) {
    if (!running_.load() || paused_.load()) return;
    if (position_mgr_->is_paused()) return;
    if (live_mode_) return; // live mode evaluates per tick in on_tick_data

    auto candles = aggregator_->closed_candles();
    if (candles.size() < 20) return;

    if (position_mgr_->has_position()) {
        evaluate_exit(candles);
    } else {
        evaluate_entry(candles);
    }

    auto m = position_mgr_->metrics();
    m.last_signal_time = QDateTime::currentMSecsSinceEpoch();
    emit metrics_updated(deployment_.id, m);
}

void DeploymentRunner::evaluate_entry(const QVector<OhlcvCandle>& candles) {
    auto result = ConditionEvaluator::evaluate_group(
        strategy_.entry_conditions, strategy_.entry_logic, candles);

    if (!result.triggered) return;

    AlgoOrderSignal signal;
    signal.deployment_id = deployment_.id;
    signal.account_id = deployment_.broker_account_id;
    signal.symbol = deployment_.symbol;
    signal.exchange = deployment_.exchange;
    signal.product_type = deployment_.product_type;
    signal.side = deployment_.entry_side;
    signal.quantity = deployment_.quantity;
    signal.order_type = "MARKET";
    signal.price = candles.last().close; // fill reference for paper sim / P&L
    signal.reason = "entry_signal";

    if (!position_mgr_->validate_order_value(signal.quantity, candles.last().close)) {
        LOG_WARN("AlgoEngine", QString("Deployment %1: order value exceeds limit, skipping entry")
                 .arg(deployment_.id));
        return;
    }

    emit_order_signal(signal);
}

void DeploymentRunner::evaluate_exit(const QVector<OhlcvCandle>& candles) {
    auto result = ConditionEvaluator::evaluate_group(
        strategy_.exit_conditions, strategy_.exit_logic, candles);

    if (!result.triggered) return;

    auto pos = position_mgr_->position();
    AlgoOrderSignal signal;
    signal.deployment_id = deployment_.id;
    signal.account_id = deployment_.broker_account_id;
    signal.symbol = deployment_.symbol;
    signal.exchange = deployment_.exchange;
    signal.product_type = deployment_.product_type;
    signal.side = (pos.side == PositionSide::Long) ? "SELL" : "BUY";
    signal.quantity = pos.quantity;
    signal.order_type = "MARKET";
    signal.price = candles.last().close; // fill reference for paper sim / P&L
    signal.reason = "exit_signal";

    emit_order_signal(signal);
}

void DeploymentRunner::emit_order_signal(const AlgoOrderSignal& signal_in) {
    // Only one order in flight at a time. Position/risk state updates only when a
    // fill arrives, so without this guard a stop-loss re-fires on every tick (and
    // entry/exit on every candle) until the fill lands — flooding duplicate orders.
    if (!pending_orders_.isEmpty())
        return;

    // Stamp the deployment's mode so the engine routes paper→simulated-fill and
    // live→broker. This is the single safety gate that stops a PAPER deployment
    // from ever hitting a real broker, even though it carries a real account id
    // (the account is its data source).
    AlgoOrderSignal signal = signal_in;
    signal.mode = deployment_.mode;
    if (signal.price <= 0)
        signal.price = position_mgr_->metrics().current_price;

    PendingOrder pending;
    pending.signal = signal;
    pending.submitted_ms = QDateTime::currentMSecsSinceEpoch();
    pending_orders_.append(pending);

    emit order_requested(signal);
    emit_live_snapshot(signal.price, QStringLiteral("%1: %2 %3 @ MARKET")
                                         .arg(signal.reason, signal.side)
                                         .arg(signal.quantity, 0, 'f', 0));
    LOG_INFO("AlgoEngine", QString("Deployment %1: %2 %3 %4 @ MARKET (%5)")
             .arg(deployment_.id, signal.side,
                  QString::number(signal.quantity), signal.symbol, signal.reason));
}

void DeploymentRunner::on_order_filled(const QString& broker_order_id,
                                        double fill_price, double fill_qty) {
    int idx = -1;
    for (int i = 0; i < pending_orders_.size(); ++i) {
        if (pending_orders_[i].broker_order_id == broker_order_id ||
            pending_orders_[i].broker_order_id.isEmpty()) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return;

    auto pending = pending_orders_.takeAt(idx);
    int64_t now = QDateTime::currentMSecsSinceEpoch();

    bool is_entry = (pending.signal.reason == "entry_signal");
    double pnl = 0;

    if (is_entry) {
        PositionSide side = (pending.signal.side == "BUY") ? PositionSide::Long : PositionSide::Short;
        position_mgr_->record_entry(side, fill_qty, fill_price, now);
    } else {
        pnl = position_mgr_->record_exit(fill_qty, fill_price, now);
    }

    AlgoTradeRecord trade;
    trade.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    trade.deployment_id = deployment_.id;
    trade.symbol = deployment_.symbol;
    trade.side = pending.signal.side;
    trade.quantity = fill_qty;
    trade.price = fill_price;
    trade.pnl = pnl;
    trade.reason = pending.signal.reason;
    trade.timestamp = now;
    trade.broker_order_id = broker_order_id;
    trade.latency_ms = now - pending.submitted_ms;

    persist_trade(trade);
    emit trade_executed(trade);
    emit metrics_updated(deployment_.id, position_mgr_->metrics());
    emit_live_snapshot(fill_price, QStringLiteral("%1 FILLED %2 @ %3")
                                       .arg(is_entry ? QStringLiteral("ENTRY") : QStringLiteral("EXIT"))
                                       .arg(fill_qty, 0, 'f', 0)
                                       .arg(fill_price, 0, 'f', 2));
}

void DeploymentRunner::on_order_rejected(const QString& /*broker_order_id*/,
                                          const QString& reason) {
    if (!pending_orders_.isEmpty())
        pending_orders_.removeFirst();
    LOG_ERROR("AlgoEngine", QString("Deployment %1: order rejected: %2")
              .arg(deployment_.id, reason));
}

void DeploymentRunner::on_heartbeat() {
    if (!running_.load()) return;
    int64_t now = QDateTime::currentMSecsSinceEpoch();

    // Keep the DB metrics row fresh (~5s) so the summary cards and any structural
    // rebuild show current values instead of stale zeros.
    persist_metrics();

    // Purge orders that never reported a fill/rejection so a lost ack doesn't
    // permanently block new signals (the in-flight guard in emit_order_signal).
    for (int i = pending_orders_.size() - 1; i >= 0; --i) {
        if (now - pending_orders_[i].submitted_ms > 60000) {
            LOG_WARN("AlgoEngine",
                     QString("Deployment %1: pending order timed out (no fill/reject in 60s), clearing")
                         .arg(deployment_.id));
            pending_orders_.removeAt(i);
        }
    }

    if (now - last_heartbeat_ms_ > 30000) {
        const QString msg =
            deployment_.broker_id.isEmpty()
                ? QStringLiteral("No market data in 30s — no broker is attached to this deployment.")
                : QString("No market data from broker '%1' in 30s — check the broker is connected "
                          "and the market is open.")
                      .arg(deployment_.broker_id);
        LOG_ERROR("AlgoEngine", QString("Deployment %1: %2").arg(deployment_.id, msg));

        auto db = Database::instance().connection();
        QSqlQuery q(db);
        q.prepare(QStringLiteral("UPDATE algo_deployments SET status='error', error_message=?, "
                                 "updated_at=datetime('now') WHERE id=?"));
        q.addBindValue(msg);
        q.addBindValue(deployment_.id);
        q.exec();

        emit status_changed(deployment_.id, QStringLiteral("error"));
        emit error_occurred(deployment_.id, msg);
    }
}

void DeploymentRunner::persist_trade(const AlgoTradeRecord& trade) {
    auto db = Database::instance().connection();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO algo_trades (id, deployment_id, symbol, side, quantity, price, pnl, "
        "reason, broker_order_id, latency_ms, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'))"));
    q.addBindValue(trade.id);
    q.addBindValue(trade.deployment_id);
    q.addBindValue(trade.symbol);
    q.addBindValue(trade.side);
    q.addBindValue(trade.quantity);
    q.addBindValue(trade.price);
    q.addBindValue(trade.pnl);
    q.addBindValue(trade.reason);
    q.addBindValue(trade.broker_order_id);
    q.addBindValue(trade.latency_ms);
    if (!q.exec())
        LOG_ERROR("AlgoEngine", QString("Failed to persist trade: %1").arg(q.lastError().text()));
}

void DeploymentRunner::persist_metrics() {
    auto m = position_mgr_->metrics();
    auto db = Database::instance().connection();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO algo_metrics (deployment_id, total_pnl, unrealized_pnl, "
        "total_trades, win_rate, max_drawdown, current_position_qty, current_position_side, "
        "current_position_entry, current_price, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'))"));
    q.addBindValue(deployment_.id);
    q.addBindValue(m.total_pnl);
    q.addBindValue(m.unrealized_pnl);
    q.addBindValue(m.total_trades);
    q.addBindValue(m.win_rate);
    q.addBindValue(m.max_drawdown);
    q.addBindValue(m.current_position_qty);
    q.addBindValue(m.current_position_side);
    q.addBindValue(m.current_position_entry);
    q.addBindValue(m.current_price);
    if (!q.exec())
        LOG_ERROR("AlgoEngine", QString("Failed to persist metrics: %1").arg(q.lastError().text()));
}

void DeploymentRunner::restore_state_from_db() {
    auto db = Database::instance().connection();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT total_pnl, total_trades, win_rate, max_drawdown, current_position_qty, "
        "current_position_side, current_position_entry FROM algo_metrics WHERE deployment_id = ?"));
    q.addBindValue(deployment_.id);
    if (!q.exec() || !q.next())
        return; // fresh deploy — no prior state

    const QString side_s = q.value("current_position_side").toString();
    const PositionSide side = (side_s == "LONG")    ? PositionSide::Long
                              : (side_s == "SHORT") ? PositionSide::Short
                                                    : PositionSide::None;
    const double qty = q.value("current_position_qty").toDouble();
    const double entry = q.value("current_position_entry").toDouble();

    position_mgr_->restore_state(side, qty, entry, q.value("total_pnl").toDouble(),
                                 q.value("total_trades").toInt(), q.value("win_rate").toDouble(),
                                 q.value("max_drawdown").toDouble());

    if (side != PositionSide::None)
        LOG_INFO("AlgoEngine", QString("Deployment %1: restored open %2 %3 @ %4 across restart")
                                   .arg(deployment_.id, side_s)
                                   .arg(qty, 0, 'f', 0)
                                   .arg(entry, 0, 'f', 2));
}

void DeploymentRunner::update_deployment_status(const QString& status) {
    auto db = Database::instance().connection();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE algo_deployments SET status = ?, updated_at = datetime('now') WHERE id = ?"));
    q.addBindValue(status);
    q.addBindValue(deployment_.id);
    q.exec();
}

AlgoMetrics DeploymentRunner::metrics() const {
    return position_mgr_->metrics();
}

AlgoPosition DeploymentRunner::position() const {
    return position_mgr_->position();
}

} // namespace fincept::algo
