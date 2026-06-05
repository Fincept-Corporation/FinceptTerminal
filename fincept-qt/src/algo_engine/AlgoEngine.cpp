// src/algo_engine/AlgoEngine.cpp
#include "algo_engine/AlgoEngine.h"
#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "storage/sqlite/Database.h"
#include "trading/UnifiedTrading.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaObject>
#include <QPointer>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QtConcurrent>

namespace fincept::algo {

AlgoEngine& AlgoEngine::instance() {
    static AlgoEngine s;
    return s;
}

AlgoEngine::AlgoEngine() {
    // Register types crossing the engine-thread → UI-thread queued signals.
    qRegisterMetaType<fincept::algo::AlgoLiveSnapshot>("fincept::algo::AlgoLiveSnapshot");
    qRegisterMetaType<fincept::algo::AlgoMetrics>("fincept::algo::AlgoMetrics");
    qRegisterMetaType<fincept::algo::AlgoTradeRecord>("fincept::algo::AlgoTradeRecord");

    engine_thread_.setObjectName(QStringLiteral("AlgoEngineThread"));
    engine_thread_.start();
    moveToThread(&engine_thread_);
}

AlgoEngine::~AlgoEngine() {
    stop_all();
    engine_thread_.quit();
    engine_thread_.wait(5000);
}

void AlgoEngine::start_deployment(const services::algo::AlgoDeployment& deployment,
                                   const services::algo::AlgoStrategy& strategy) {
    QMutexLocker lock(&mutex_);
    if (runners_.contains(deployment.id)) {
        emit error_occurred(deployment.id, QStringLiteral("Deployment already running"));
        return;
    }

    auto* runner = new DeploymentRunner(deployment, strategy);
    runner->moveToThread(&engine_thread_);

    connect(runner, &DeploymentRunner::trade_executed, this, &AlgoEngine::trade_executed);
    connect(runner, &DeploymentRunner::metrics_updated, this, &AlgoEngine::metrics_updated);
    connect(runner, &DeploymentRunner::error_occurred, this, &AlgoEngine::error_occurred);
    connect(runner, &DeploymentRunner::live_update, this, &AlgoEngine::live_update);
    connect(runner, &DeploymentRunner::order_requested, this, &AlgoEngine::on_order_requested);
    connect(runner, &DeploymentRunner::status_changed, this, [this](const QString& id, const QString& status) {
        if (status == "stopped" || status == "error") {
            QMutexLocker lock(&mutex_);
            if (auto* r = runners_.take(id)) {
                r->deleteLater();
                emit deployment_stopped(id);
            }
        }
        auto& hub = datahub::DataHub::instance();
        hub.publish(QStringLiteral("algo:state:") + id, QVariant::fromValue(status));
    });

    // Publish metrics to DataHub on update
    connect(runner, &DeploymentRunner::metrics_updated, this,
        [](const QString& dep_id, const AlgoMetrics& m) {
            datahub::DataHub::instance().publish(
                QStringLiteral("algo:metrics:") + dep_id, QVariant::fromValue(m));
        });
    connect(runner, &DeploymentRunner::trade_executed, this,
        [](const AlgoTradeRecord& trade) {
            datahub::DataHub::instance().publish(
                QStringLiteral("algo:trade:") + trade.deployment_id,
                QVariant::fromValue(trade));
        });

    runners_.insert(deployment.id, runner);
    lock.unlock();

    // Persist the row BEFORE the runner's queued start() (which UPDATEs status) and
    // before the UI refreshes the Dashboard — both read/expect this row to exist.
    persist_deployment(deployment);

    QMetaObject::invokeMethod(runner, &DeploymentRunner::start, Qt::QueuedConnection);
    emit deployment_started(deployment.id);
    LOG_INFO("AlgoEngine", QString("Started deployment %1 for strategy '%2' on %3")
             .arg(deployment.id, strategy.name, deployment.symbol));
}

void AlgoEngine::stop_deployment(const QString& deployment_id) {
    QMutexLocker lock(&mutex_);
    auto* runner = runners_.value(deployment_id, nullptr);
    if (!runner) return;
    lock.unlock();
    QMetaObject::invokeMethod(runner, &DeploymentRunner::stop, Qt::QueuedConnection);
}

void AlgoEngine::pause_deployment(const QString& deployment_id) {
    QMutexLocker lock(&mutex_);
    auto* runner = runners_.value(deployment_id, nullptr);
    if (!runner) return;
    lock.unlock();
    QMetaObject::invokeMethod(runner, &DeploymentRunner::pause, Qt::QueuedConnection);
}

void AlgoEngine::resume_deployment(const QString& deployment_id) {
    QMutexLocker lock(&mutex_);
    auto* runner = runners_.value(deployment_id, nullptr);
    if (!runner) return;
    lock.unlock();
    QMetaObject::invokeMethod(runner, &DeploymentRunner::resume, Qt::QueuedConnection);
}

void AlgoEngine::stop_all() {
    QMutexLocker lock(&mutex_);
    auto ids = runners_.keys();
    lock.unlock();
    for (const auto& id : ids)
        stop_deployment(id);
}

bool AlgoEngine::is_running(const QString& deployment_id) const {
    QMutexLocker lock(&mutex_);
    auto* runner = runners_.value(deployment_id, nullptr);
    return runner && runner->is_running();
}

QStringList AlgoEngine::active_deployment_ids() const {
    QMutexLocker lock(&mutex_);
    return runners_.keys();
}

AlgoMetrics AlgoEngine::metrics(const QString& deployment_id) const {
    QMutexLocker lock(&mutex_);
    auto* runner = runners_.value(deployment_id, nullptr);
    return runner ? runner->metrics() : AlgoMetrics{};
}

int AlgoEngine::active_count() const {
    QMutexLocker lock(&mutex_);
    return runners_.size();
}

void AlgoEngine::on_order_requested(const AlgoOrderSignal& signal) {
    execute_order(signal);
}

void AlgoEngine::execute_order(const AlgoOrderSignal& signal) {
    QPointer<AlgoEngine> self = this;
    QString dep_id = signal.deployment_id;
    const double submitted_price = signal.price;
    const double qty = signal.quantity;

    // ── Paper: never touch a real broker ────────────────────────────────────────
    // Simulate an immediate fill at the signal's reference price. This is the hard
    // safety gate: even though a paper deployment carries a real broker account (its
    // data source), its orders are never sent to that broker.
    if (signal.mode != QStringLiteral("live")) {
        const QString sim_id = QStringLiteral("paper-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
        QMetaObject::invokeMethod(this, [self, dep_id, sim_id, submitted_price, qty]() {
            if (!self) return;
            QMutexLocker lock(&self->mutex_);
            auto* runner = self->runners_.value(dep_id, nullptr);
            if (!runner) return;
            lock.unlock();
            runner->on_order_filled(sim_id, submitted_price, qty);
        }, Qt::QueuedConnection);
        LOG_INFO("AlgoEngine", QString("Deployment %1: PAPER simulated fill %2 %3 @ %4")
                                   .arg(dep_id, signal.side).arg(qty).arg(submitted_price));
        return;
    }

    fincept::trading::UnifiedOrder order;
    order.symbol = signal.symbol;
    order.exchange = signal.exchange;
    order.side = (signal.side == "BUY") ? trading::OrderSide::Buy : trading::OrderSide::Sell;
    order.order_type = trading::OrderType::Market;
    order.quantity = signal.quantity;
    if (signal.product_type == "CNC" || signal.product_type == "delivery")
        order.product_type = trading::ProductType::Delivery;
    else if (signal.product_type == "NRML" || signal.product_type == "margin")
        order.product_type = trading::ProductType::Margin;
    else
        order.product_type = trading::ProductType::Intraday;

    const QString account_id = signal.account_id;

    QtConcurrent::run([self, dep_id, account_id, order, submitted_price]() {
        auto response = fincept::trading::UnifiedTrading::instance()
                            .place_order(account_id, order);

        if (!self) return;
        QMetaObject::invokeMethod(self, [self, dep_id, response, order, submitted_price]() {
            if (!self) return;
            QMutexLocker lock(&self->mutex_);
            auto* runner = self->runners_.value(dep_id, nullptr);
            if (!runner) return;
            lock.unlock();

            if (response.success) {
                // Use the signal's reference price as the fill price (market orders
                // carry no price; the broker's avg-fill isn't returned synchronously).
                runner->on_order_filled(response.order_id, submitted_price, order.quantity);
            } else {
                runner->on_order_rejected(response.order_id, response.message);
            }
        }, Qt::QueuedConnection);
    });
}

void AlgoEngine::persist_deployment(const services::algo::AlgoDeployment& d) {
    auto db = fincept::Database::instance().connection();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO algo_deployments "
        "(id, strategy_id, strategy_name, strategy_kind, symbol, exchange, product_type, "
        " mode, entry_side, backend, broker_id, broker_account_id, paper_portfolio_id, "
        " timeframe, quantity, max_order_value, max_daily_loss, status, created_at, updated_at) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?, datetime('now'), datetime('now'))"));
    q.addBindValue(d.id);
    q.addBindValue(d.strategy_id);
    q.addBindValue(d.strategy_name);
    q.addBindValue(d.strategy_kind);
    q.addBindValue(d.symbol);
    q.addBindValue(d.exchange);
    q.addBindValue(d.product_type);
    q.addBindValue(d.mode);
    q.addBindValue(d.entry_side);
    q.addBindValue(d.backend);
    q.addBindValue(d.broker_id);
    q.addBindValue(d.broker_account_id);
    q.addBindValue(d.paper_portfolio_id);
    q.addBindValue(d.timeframe);
    q.addBindValue(d.quantity);
    q.addBindValue(d.max_order_value);
    q.addBindValue(d.max_daily_loss);
    q.addBindValue(QStringLiteral("starting"));
    if (!q.exec())
        LOG_ERROR("AlgoEngine",
                  QString("Failed to persist deployment %1: %2").arg(d.id, q.lastError().text()));
    else
        LOG_INFO("AlgoEngine", QString("Persisted deployment row %1 (%2/%3 on %4)")
                                   .arg(d.id, d.mode, d.backend, d.symbol));
}

