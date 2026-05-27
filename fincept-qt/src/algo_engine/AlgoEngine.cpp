// src/algo_engine/AlgoEngine.cpp
#include "algo_engine/AlgoEngine.h"
#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "storage/sqlite/Database.h"
#include "trading/UnifiedTrading.h"

#include <QDateTime>
#include <QMetaObject>
#include <QPointer>
#include <QSqlQuery>
#include <QtConcurrent>

namespace fincept::algo {

AlgoEngine& AlgoEngine::instance() {
    static AlgoEngine s;
    return s;
}

AlgoEngine::AlgoEngine() {
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

    QString account_id = signal.account_id;
    double submitted_price = signal.price;

    QtConcurrent::run([self, dep_id, account_id, order, submitted_price]() {
        auto response = fincept::trading::UnifiedTrading::instance()
                            .place_order(account_id, order);

        if (!self) return;
        QMetaObject::invokeMethod(self, [self, dep_id, response, order]() {
            if (!self) return;
            QMutexLocker lock(&self->mutex_);
            auto* runner = self->runners_.value(dep_id, nullptr);
            if (!runner) return;
            lock.unlock();

            if (response.success) {
                runner->on_order_filled(response.order_id, order.quantity > 0 ? order.price : 0, order.quantity);
            } else {
                runner->on_order_rejected(response.order_id, response.message);
            }
        }, Qt::QueuedConnection);
    });
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
    auto db = fincept::Database::instance().connection();
    QSqlQuery q(db);
    q.exec(QStringLiteral(
        "SELECT id FROM algo_deployments WHERE status IN ('running', 'starting')"));
    while (q.next()) {
        QString id = q.value(0).toString();
        QSqlQuery u(db);
        u.prepare(QStringLiteral(
            "UPDATE algo_deployments SET status = 'crashed', "
            "error_message = 'Application restarted' WHERE id = ?"));
        u.addBindValue(id);
        u.exec();

        datahub::DataHub::instance().publish(
            QStringLiteral("algo:state:") + id, QVariant::fromValue(QStringLiteral("crashed")));
        emit deployment_crashed(id, QStringLiteral("Application restarted unexpectedly"));
        LOG_WARN("AlgoEngine", QString("Marked orphaned deployment %1 as crashed").arg(id));
    }
}

} // namespace fincept::algo
