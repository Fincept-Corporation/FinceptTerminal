// src/algo_engine/DeploymentRunner.cpp
#include "algo_engine/DeploymentRunner.h"
#include "algo_engine/CandleDataFetcher.h"
#include "algo_engine/ConditionEvaluator.h"
#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "storage/sqlite/Database.h"

using fincept::Database;
#include "trading/BrokerTopic.h"
#include "trading/TradingTypes.h"

#include <QDateTime>
#include <QPointer>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

namespace fincept::algo {

DeploymentRunner::DeploymentRunner(const services::algo::AlgoDeployment& deployment,
                                   const services::algo::AlgoStrategy& strategy,
                                   QObject* parent)
    : QObject(parent)
    , deployment_(deployment)
    , strategy_(strategy)
    , timeframe_(timeframe_from_string(deployment.timeframe.isEmpty() ? strategy.timeframe : deployment.timeframe)) {

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
            self->subscribe_tick_topic();
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
    unsubscribe_tick_topic();
    persist_metrics();
    update_deployment_status(QStringLiteral("stopped"));
    emit status_changed(deployment_.id, QStringLiteral("stopped"));
    LOG_INFO("AlgoEngine", QString("Deployment %1 stopped").arg(deployment_.id));
}

void DeploymentRunner::subscribe_tick_topic() {
    const QString topic = fincept::trading::broker_topic(
        deployment_.broker_id, deployment_.broker_account_id,
        QStringLiteral("ticks"), deployment_.symbol);

    auto& hub = fincept::datahub::DataHub::instance();
    tick_connection_ = hub.subscribe(this, topic,
        [this](const QVariant& v) { on_tick_data(v); });
}

void DeploymentRunner::unsubscribe_tick_topic() {
    if (tick_connection_) {
        QObject::disconnect(tick_connection_);
        tick_connection_ = {};
    }
    fincept::datahub::DataHub::instance().unsubscribe(this);
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

    position_mgr_->update_price(price);

    auto risk_signal = position_mgr_->check_risk(price);
    if (risk_signal) {
        risk_signal->account_id = deployment_.broker_account_id;
        risk_signal->symbol = deployment_.symbol;
        risk_signal->exchange = deployment_.exchange;
        risk_signal->product_type = deployment_.product_type;
        emit_order_signal(*risk_signal);
    }

    aggregator_->on_tick(price, volume, timestamp);
}

void DeploymentRunner::on_candle_closed(const OhlcvCandle& /*candle*/) {
    if (!running_.load() || paused_.load()) return;
    if (position_mgr_->is_paused()) return;

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
    signal.reason = "exit_signal";

    emit_order_signal(signal);
}

void DeploymentRunner::emit_order_signal(const AlgoOrderSignal& signal) {
    // Only one order in flight at a time. Position/risk state updates only when a
    // fill arrives, so without this guard a stop-loss re-fires on every tick (and
    // entry/exit on every candle) until the fill lands — flooding duplicate orders.
    if (!pending_orders_.isEmpty())
        return;

    PendingOrder pending;
    pending.signal = signal;
    pending.submitted_ms = QDateTime::currentMSecsSinceEpoch();
    pending_orders_.append(pending);

    emit order_requested(signal);
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
        LOG_ERROR("AlgoEngine", QString("Deployment %1: no tick data for 30s, marking error")
                  .arg(deployment_.id));
        update_deployment_status(QStringLiteral("error"));
        emit status_changed(deployment_.id, QStringLiteral("error"));
        emit error_occurred(deployment_.id, QStringLiteral("No tick data received for 30 seconds"));
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
    q.addBindValue(QVariant::fromValue(trade.latency_ms));
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