void AlgoEngine::list_deployments() {
    QPointer<AlgoEngine> self = this;
    QtConcurrent::run([self]() {
        auto db = fincept::Database::instance().connection();
        QSqlQuery q(db);
        q.exec(QStringLiteral("SELECT * FROM algo_deployments ORDER BY created_at DESC"));

        QVector<services::algo::AlgoDeployment> deployments;
        while (q.next()) {
            services::algo::AlgoDeployment d;
            d.id = q.value("id").toString();
            d.strategy_id = q.value("strategy_id").toString();
            d.strategy_name = q.value("strategy_name").toString();
            d.symbol = q.value("symbol").toString();
            d.exchange = q.value("exchange").toString();
            d.product_type = q.value("product_type").toString();
            d.mode = q.value("mode").toString();
            d.entry_side = q.value("entry_side").toString();
            d.backend = q.value("backend").toString();
            d.broker_id = q.value("broker_id").toString();
            d.broker_account_id = q.value("broker_account_id").toString();
            d.status = q.value("status").toString();
            d.error_message = q.value("error_message").toString();
            d.timeframe = q.value("timeframe").toString();
            d.quantity = q.value("quantity").toDouble();
            d.max_order_value = q.value("max_order_value").toDouble();
            d.max_daily_loss = q.value("max_daily_loss").toDouble();
            d.created_at = q.value("created_at").toString();
            deployments.append(d);
        }

        // Load live metrics for running deployments
        QSqlQuery mq(db);
        mq.exec(QStringLiteral("SELECT * FROM algo_metrics"));
        QHash<QString, int> metrics_idx;
        while (mq.next()) {
            QString dep_id = mq.value("deployment_id").toString();
            for (int i = 0; i < deployments.size(); ++i) {
                if (deployments[i].id == dep_id) {
                    deployments[i].total_pnl = mq.value("total_pnl").toDouble();
                    deployments[i].unrealized_pnl = mq.value("unrealized_pnl").toDouble();
                    deployments[i].total_trades = mq.value("total_trades").toInt();
                    deployments[i].win_rate = mq.value("win_rate").toDouble();
                    deployments[i].max_drawdown = mq.value("max_drawdown").toDouble();
                    deployments[i].current_price = mq.value("current_price").toDouble();
                    deployments[i].position_qty = mq.value("current_position_qty").toDouble();
                    deployments[i].position_side = mq.value("current_position_side").toString();
                    deployments[i].position_entry = mq.value("current_position_entry").toDouble();
                    break;
                }
            }
        }

        if (!self) return;
        QMetaObject::invokeMethod(self, [self, deployments]() {
            if (!self) return;
            emit self->deployments_loaded(deployments);
        }, Qt::QueuedConnection);
    });
}

