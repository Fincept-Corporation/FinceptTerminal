// src/algo_engine/AlgoEngine.cpp
#include "algo_engine/AlgoEngine.h"
#include "algo_engine/fno/FnoExecution.h"
#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "storage/sqlite/Database.h"
#include "trading/AccountManager.h"
#include "trading/PaperTrading.h"
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

    // Create the FnoDataBridge BEFORE moveToThread so it stays on the main thread.
    // Do NOT parent it to `this` — `this` moves to the engine thread, which would
    // carry fno_bridge_ along. Ownership: deleted in dtor.
    fno_bridge_ = new fincept::algo::fno::FnoDataBridge();

    engine_thread_.setObjectName(QStringLiteral("AlgoEngineThread"));
    engine_thread_.start();
    moveToThread(&engine_thread_);
}

AlgoEngine::~AlgoEngine() {
    stop_all();
    engine_thread_.quit();
    engine_thread_.wait(5000);
    delete fno_bridge_;
    fno_bridge_ = nullptr;
}

void AlgoEngine::start_deployment(const services::algo::AlgoDeployment& deployment,
                                   const services::algo::AlgoStrategy& strategy) {
    QMutexLocker lock(&mutex_);
    if (runners_.contains(deployment.id)) {
        emit error_occurred(deployment.id, QStringLiteral("Deployment already running"));
        return;
    }

    auto* runner = new DeploymentRunner(deployment, strategy);
    runner->set_fno_bridge(fno_bridge_);
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
    // Multi-leg F&O basket → dedicated path (paper portfolio / live broker basket).
    if (!signal.legs.isEmpty()) {
        execute_basket(signal);
        return;
    }

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

    (void)QtConcurrent::run([self, dep_id, account_id, order, submitted_price]() {
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

QString AlgoEngine::resolve_paper_portfolio_id(const AlgoOrderSignal& signal) {
    // 1) Explicit portfolio stamped on the signal from the deployment.
    if (!signal.paper_portfolio_id.isEmpty())
        return signal.paper_portfolio_id;

    // 2) The deployment account's linked paper portfolio.
    if (!signal.account_id.isEmpty()) {
        const auto acct = fincept::trading::AccountManager::instance().get_account(signal.account_id);
        if (!acct.paper_portfolio_id.isEmpty())
            return acct.paper_portfolio_id;
    }

    // 3) Reuse or create a shared "F&O Paper" portfolio (mirrors the F&O Builder).
    if (auto existing = fincept::trading::pt_find_portfolio(QStringLiteral("F&O Paper"),
                                                            QStringLiteral("NFO")))
        return existing->id;
    const auto created = fincept::trading::pt_create_portfolio(
        QStringLiteral("F&O Paper"), 1000000.0, QStringLiteral("INR"),
        1.0, QStringLiteral("cross"), 0.001, QStringLiteral("NFO"));
    LOG_INFO("AlgoEngine", QString("Created F&O Paper portfolio %1 for basket execution")
                               .arg(created.id));
    return created.id;
}

void AlgoEngine::execute_basket(const AlgoOrderSignal& signal) {
    QPointer<AlgoEngine> self = this;
    const QString dep_id = signal.deployment_id;
    const QVector<AlgoOrderLeg> legs = signal.legs;

    // ── Paper: route every leg to the paper portfolio, NEVER the broker ──────────
    // Hard safety gate: a paper deployment carries a real broker account as its
    // data source, but its basket is simulated against pt_place_order. We never
    // call place_basket_orders here (it routes by ACCOUNT mode and could fire real
    // orders on a live data account).
    if (signal.mode != QStringLiteral("live")) {
        const QString portfolio_id = resolve_paper_portfolio_id(signal);
        if (portfolio_id.isEmpty()) {
            LOG_ERROR("AlgoEngine", QString("Deployment %1: PAPER basket: no portfolio").arg(dep_id));
            return;
        }
        QMetaObject::invokeMethod(this, [self, dep_id, legs, portfolio_id]() {
            if (!self) return;
            QMutexLocker lock(&self->mutex_);
            auto* runner = self->runners_.value(dep_id, nullptr);
            if (!runner) return;
            lock.unlock();

            // Place each leg; immediate fill at the leg's reference price. If any
            // leg fails, reverse the already-placed legs (reduce-only) and reject
            // the whole basket so the runner records no position (atomic entry).
            QVector<int> placed_ok;
            bool any_fail = false;
            for (int i = 0; i < legs.size(); ++i) {
                const auto& leg = legs[i];
                bool ok = true;
                try {
                    // pt_place_order inserts a pending order; a market order is
                    // filled explicitly at the leg's reference price (same as the
                    // Equity/F&O paper paths — there is no implicit market fill).
                    const auto po = fincept::trading::pt_place_order(
                        portfolio_id, leg.symbol, leg.side.toLower(),
                        QStringLiteral("market"), leg.quantity, leg.price,
                        std::nullopt, false, QStringLiteral("NFO"), QStringLiteral("NRML"));
                    fincept::trading::pt_fill_order(po.id, leg.price);
                } catch (const std::exception& e) {
                    ok = false;
                    any_fail = true;
                    LOG_ERROR("AlgoEngine", QString("Deployment %1: PAPER leg %2 (%3) failed: %4")
                                  .arg(dep_id).arg(i).arg(leg.symbol, QString::fromUtf8(e.what())));
                }
                if (ok) placed_ok.append(i);
            }

            if (!any_fail) {
                for (int i = 0; i < legs.size(); ++i)
                    runner->on_leg_filled(i, legs[i], legs[i].price, legs[i].quantity);
                return;
            }

            // Rollback the legs that did fill, then reject every leg.
            for (int i : std::as_const(placed_ok)) {
                const auto& leg = legs[i];
                const QString rev = (leg.side.toLower() == QStringLiteral("buy"))
                                        ? QStringLiteral("sell") : QStringLiteral("buy");
                try {
                    const auto ro = fincept::trading::pt_place_order(
                        portfolio_id, leg.symbol, rev, QStringLiteral("market"),
                        leg.quantity, leg.price, std::nullopt, true,
                        QStringLiteral("NFO"), QStringLiteral("NRML"));
                    fincept::trading::pt_fill_order(ro.id, leg.price);
                } catch (const std::exception& e) {
                    LOG_ERROR("AlgoEngine", QString("Deployment %1: PAPER rollback leg %2 failed: %3")
                                  .arg(dep_id).arg(i).arg(QString::fromUtf8(e.what())));
                }
            }
            for (int i = 0; i < legs.size(); ++i)
                runner->on_leg_rejected(i, QStringLiteral("paper basket aborted (partial fill)"));
        }, Qt::QueuedConnection);
        LOG_INFO("AlgoEngine", QString("Deployment %1: PAPER basket %2 legs -> portfolio %3")
                                   .arg(dep_id).arg(legs.size()).arg(portfolio_id));
        return;
    }

    // ── Live: broker basket (ONLY signal.mode == "live") ─────────────────────────
    const QString account_id = signal.account_id;
    const fincept::trading::BasketOrderRequest basket =
        fincept::algo::fno::build_basket_request(legs, signal.product_type);

    fincept::trading::UnifiedTrading::instance().place_basket_orders(
        account_id, basket,
        [self, dep_id, legs, account_id](const fincept::trading::BasketOrderResult& res) {
            if (!self) return;
            QMetaObject::invokeMethod(self, [self, dep_id, legs, account_id, res]() {
                if (!self) return;
                QMutexLocker lock(&self->mutex_);
                auto* runner = self->runners_.value(dep_id, nullptr);
                if (!runner) return;
                lock.unlock();

                // place_basket_orders reorders (BUY-first) and batches, so map each
                // leg to its result by symbol rather than by index.
                auto leg_succeeded = [&res](const QString& sym) -> bool {
                    for (const auto& r : res.results)
                        if (r.symbol == sym) return r.success;
                    return false;
                };

                QVector<int> filled_idx;
                bool any_fail = false;
                for (int i = 0; i < legs.size(); ++i) {
                    if (leg_succeeded(legs[i].symbol)) filled_idx.append(i);
                    else                                any_fail = true;
                }

                if (!any_fail) {
                    // Fills carry no price; mark at the leg's reference LTP (P3.5
                    // replaces this with live leg-quote marks).
                    for (int i = 0; i < legs.size(); ++i)
                        runner->on_leg_filled(i, legs[i], legs[i].price, legs[i].quantity);
                    return;
                }

                // Rollback: close the legs that filled, then reject the basket so no
                // position is recorded, and surface the error.
                if (!filled_idx.isEmpty()) {
                    QVector<AlgoOrderLeg> reverse;
                    for (int i : std::as_const(filled_idx)) {
                        AlgoOrderLeg rl = legs[i];
                        rl.side = (legs[i].side == QLatin1String("BUY"))
                                      ? QStringLiteral("SELL") : QStringLiteral("BUY");
                        reverse.append(rl);
                    }
                    const auto rb = fincept::algo::fno::build_basket_request(reverse, QString());
                    fincept::trading::UnifiedTrading::instance().place_basket_orders(
                        account_id, rb, [dep_id](const fincept::trading::BasketOrderResult&) {
                            LOG_WARN("AlgoEngine", QString("Deployment %1: entry basket rolled back")
                                         .arg(dep_id));
                        });
                }
                for (int i = 0; i < legs.size(); ++i)
                    runner->on_leg_rejected(i, QStringLiteral("live basket aborted (partial fill)"));
                emit self->error_occurred(dep_id, QStringLiteral("F&O entry basket partially failed; rolled back"));
            }, Qt::QueuedConnection);
        });
    LOG_INFO("AlgoEngine", QString("Deployment %1: LIVE basket %2 legs -> account %3")
                               .arg(dep_id).arg(legs.size()).arg(account_id));
}

void AlgoEngine::persist_deployment(const services::algo::AlgoDeployment& d) {
    auto db = fincept::Database::instance().connection();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO algo_deployments "
        "(id, strategy_id, strategy_name, strategy_kind, symbol, exchange, product_type, "
        " mode, entry_side, backend, broker_id, broker_account_id, paper_portfolio_id, "
        " timeframe, quantity, max_order_value, max_daily_loss, "
        " instrument_type, underlying, status, created_at, updated_at) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?, datetime('now'), datetime('now'))"));
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
    q.addBindValue(d.instrument_type); // F&O: option|future — drives the multi-leg path on resume
    q.addBindValue(d.underlying);      // F&O underlying for chain lookup on resume
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
    (void)QtConcurrent::run([self]() {
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
            d.instrument_type = q.value("instrument_type").toString();
            d.underlying = q.value("underlying").toString();
            d.resolved_expiry = q.value("resolved_expiry").toString();
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
        // F&O: needed so the resumed runner takes the multi-leg path and can
        // reattach its open basket (resolved_legs_json read in restore_state_from_db).
        d.instrument_type = q.value("instrument_type").toString();
        d.underlying = q.value("underlying").toString();
        d.resolved_expiry = q.value("resolved_expiry").toString();
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