void AlgoEngine::recover_orphaned() {
    // On restart, deployments that were active (or stuck in error from the prior
    // no-data bug) should resume rather than silently die — that's what users
    // expect of a deployed algo. Reload each one + its strategy and restart it.
    auto db = fincept::Database::instance().connection();
    QSqlQuery q(db);
    q.exec(QStringLiteral(
        "SELECT * FROM algo_deployments WHERE status IN ('running','starting','error','crashed')"));

    QVector<services::algo::AlgoDeployment> to_resume;
    while (q.next()) {
        services::algo::AlgoDeployment d;
        d.id = q.value("id").toString();
        d.strategy_id = q.value("strategy_id").toString();
        d.strategy_name = q.value("strategy_name").toString();
        d.strategy_kind = q.value("strategy_kind").toString();
        d.symbol = q.value("symbol").toString();
        d.exchange = q.value("exchange").toString();
        d.product_type = q.value("product_type").toString();
        d.mode = q.value("mode").toString();
        d.entry_side = q.value("entry_side").toString();
        d.backend = q.value("backend").toString();
        d.broker_id = q.value("broker_id").toString();
        d.broker_account_id = q.value("broker_account_id").toString();
        d.paper_portfolio_id = q.value("paper_portfolio_id").toString();
        d.timeframe = q.value("timeframe").toString();
        d.quantity = q.value("quantity").toDouble();
        d.max_order_value = q.value("max_order_value").toDouble();
        d.max_daily_loss = q.value("max_daily_loss").toDouble();
        to_resume.append(d);
    }

    for (const auto& d : to_resume) {
        auto strat = load_strategy(d.strategy_id);
        if (strat.id.isEmpty()) {
            QSqlQuery u(db);
            u.prepare(QStringLiteral("UPDATE algo_deployments SET status='stopped', "
                                     "error_message='Strategy not found on resume' WHERE id=?"));
            u.addBindValue(d.id);
            u.exec();
            LOG_WARN("AlgoEngine",
                     QString("Cannot resume deployment %1: strategy %2 missing").arg(d.id, d.strategy_id));
            continue;
        }
        LOG_INFO("AlgoEngine", QString("Resuming deployment %1 ('%2' on %3) after restart")
                                   .arg(d.id, strat.name, d.symbol));
        start_deployment(d, strat);
    }
}

bool AlgoEngine::has_active_duplicate(const QString& strategy_id, const QString& symbol,
                                     const QString& mode, const QString& entry_side) const {
    auto db = fincept::Database::instance().connection();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM algo_deployments WHERE strategy_id=? AND symbol=? AND mode=? "
        "AND entry_side=? AND status IN ('running','starting')"));
    q.addBindValue(strategy_id);
    q.addBindValue(symbol);
    q.addBindValue(mode);
    q.addBindValue(entry_side);
    if (q.exec() && q.next())
        return q.value(0).toInt() > 0;
    return false;
}

void AlgoEngine::remove_deployment(const QString& deployment_id) {
    // Stop the runner first if it happens to be live (REMOVE is normally only shown
    // for already-stopped rows, but guard anyway).
    {
        QMutexLocker lock(&mutex_);
        if (auto* r = runners_.value(deployment_id, nullptr)) {
            lock.unlock();
            QMetaObject::invokeMethod(r, &DeploymentRunner::stop, Qt::QueuedConnection);
        }
    }

    auto db = fincept::Database::instance().connection();
    const char* const stmts[] = {
        "DELETE FROM algo_deployments WHERE id = ?",
        "DELETE FROM algo_metrics WHERE deployment_id = ?",
        "DELETE FROM algo_deployment_heartbeat WHERE deployment_id = ?",
        "DELETE FROM algo_order_signals WHERE deployment_id = ?",
        "DELETE FROM algo_trades WHERE deployment_id = ?",
    };
    for (const char* sql : stmts) {
        QSqlQuery q(db);
        q.prepare(QString::fromLatin1(sql));
        q.addBindValue(deployment_id);
        if (!q.exec())
            LOG_ERROR("AlgoEngine",
                      QString("remove_deployment(%1): %2").arg(deployment_id, q.lastError().text()));
    }
    LOG_INFO("AlgoEngine", QString("Removed deployment %1").arg(deployment_id));
    list_deployments(); // refresh the Dashboard
}

services::algo::AlgoStrategy AlgoEngine::load_strategy(const QString& strategy_id) {
    services::algo::AlgoStrategy s;
    auto db = fincept::Database::instance().connection();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT * FROM algo_strategies WHERE id = ?"));
    q.addBindValue(strategy_id);
    if (q.exec() && q.next()) {
        s.id = q.value("id").toString();
        s.name = q.value("name").toString();
        s.description = q.value("description").toString();
        s.timeframe = q.value("timeframe").toString();
        s.entry_conditions =
            QJsonDocument::fromJson(q.value("entry_conditions").toString().toUtf8()).array();
        s.exit_conditions =
            QJsonDocument::fromJson(q.value("exit_conditions").toString().toUtf8()).array();
        s.entry_logic = q.value("entry_logic").toString();
        s.exit_logic = q.value("exit_logic").toString();
        s.stop_loss = q.value("stop_loss").toDouble();
        s.take_profit = q.value("take_profit").toDouble();
        s.trailing_stop = q.value("trailing_stop").toDouble();
    }
    return s;
}

} // namespace fincept::algo
