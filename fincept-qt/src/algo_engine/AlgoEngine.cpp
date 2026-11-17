// src/algo_engine/AlgoEngine.cpp
#include "algo_engine/AlgoEngine.h"

#include "algo_engine/fno/FnoExecution.h"
#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "storage/sqlite/Database.h"
#include "trading/AccountManager.h"
#include "trading/ActionCenter.h"
#include "trading/BrokerInterface.h"
#include "trading/DataStreamManager.h"
#include "trading/OrderThreadPool.h"
#include "trading/PaperTrading.h"
#include "trading/UnifiedTrading.h"

#include <QCoreApplication>
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

// How long the engine thread gets to acknowledge a stop before the GUI-thread
// watchdog force-detaches the runner.
static constexpr int kAlgoEngineStopConfirmMs = 10000;

namespace {

// ── LIVE fill confirmation ─────────────────────────────────────────────────────
// A successful place_order only means the broker ACCEPTED the order; the exchange
// can still reject or cancel it (margin, circuit, freeze qty). Positions are
// recorded only from the broker's order book.
constexpr int kFillPollFirstDelayMs = 1000;
constexpr int kFillPollIntervalMs = 2000;
constexpr int kFillPollAttempts = 15; // ≈ 30 s

enum class FillOutcome { Filled, Dead, Unconfirmed };

struct FillCheck {
    FillOutcome outcome = FillOutcome::Unconfirmed;
    double filled_qty = 0;
    double avg_price = 0;
    double ordered_qty = 0;
    QString detail = QStringLiteral("no status reported by the broker yet");
};

bool algo_status_is_filled(const QString& s) {
    return s == "filled" || s == "complete" || s == "completed" || s == "executed" || s == "traded";
}

bool algo_status_is_dead(const QString& s) {
    return s == "rejected" || s == "cancelled" || s == "canceled" || s == "expired" || s == "lapsed" ||
           s == "cancelled_amo" || s == "failed";
}

// BLOCKING (broker round-trips + sleeps): run on blocking_thread_pool() only.
// Polls the account's order book until every id is terminal or the bound expires.
QHash<QString, FillCheck> await_broker_fills(const QString& account_id, const QStringList& order_ids) {
    QHash<QString, FillCheck> out;
    for (const auto& id : order_ids)
        out.insert(id, FillCheck{});
    auto* broker = fincept::trading::AccountManager::instance().broker_for(account_id);
    if (!broker) {
        for (auto it = out.begin(); it != out.end(); ++it)
            it->detail = QStringLiteral("broker not available to confirm the fill");
        return out;
    }
    const auto creds = fincept::trading::AccountManager::instance().load_credentials(account_id);
    QStringList open = order_ids;
    for (int attempt = 0; attempt < kFillPollAttempts && !open.isEmpty(); ++attempt) {
        QThread::msleep(attempt == 0 ? kFillPollFirstDelayMs : kFillPollIntervalMs);
        const auto book = broker->get_orders(creds);
        if (!book.success || !book.data.has_value()) {
            for (const auto& id : std::as_const(open))
                out[id].detail = QStringLiteral("order book unreadable (%1)").arg(book.error);
            continue;
        }
        for (int i = open.size() - 1; i >= 0; --i) {
            const QString id = open.at(i);
            FillCheck& fc = out[id];
            const fincept::trading::BrokerOrderInfo* info = nullptr;
            for (const auto& o : *book.data) {
                if (o.order_id == id || (!o.exchange_order_id.isEmpty() && o.exchange_order_id == id)) {
                    info = &o;
                    break;
                }
            }
            if (!info) {
                fc.detail = QStringLiteral("order not found in the broker order book");
                continue;
            }
            const QString st = info->status.trimmed().toLower();
            fc.filled_qty = info->filled_qty;
            fc.avg_price = info->avg_price;
            fc.ordered_qty = info->quantity;
            if (algo_status_is_filled(st)) {
                if (fc.filled_qty <= 0)
                    fc.filled_qty = info->quantity; // brokers that report status but not filled_qty
                fc.outcome = FillOutcome::Filled;
                fc.detail = info->status;
                open.removeAt(i);
            } else if (algo_status_is_dead(st)) {
                fc.outcome = FillOutcome::Dead;
                fc.detail = info->message.isEmpty() ? info->status : info->status + QStringLiteral(": ") + info->message;
                open.removeAt(i);
            } else {
                fc.detail = QStringLiteral("still '%1' at the broker").arg(info->status);
            }
        }
    }
    return out;
}

// UnifiedOrder::source of an order a deployment sends for a signal: "algo:<id>" for an
// entry, "algo_exit:<id>" for every other reason — exit signal, stop-loss, take-profit,
// trailing stop, daily-loss / drawdown flatten, stop-with-position flatten — as
// DeploymentRunner classifies them (reason != "entry_signal" is an exit).
// trading/OrderApprovalRule.h.
QString algo_engine_order_source(const QString& deployment_id, const QString& reason) {
    return (reason == QLatin1String("entry_signal") ? QStringLiteral("algo:") : QStringLiteral("algo_exit:")) +
           deployment_id;
}

} // namespace

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

    // Semi-Auto approval outcomes. ActionCenter lives on the main thread and this
    // engine on engine_thread_, so these arrive queued — which is what we want:
    // the runner bookkeeping then runs on the runner's own thread.
    connect(&fincept::trading::ActionCenter::instance(), &fincept::trading::ActionCenter::order_approved, this,
            &AlgoEngine::on_approval_approved);
    connect(&fincept::trading::ActionCenter::instance(), &fincept::trading::ActionCenter::order_rejected, this,
            &AlgoEngine::on_approval_rejected);

    engine_thread_.setObjectName(QStringLiteral("AlgoEngineThread"));
    engine_thread_.start();
    moveToThread(&engine_thread_);
}

AlgoEngine::~AlgoEngine() {
    // Process exit: ask runners to stop but do NOT mark rows stopped in the DB -
    // recover_orphaned() resumes deployments that were live at shutdown. (stop_all()
    // marks rows stopped, which is the user's explicit STOP ALL.)
    {
        QMutexLocker lock(&mutex_);
        for (auto* r : std::as_const(runners_)) {
            r->request_stop();
            QMetaObject::invokeMethod(r, &DeploymentRunner::stop, Qt::QueuedConnection);
        }
    }
    engine_thread_.quit();
    engine_thread_.wait(5000);
    delete fno_bridge_;
    fno_bridge_ = nullptr;
}

void AlgoEngine::start_deployment(const services::algo::AlgoDeployment& deployment,
                                  const services::algo::AlgoStrategy& strategy) {
    QMutexLocker lock(&mutex_);
    if (runners_.contains(deployment.id)) {
        lock.unlock(); // never emit under the (non-recursive) lock — a direct slot would deadlock
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
    connect(runner, &DeploymentRunner::status_changed, this, [this, runner](const QString& id, const QString& status) {
        if (status == "stopped" || status == "error") {
            QMutexLocker lock(&mutex_);
            // Compare by identity: a runner force-detached by verify_stop() is no
            // longer in the map, but it is still THIS object and must be deleted.
            const bool registered = runners_.value(id, nullptr) == runner;
            if (registered)
                runners_.remove(id);
            stopping_.remove(id);
            lock.unlock(); // never emit under the (non-recursive) lock — a direct slot would deadlock
            runner->deleteLater();
            if (registered)
                emit deployment_stopped(id);
        }
        emit deployment_state_changed(id, status);
        auto& hub = datahub::DataHub::instance();
        hub.publish(QStringLiteral("algo:state:") + id, QVariant::fromValue(status));
    });

    // Publish metrics to DataHub on update
    connect(runner, &DeploymentRunner::metrics_updated, this, [](const QString& dep_id, const AlgoMetrics& m) {
        datahub::DataHub::instance().publish(QStringLiteral("algo:metrics:") + dep_id, QVariant::fromValue(m));
    });
    connect(runner, &DeploymentRunner::trade_executed, this, [](const AlgoTradeRecord& trade) {
        datahub::DataHub::instance().publish(QStringLiteral("algo:trade:") + trade.deployment_id,
                                             QVariant::fromValue(trade));
    });

    runners_.insert(deployment.id, runner);
    lock.unlock();

    // Persist the row BEFORE the runner's queued start() (which UPDATEs status) and
    // before the UI refreshes the Dashboard — both read/expect this row to exist.
    persist_deployment(deployment);

    QMetaObject::invokeMethod(runner, &DeploymentRunner::start, Qt::QueuedConnection);
    emit deployment_started(deployment.id);
    LOG_INFO(
        "AlgoEngine",
        QString("Started deployment %1 for strategy '%2' on %3").arg(deployment.id, strategy.name, deployment.symbol));
}

AlgoStopResult AlgoEngine::stop_deployment(const QString& deployment_id) {
    return stop_deployment_impl(deployment_id, QString());
}

AlgoStopResult AlgoEngine::stop_deployment_impl(const QString& deployment_id, const QString& note) {
    AlgoStopResult res;
    res.deployment_id = deployment_id;

    DeploymentRunner* runner = nullptr;
    bool already_stopping = false;
    {
        QMutexLocker lock(&mutex_);
        runner = runners_.value(deployment_id, nullptr);
        if (runner) {
            already_stopping = stopping_.contains(deployment_id);
            stopping_.insert(deployment_id);
            // 1) Kill flag first - effective immediately on the engine thread.
            runner->request_stop();
            res.runner_found = true;
            res.orders_blocked = true;
            const auto& d = runner->deployment();
            res.strategy_name = d.strategy_name;
            res.symbol = d.symbol.isEmpty() ? d.underlying : d.symbol;
            res.mode = d.mode;
        }
    }

    // 2) Persist now, from this thread: whatever the engine thread does next, the
    //    row will not be resumed by recover_orphaned() after a restart. (A STOP on a
    //    row with no runner - stale, errored, or recovery not run yet - used to do
    //    nothing at all, and that row resumed trading at the next launch.)
    res.db_marked_stopped = mark_stopped_in_db(deployment_id, note);
    if (res.strategy_name.isEmpty()) {
        auto db = fincept::Database::instance().connection();
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT strategy_name, symbol, underlying, mode FROM algo_deployments WHERE id = ?"));
        q.addBindValue(deployment_id);
        if (!q.exec()) {
            LOG_WARN("AlgoEngine", logf("Deployment row could not be read for the stop report",
                                        {{"deployment", deployment_id}, {"error", q.lastError().text()}}));
        } else if (q.next()) {
            res.strategy_name = q.value(0).toString();
            res.symbol = q.value(1).toString().isEmpty() ? q.value(2).toString() : q.value(1).toString();
            res.mode = q.value(3).toString();
        }
    }

    if (!runner) {
        res.note = QStringLiteral("no runner in this session - row marked stopped");
        LOG_INFO("AlgoEngine", QString("Stop %1: %2").arg(deployment_id, res.note));
        emit deployment_stopped(deployment_id); // refresh the Dashboard
        return res;
    }

    if (already_stopping) {
        res.note = QStringLiteral("stop already in progress");
        return res;
    }

    // 3) Orderly cleanup on the runner's own thread (unsubscribe, persist metrics).
    QMetaObject::invokeMethod(runner, &DeploymentRunner::stop, Qt::QueuedConnection);

    // 4) Verify from the GUI thread - a timer on the engine thread would never fire
    //    if that thread is the one that is stuck.
    if (auto* app = QCoreApplication::instance()) {
        QTimer::singleShot(kAlgoEngineStopConfirmMs, app, [this, deployment_id]() { verify_stop(deployment_id); });
        res.note = QStringLiteral("orders blocked; stop confirmation pending");
    } else {
        res.note = QStringLiteral("orders blocked; application shutting down");
    }
    LOG_INFO("AlgoEngine", QString("Stop requested for %1 (%2)").arg(deployment_id, res.note));
    return res;
}

bool AlgoEngine::mark_stopped_in_db(const QString& deployment_id, const QString& note) {
    auto db = fincept::Database::instance().connection();
    QSqlQuery q(db);
    if (note.isEmpty()) {
        q.prepare(QStringLiteral("UPDATE algo_deployments SET status='stopped', updated_at=datetime('now') WHERE id=?"));
    } else {
        q.prepare(QStringLiteral("UPDATE algo_deployments SET status='stopped', error_message=?, "
                                 "updated_at=datetime('now') WHERE id=?"));
        q.addBindValue(note);
    }
    q.addBindValue(deployment_id);
    if (!q.exec()) {
        LOG_ERROR("AlgoEngine", QString("Could not mark %1 stopped: %2").arg(deployment_id, q.lastError().text()));
        return false;
    }
    return true;
}

void AlgoEngine::verify_stop(const QString& deployment_id) {
    DeploymentRunner* runner = nullptr;
    services::algo::AlgoDeployment d;
    {
        QMutexLocker lock(&mutex_);
        if (!stopping_.contains(deployment_id))
            return; // acknowledged: the status handler cleared it
        runner = runners_.value(deployment_id, nullptr);
        if (!runner) {
            stopping_.remove(deployment_id);
            return;
        }
        // Force-detach: the engine never processed the stop. The kill flag already
        // blocks every order path; take the runner out of the active set so the
        // Dashboard and active_count() stop reporting it as running. It is deleted
        // when (if) its thread wakes up and finally runs stop().
        d = runner->deployment(); // copied under the lock: the object may be deleted later
        runners_.remove(deployment_id);
        stopping_.remove(deployment_id);
    }
    // Cut its market data from this thread so no further ticks are queued to it.
    trading::DataStreamManager::instance().close_quote_feed(QStringLiteral("algo:") + deployment_id,
                                                            d.broker_account_id);
    const QString msg =
        QStringLiteral("The algo engine did not confirm the stop within %1 s. New orders are blocked and the "
                       "runner was detached; %2")
            .arg(kAlgoEngineStopConfirmMs / 1000)
            .arg(d.mode == QLatin1String("live")
                     ? QStringLiteral("this is a LIVE deployment - verify open orders and positions at the broker.")
                     : QStringLiteral("no further paper fills will be recorded."));
    mark_stopped_in_db(deployment_id, msg);
    LOG_ERROR("AlgoEngine", QString("Deployment %1: %2").arg(deployment_id, msg));
    emit error_occurred(deployment_id, msg);
    emit deployment_stopped(deployment_id);
}

QVector<AlgoStopResult> AlgoEngine::stop_all_live(const QString& reason) {
    QStringList ids;
    {
        QMutexLocker lock(&mutex_);
        for (auto it = runners_.constBegin(); it != runners_.constEnd(); ++it)
            if (it.value()->deployment().mode == QLatin1String("live"))
                ids << it.key();
    }
    // Rows that could resume or are mid-start without a runner in this session.
    {
        auto db = fincept::Database::instance().connection();
        QSqlQuery q(db);
        if (q.exec(QStringLiteral("SELECT id FROM algo_deployments WHERE mode='live' "
                                  "AND status IN ('running','starting','error','crashed')"))) {
            while (q.next()) {
                const QString id = q.value(0).toString();
                if (!ids.contains(id))
                    ids << id;
            }
        } else {
            // Runner-less live rows would be missed here and resume at the next launch.
            LOG_ERROR("AlgoEngine", logf("Live deployments could not be listed for stop-all",
                                         {{"reason", reason}, {"error", q.lastError().text()}}));
        }
    }
    QVector<AlgoStopResult> out;
    out.reserve(ids.size());
    for (const auto& id : ids)
        out.append(stop_deployment_impl(id, reason));
    LOG_WARN("AlgoEngine", QString("stop_all_live (%1): %2 live deployment(s) stopped").arg(reason).arg(out.size()));
    return out;
}

void AlgoEngine::pause_deployment(const QString& deployment_id) {
    QMutexLocker lock(&mutex_);
    auto* runner = runners_.value(deployment_id, nullptr);
    if (!runner)
        return;
    lock.unlock();
    QMetaObject::invokeMethod(runner, &DeploymentRunner::pause, Qt::QueuedConnection);
}

void AlgoEngine::resume_deployment(const QString& deployment_id) {
    QMutexLocker lock(&mutex_);
    auto* runner = runners_.value(deployment_id, nullptr);
    if (!runner)
        return;
    lock.unlock();
    QMetaObject::invokeMethod(runner, &DeploymentRunner::resume, Qt::QueuedConnection);
}

void AlgoEngine::flatten_and_stop(const QString& deployment_id) {
    DeploymentRunner* runner = nullptr;
    {
        QMutexLocker lock(&mutex_);
        runner = runners_.value(deployment_id, nullptr);
        if (runner && stopping_.contains(deployment_id))
            runner = nullptr; // a stop is already in flight: just let it finish
    }
    if (!runner) {
        stop_deployment(deployment_id);
        return;
    }
    // The exit goes through the runner's normal order path (paper simulation, or
    // the live broker with the approval queue and fill confirmation). Only once it
    // reports back is the deployment stopped — the kill flag set by stop would
    // otherwise block that very exit. Whatever the outcome, the stop follows.
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(runner, &DeploymentRunner::flatten_finished, this,
                    [this, conn, deployment_id](const QString& id, bool flat, const QString& detail) {
                        if (id != deployment_id)
                            return;
                        QObject::disconnect(*conn);
                        if (!flat)
                            emit error_occurred(id, QStringLiteral("Stop: the position could NOT be closed (%1) — the "
                                                                   "deployment is stopped anyway; close it at the "
                                                                   "broker.")
                                                        .arg(detail));
                        LOG_INFO("AlgoEngine", QString("Deployment %1: flatten %2 (%3) — stopping")
                                                   .arg(id, flat ? QStringLiteral("done") : QStringLiteral("FAILED"),
                                                        detail));
                        // stop_deployment() is a GUI-thread call (its watchdog timer).
                        if (auto* app = QCoreApplication::instance())
                            QMetaObject::invokeMethod(
                                app, [id]() { AlgoEngine::instance().stop_deployment(id); }, Qt::QueuedConnection);
                    });
    QMetaObject::invokeMethod(runner, &DeploymentRunner::request_flatten, Qt::QueuedConnection);
    LOG_INFO("AlgoEngine", QString("Deployment %1: flatten-then-stop requested").arg(deployment_id));
}

void AlgoEngine::stop_all() {
    QMutexLocker lock(&mutex_);
    auto ids = runners_.keys();
    lock.unlock();
    for (const auto& id : ids)
        stop_deployment_impl(id, QString());
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

void AlgoEngine::on_approval_approved(const QString& pending_id, const QString& broker_order_id) {
    QMutexLocker lock(&mutex_);
    const QString dep_id = approvals_.take(pending_id);
    if (dep_id.isEmpty())
        return; // not one of ours (another screen queued it)
    auto* runner = runners_.value(dep_id, nullptr);
    lock.unlock();
    if (!runner)
        return; // deployment stopped while the order sat in the queue
    LOG_INFO("AlgoEngine", QString("Deployment %1: queued order %2 approved -> broker order %3")
                               .arg(dep_id, pending_id, broker_order_id));
    // Approval placed the order (accepted); the fill is confirmed from the order
    // book before any position is recorded, exactly like the un-gated live path.
    runner->on_approval_resolved(pending_id, true, broker_order_id, QString());
    confirm_fill(dep_id, runner->deployment().broker_account_id, broker_order_id);
}

void AlgoEngine::on_approval_rejected(const QString& pending_id, const QString& reason) {
    QMutexLocker lock(&mutex_);
    const QString dep_id = approvals_.take(pending_id);
    if (dep_id.isEmpty())
        return;
    auto* runner = runners_.value(dep_id, nullptr);
    lock.unlock();
    if (!runner)
        return;
    LOG_WARN("AlgoEngine",
             QString("Deployment %1: queued order %2 rejected: %3").arg(dep_id, pending_id, reason));
    runner->on_approval_resolved(pending_id, false, QString(), reason);
}

void AlgoEngine::execute_order(const AlgoOrderSignal& signal) {
    // Last gate before any order leaves the engine: a deployment being stopped (or
    // already gone) places nothing, even if its signal was emitted a moment before
    // the kill flag was set.
    {
        QMutexLocker lock(&mutex_);
        auto* r = runners_.value(signal.deployment_id, nullptr);
        if (!r || r->stop_requested()) {
            lock.unlock();
            LOG_WARN("AlgoEngine", QString("Deployment %1: %2 order DROPPED - deployment is stopping/stopped")
                                       .arg(signal.deployment_id, signal.reason));
            return;
        }
    }
    // Multi-leg F&O basket → dedicated path (paper portfolio / live broker basket).
    if (!signal.legs.isEmpty()) {
        execute_basket(signal);
        return;
    }

    QPointer<AlgoEngine> self = this;
    QString dep_id = signal.deployment_id;
    const double submitted_price = signal.price; // paper: simulated fill price
    const double qty = signal.quantity;

    // ── Paper: never touch a real broker ────────────────────────────────────────
    // This is the hard safety gate: even though a paper deployment carries a real
    // broker account (its data source), its orders are never sent to that broker.
    // The simulator ACCEPTS the order; the runner fills it on the next quote (the
    // next bar's open for a bar strategy, as in the backtest) with the cost
    // model's slippage and fees. It used to fill instantly at the signal bar's
    // close, so paper systematically beat its own backtest.
    if (signal.mode != QStringLiteral("live")) {
        const QString sim_id = QStringLiteral("paper-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
        QMetaObject::invokeMethod(
            this,
            [self, dep_id, sim_id]() {
                if (!self)
                    return;
                QMutexLocker lock(&self->mutex_);
                auto* runner = self->runners_.value(dep_id, nullptr);
                if (!runner)
                    return;
                lock.unlock();
                runner->on_paper_order_accepted(sim_id);
            },
            Qt::QueuedConnection);
        // Simulated here, never through PaperTrading/UnifiedTrading, so the order audit
        // trail for it is written here (the fill: DeploymentRunner::fill_waiting_paper_order).
        LOG_AUDIT("Orders", "Order submitted",
                  {{"venue", "paper"},
                   {"account", signal.account_id},
                   {"mode", "paper"},
                   {"symbol", signal.symbol},
                   {"exchange", signal.exchange},
                   {"side", signal.side},
                   {"type", "market"},
                   {"qty", qty},
                   {"price", submitted_price},
                   {"client_id", sim_id},
                   {"source", algo_engine_order_source(dep_id, signal.reason)}});
        LOG_DEBUG("AlgoEngine", QString("Deployment %1: PAPER order %2 %3 accepted (ref %4) — fills on the next quote")
                                    .arg(dep_id, signal.side)
                                    .arg(qty)
                                    .arg(submitted_price));
        return;
    }

    fincept::trading::UnifiedOrder order;
    order.symbol = signal.symbol;
    order.exchange = signal.exchange;
    order.side = (signal.side == "BUY") ? trading::OrderSide::Buy : trading::OrderSide::Sell;
    order.order_type = trading::OrderType::Market;
    order.quantity = signal.quantity;
    // Provenance + the approval rule (OrderApprovalRule.h): algo:<id> for an entry,
    // algo_exit:<id> for an exit / stop-loss / flatten. The Semi-Auto gate below still
    // queues every live order of the deployment, exits included, as before.
    order.source = algo_engine_order_source(dep_id, signal.reason);
    const QString account_id = signal.account_id;
    if (signal.product_type == "CNC" || signal.product_type == "delivery")
        order.product_type = trading::ProductType::Delivery;
    else if (signal.product_type == "NRML" || signal.product_type == "margin")
        order.product_type = trading::ProductType::Margin;
    else if (signal.product_type == "mtf" || signal.product_type == "MTF")
        order.product_type = trading::ProductType::MTF;
    else if (signal.product_type.isEmpty() || signal.product_type == "intraday" || signal.product_type == "MIS")
        order.product_type = trading::ProductType::Intraday;
    else {
        // Cover / bracket (or an unknown) product needs stop/target legs the engine
        // does not send; it used to go out silently as a plain intraday (MIS) order —
        // not what the user deployed. Refuse it instead.
        const QString why = QStringLiteral("product type '%1' is not supported for algo orders (use Intraday, "
                                           "Delivery, Margin or MTF) — order NOT sent")
                                .arg(signal.product_type);
        LOG_ERROR("AlgoEngine", QString("Deployment %1: %2").arg(dep_id, why));
        QMutexLocker lock(&mutex_);
        auto* runner = runners_.value(dep_id, nullptr);
        lock.unlock();
        if (runner)
            runner->on_order_rejected(QString(), why);
        return;
    }

    // ── Semi-Auto approval gate ─────────────────────────────────────────────────
    // Every other live-order path opts into this (MCP live tools, workflow order
    // nodes, the equity screen, basket/broadcast dialogs). Without it an account
    // set to "Semi-Auto — require approval" still had algo deployments firing
    // unattended orders on it. Mirrors mcp/tools/LiveTradingTools.cpp and
    // services/workflow/adapters/ServiceBridges.cpp: queue, don't send.
    if (fincept::trading::ActionCenter::instance().should_queue(account_id, QStringLiteral("placeorder"))) {
        const QString pending_id = fincept::trading::ActionCenter::instance().queue_order(
            account_id, QStringLiteral("placeorder"),
            fincept::trading::ActionCenter::serialize_unified_order(order));

        QMutexLocker lock(&mutex_);
        auto* runner = runners_.value(dep_id, nullptr);
        if (pending_id.isEmpty()) {
            lock.unlock();
            LOG_ERROR("AlgoEngine",
                      QString("Deployment %1: Semi-Auto queue failed — order dropped (NOT sent)").arg(dep_id));
            if (runner)
                runner->on_order_rejected(QString(), QStringLiteral("Semi-Auto: failed to queue for approval"));
            emit error_occurred(dep_id, QStringLiteral("Order could not be queued for approval"));
            return;
        }
        approvals_.insert(pending_id, dep_id);
        lock.unlock();
        // Keeps the runner's in-flight guard held until the user acts, so the
        // deployment cannot stack one approval request per tick.
        if (runner)
            runner->on_order_queued_for_approval(pending_id);
        LOG_WARN("AlgoEngine", QString("Deployment %1: account %2 is in Semi-Auto — order queued for approval (%3)")
                                   .arg(dep_id, account_id, pending_id));
        return;
    }

    // Dedicated order executor (never starved by sleeping jobs on the global pool).
    (void)QtConcurrent::run(&fincept::trading::order_thread_pool(), [self, dep_id, account_id, order]() {
        auto response = fincept::trading::UnifiedTrading::instance().place_order(account_id, order);

        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, dep_id, account_id, response]() {
                if (!self)
                    return;
                QMutexLocker lock(&self->mutex_);
                auto* runner = self->runners_.value(dep_id, nullptr);
                if (!runner)
                    return;
                lock.unlock();

                if (response.success) {
                    // ACCEPTED, not filled: keep it in flight and confirm the fill
                    // (status + average price + filled qty) from the order book.
                    runner->on_order_accepted(response.order_id);
                    self->confirm_fill(dep_id, account_id, response.order_id);
                } else {
                    runner->on_order_rejected(response.order_id, response.message);
                }
            },
            Qt::QueuedConnection);
    });
}

void AlgoEngine::confirm_fill(const QString& deployment_id, const QString& account_id,
                              const QString& broker_order_id) {
    QPointer<AlgoEngine> self = this;
    auto deliver = [self, deployment_id, broker_order_id](const FillCheck& fc) {
        if (!self)
            return;
        QMutexLocker lock(&self->mutex_);
        auto* runner = self->runners_.value(deployment_id, nullptr);
        lock.unlock();
        if (!runner) {
            LOG_WARN("AlgoEngine", QString("Deployment %1: fill check for order %2 finished after the runner was "
                                           "removed (%3) — verify the position at the broker")
                                       .arg(deployment_id, broker_order_id, fc.detail));
            return;
        }
        switch (fc.outcome) {
        case FillOutcome::Filled:
            runner->on_order_filled(broker_order_id, fc.avg_price, fc.filled_qty);
            break;
        case FillOutcome::Dead:
            if (fc.filled_qty > 0) {
                // Partly filled, remainder rejected/cancelled: the filled part is real.
                LOG_WARN("AlgoEngine", QString("Deployment %1: order %2 partly filled (%3) then %4")
                                           .arg(deployment_id, broker_order_id)
                                           .arg(fc.filled_qty)
                                           .arg(fc.detail));
                runner->on_order_filled(broker_order_id, fc.avg_price, fc.filled_qty);
            } else {
                runner->on_order_rejected(broker_order_id, fc.detail);
            }
            break;
        case FillOutcome::Unconfirmed:
            runner->on_order_unconfirmed(broker_order_id, fc.detail);
            break;
        }
    };

    if (broker_order_id.trimmed().isEmpty()) {
        FillCheck fc;
        fc.detail = QStringLiteral("the broker accepted the order but returned no order id, so its fill cannot be "
                                   "verified");
        deliver(fc);
        return;
    }

    (void)QtConcurrent::run(&fincept::trading::blocking_thread_pool(), [self, account_id, broker_order_id, deliver]() {
        const auto checks = await_broker_fills(account_id, QStringList{broker_order_id});
        const FillCheck fc = checks.value(broker_order_id);
        if (!self)
            return;
        QMetaObject::invokeMethod(self, [deliver, fc]() { deliver(fc); }, Qt::QueuedConnection);
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
    if (auto existing = fincept::trading::pt_find_portfolio(QStringLiteral("F&O Paper"), QStringLiteral("NFO")))
        return existing->id;
    const auto created =
        fincept::trading::pt_create_portfolio(QStringLiteral("F&O Paper"), 1000000.0, QStringLiteral("INR"), 1.0,
                                              QStringLiteral("cross"), 0.001, QStringLiteral("NFO"));
    LOG_INFO("AlgoEngine", QString("Created F&O Paper portfolio %1 for basket execution").arg(created.id));
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
        // Paper audit provenance: algo:<id> for an entry basket, algo_exit:<id> for an exit.
        const QString leg_source = algo_engine_order_source(dep_id, signal.reason);
        QMetaObject::invokeMethod(
            this,
            [self, dep_id, legs, portfolio_id, leg_source]() {
                if (!self)
                    return;
                QMutexLocker lock(&self->mutex_);
                auto* runner = self->runners_.value(dep_id, nullptr);
                if (!runner)
                    return;
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
                            portfolio_id, leg.symbol, leg.side.toLower(), QStringLiteral("market"), leg.quantity,
                            leg.price, std::nullopt, false, QStringLiteral("NFO"), QStringLiteral("NRML"),
                            leg_source);
                        fincept::trading::pt_fill_order(po.id, leg.price);
                    } catch (const std::exception& e) {
                        ok = false;
                        any_fail = true;
                        LOG_ERROR("AlgoEngine", QString("Deployment %1: PAPER leg %2 (%3) failed: %4")
                                                    .arg(dep_id)
                                                    .arg(i)
                                                    .arg(leg.symbol, QString::fromUtf8(e.what())));
                    }
                    if (ok)
                        placed_ok.append(i);
                }

                if (!any_fail) {
                    for (int i = 0; i < legs.size(); ++i)
                        runner->on_leg_filled(i, legs[i], legs[i].price, legs[i].quantity);
                    return;
                }

                // Rollback the legs that did fill, then reject every leg.
                for (int i : std::as_const(placed_ok)) {
                    const auto& leg = legs[i];
                    const QString rev =
                        (leg.side.toLower() == QStringLiteral("buy")) ? QStringLiteral("sell") : QStringLiteral("buy");
                    try {
                        // A rollback is reduce-only: provenance algo_exit:<id>.
                        const auto ro = fincept::trading::pt_place_order(
                            portfolio_id, leg.symbol, rev, QStringLiteral("market"), leg.quantity, leg.price,
                            std::nullopt, true, QStringLiteral("NFO"), QStringLiteral("NRML"),
                            QStringLiteral("algo_exit:") + dep_id);
                        fincept::trading::pt_fill_order(ro.id, leg.price);
                    } catch (const std::exception& e) {
                        LOG_ERROR("AlgoEngine", QString("Deployment %1: PAPER rollback leg %2 failed: %3")
                                                    .arg(dep_id)
                                                    .arg(i)
                                                    .arg(QString::fromUtf8(e.what())));
                    }
                }
                for (int i = 0; i < legs.size(); ++i)
                    runner->on_leg_rejected(i, QStringLiteral("paper basket aborted (partial fill)"));
            },
            Qt::QueuedConnection);
        LOG_INFO("AlgoEngine", QString("Deployment %1: PAPER basket %2 legs -> portfolio %3")
                                   .arg(dep_id)
                                   .arg(legs.size())
                                   .arg(portfolio_id));
        return;
    }

    // ── Live: broker basket (ONLY signal.mode == "live") ─────────────────────────
    const QString account_id = signal.account_id;
    const bool is_entry = (signal.reason == QLatin1String("entry_signal"));

    // Semi-Auto accounts require approval for every live order. A basket approved
    // in the Action Center reports no per-leg order ids, so its fills could not be
    // confirmed and it could fill partially: refuse it whole instead of sending it
    // unapproved (this path used to bypass the approval queue entirely).
    if (fincept::trading::ActionCenter::instance().should_queue(account_id, QStringLiteral("basketorder")) ||
        fincept::trading::ActionCenter::instance().should_queue(account_id, QStringLiteral("placeorder"))) {
        const QString why = QStringLiteral("account is in Semi-Auto (approval required) — multi-leg algo baskets are "
                                           "not sent; switch the account to Auto or deploy in paper");
        QMutexLocker lock(&mutex_);
        auto* runner = runners_.value(dep_id, nullptr);
        lock.unlock();
        LOG_WARN("AlgoEngine", QString("Deployment %1: LIVE basket NOT sent: %2").arg(dep_id, why));
        if (runner)
            for (int i = 0; i < legs.size(); ++i)
                runner->on_leg_rejected(i, why);
        emit error_occurred(dep_id, QStringLiteral("F&O basket not sent: ") + why);
        return;
    }

    fincept::trading::BasketOrderRequest basket = fincept::algo::fno::build_basket_request(legs, signal.product_type);
    // Provenance + the approval rule (OrderApprovalRule.h): algo:<id> for an entry basket,
    // algo_exit:<id> for an exit basket (a basket leg is never verified reduce-only by the
    // gate, so it is treated like algo:<id> — and Semi-Auto never reaches here, see above).
    const QString leg_source = algo_engine_order_source(dep_id, signal.reason);
    for (auto& leg : basket.orders)
        leg.source = leg_source;

    fincept::trading::UnifiedTrading::instance().place_basket_orders(
        account_id, basket, [self, dep_id, legs, account_id, is_entry](const fincept::trading::BasketOrderResult& res) {
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self,
                [self, dep_id, legs, account_id, is_entry, res]() {
                    if (!self)
                        return;
                    self->confirm_basket(dep_id, account_id, is_entry, legs, res);
                },
                Qt::QueuedConnection);
        });
    LOG_INFO("AlgoEngine",
             QString("Deployment %1: LIVE basket %2 legs -> account %3").arg(dep_id).arg(legs.size()).arg(account_id));
}

void AlgoEngine::confirm_basket(const QString& deployment_id, const QString& account_id, bool is_entry,
                                const QVector<AlgoOrderLeg>& legs,
                                const fincept::trading::BasketOrderResult& placed) {
    QMutexLocker lock(&mutex_);
    auto* runner = runners_.value(deployment_id, nullptr);
    lock.unlock();
    if (!runner) {
        LOG_WARN("AlgoEngine", QString("Deployment %1: basket result arrived after the runner was removed — verify "
                                       "positions at the broker")
                                   .arg(deployment_id));
        return;
    }

    // place_basket_orders reorders (BUY-first) and batches, so map each leg to its
    // result by symbol rather than by index.
    QHash<QString, fincept::trading::BasketOrderResult::OrderResult> by_symbol;
    for (const auto& r : placed.results)
        by_symbol.insert(r.symbol, r);

    QStringList accepted_ids;
    QStringList refused;
    for (const auto& leg : legs) {
        const auto it = by_symbol.constFind(leg.symbol);
        if (it != by_symbol.constEnd() && it->success && !it->order_id.isEmpty())
            accepted_ids << it->order_id;
        else if (it != by_symbol.constEnd() && it->success)
            accepted_ids << QString(); // accepted without an id — unverifiable
        else
            refused << QStringLiteral("%1 (%2)").arg(leg.symbol,
                                                     it != by_symbol.constEnd() ? it->error : QStringLiteral("no result"));
    }

    if (accepted_ids.isEmpty()) {
        // Nothing reached the broker: no exposure changed.
        for (int i = 0; i < legs.size(); ++i)
            runner->on_leg_rejected(i, QStringLiteral("basket rejected"));
        emit error_occurred(deployment_id, QStringLiteral("F&O %1 basket rejected: %2")
                                               .arg(is_entry ? QStringLiteral("entry") : QStringLiteral("exit"),
                                                    refused.join(QStringLiteral("; "))));
        return;
    }

    runner->on_order_accepted(QString()); // in flight: confirming fills, not purged as a lost ack
    QPointer<AlgoEngine> self = this;
    (void)QtConcurrent::run(&fincept::trading::blocking_thread_pool(), [self, deployment_id, account_id, is_entry,
                                                                        legs, placed, accepted_ids, refused]() {
        QStringList ids;
        for (const auto& id : accepted_ids)
            if (!id.isEmpty())
                ids << id;
        const auto checks = await_broker_fills(account_id, ids);

        // Classify every leg.
        QVector<double> fill_px(legs.size(), 0.0);
        QVector<double> fill_qty(legs.size(), 0.0);
        QStringList unknown, short_filled;
        bool all_filled = refused.isEmpty();
        for (int i = 0; i < legs.size(); ++i) {
            QString oid;
            bool ok_placed = false;
            for (const auto& r : placed.results)
                if (r.symbol == legs[i].symbol) {
                    oid = r.order_id;
                    ok_placed = r.success;
                    break;
                }
            if (!ok_placed) {
                all_filled = false;
                continue;
            }
            if (oid.isEmpty()) {
                unknown << QStringLiteral("%1 (no order id)").arg(legs[i].symbol);
                all_filled = false;
                continue;
            }
            const FillCheck fc = checks.value(oid);
            if (fc.outcome == FillOutcome::Unconfirmed) {
                unknown << QStringLiteral("%1 (%2)").arg(legs[i].symbol, fc.detail);
                all_filled = false;
            } else if (fc.outcome == FillOutcome::Filled && fc.filled_qty + 1e-9 >= legs[i].quantity) {
                fill_px[i] = fc.avg_price > 0 ? fc.avg_price : legs[i].price;
                fill_qty[i] = fc.filled_qty;
            } else {
                short_filled << QStringLiteral("%1 (%2, filled %3/%4)")
                                    .arg(legs[i].symbol, fc.detail)
                                    .arg(fc.filled_qty)
                                    .arg(legs[i].quantity);
                all_filled = false;
            }
        }

        // Entry with a failed leg and nothing unknown: undo what filled through the
        // verified unwind (reads the book, cancels open remainders, reverses ONLY
        // the filled quantity). Exits are never auto-reversed (that would re-open
        // a closed leg); a partial exit latches the runner for the user instead.
        // The reversals are this deployment's rollback: provenance algo_exit:<id>.
        QVector<fincept::trading::UnifiedTrading::UnwindLegResult> unwind;
        const bool do_unwind = is_entry && !all_filled && unknown.isEmpty();
        if (do_unwind)
            unwind = fincept::trading::UnifiedTrading::instance().unwind_basket_legs(
                account_id, placed.results, QStringLiteral("algo_exit:") + deployment_id);

        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, deployment_id, is_entry, legs, fill_px, fill_qty, all_filled, unknown, short_filled, refused,
             do_unwind, unwind]() {
                if (!self)
                    return;
                QMutexLocker lk(&self->mutex_);
                auto* r = self->runners_.value(deployment_id, nullptr);
                lk.unlock();
                if (!r) {
                    LOG_WARN("AlgoEngine", QString("Deployment %1: basket fill check finished after the runner was "
                                                   "removed — verify positions at the broker")
                                               .arg(deployment_id));
                    return;
                }
                if (all_filled) {
                    for (int i = 0; i < legs.size(); ++i)
                        r->on_leg_filled(i, legs[i], fill_px[i], fill_qty[i]);
                    return;
                }
                QStringList problems;
                if (!refused.isEmpty())
                    problems << QStringLiteral("refused: ") + refused.join(QStringLiteral("; "));
                if (!short_filled.isEmpty())
                    problems << QStringLiteral("not filled: ") + short_filled.join(QStringLiteral("; "));
                if (!unknown.isEmpty())
                    problems << QStringLiteral("fill unknown: ") + unknown.join(QStringLiteral("; "));

                if (do_unwind) {
                    QStringList unwind_failed, unwind_done;
                    for (const auto& u : unwind) {
                        if (u.ok)
                            unwind_done << QStringLiteral("%1: %2").arg(u.symbol, u.action);
                        else
                            unwind_failed << QStringLiteral("%1: %2").arg(u.symbol, u.error);
                    }
                    if (unwind_failed.isEmpty()) {
                        for (int i = 0; i < legs.size(); ++i)
                            r->on_leg_rejected(i, QStringLiteral("live entry basket aborted"));
                        emit self->error_occurred(
                            deployment_id, QStringLiteral("F&O entry basket aborted (%1); unwound: %2")
                                               .arg(problems.join(QStringLiteral(" | ")),
                                                    unwind_done.isEmpty() ? QStringLiteral("nothing had filled")
                                                                          : unwind_done.join(QStringLiteral("; "))));
                        return;
                    }
                    problems << QStringLiteral("UNWIND FAILED: ") + unwind_failed.join(QStringLiteral("; "));
                }
                // Broker-side exposure is unknown or partially changed: record
                // nothing and latch the runner (no further orders) for the user.
                r->on_order_unconfirmed(QString(), QStringLiteral("%1 basket: %2")
                                                       .arg(is_entry ? QStringLiteral("entry") : QStringLiteral("exit"),
                                                            problems.join(QStringLiteral(" | "))));
            },
            Qt::QueuedConnection);
    });
}

void AlgoEngine::persist_deployment(const services::algo::AlgoDeployment& d) {
    auto db = fincept::Database::instance().connection();
    QSqlQuery q(db);
    // UPSERT, not INSERT OR REPLACE: recover_orphaned() re-runs start_deployment() — and so
    // this — for every deployment that was live before a restart. REPLACE deletes the
    // existing row and re-inserts it with column defaults, which wiped resolved_legs_json /
    // resolved_expiry (the open F&O basket the runner is about to reattach from that very
    // row) before DeploymentRunner::restore_state_from_db() read it, and reset created_at.
    // The UPDATE branch deliberately leaves those columns (and pid) alone.
    q.prepare(QStringLiteral(
        "INSERT INTO algo_deployments "
        "(id, strategy_id, strategy_name, strategy_kind, symbol, exchange, product_type, "
        " mode, entry_side, backend, broker_id, broker_account_id, paper_portfolio_id, "
        " timeframe, quantity, max_order_value, max_daily_loss, "
        " instrument_type, underlying, status, created_at, updated_at) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?, datetime('now'), datetime('now')) "
        "ON CONFLICT(id) DO UPDATE SET "
        " strategy_id=excluded.strategy_id, strategy_name=excluded.strategy_name, "
        " strategy_kind=excluded.strategy_kind, symbol=excluded.symbol, exchange=excluded.exchange, "
        " product_type=excluded.product_type, mode=excluded.mode, entry_side=excluded.entry_side, "
        " backend=excluded.backend, broker_id=excluded.broker_id, "
        " broker_account_id=excluded.broker_account_id, paper_portfolio_id=excluded.paper_portfolio_id, "
        " timeframe=excluded.timeframe, quantity=excluded.quantity, max_order_value=excluded.max_order_value, "
        " max_daily_loss=excluded.max_daily_loss, instrument_type=excluded.instrument_type, "
        " underlying=excluded.underlying, status=excluded.status, error_message='', "
        " updated_at=datetime('now')"));
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
    if (!q.exec()) {
        LOG_ERROR("AlgoEngine", QString("Failed to persist deployment %1: %2").arg(d.id, q.lastError().text()));
        return;
    }
    LOG_INFO("AlgoEngine", QString("Persisted deployment row %1 (%2/%3 on %4)").arg(d.id, d.mode, d.backend, d.symbol));
    // Risk limits + cost model (v057) in their own statement, so a database that
    // has not run v057 still persists the deployment itself.
    QSqlQuery x(db);
    x.prepare(QStringLiteral("UPDATE algo_deployments SET risk_json = ?, cost_model_json = ? WHERE id = ?"));
    x.addBindValue(QString::fromUtf8(QJsonDocument(d.risk_limits).toJson(QJsonDocument::Compact)));
    x.addBindValue(QString::fromUtf8(QJsonDocument(d.cost_model).toJson(QJsonDocument::Compact)));
    x.addBindValue(d.id);
    if (!x.exec())
        LOG_WARN("AlgoEngine", QString("Deployment %1: risk limits / cost model not persisted (schema v057?): %2")
                                   .arg(d.id, x.lastError().text()));
}

void AlgoEngine::list_deployments() {
    QPointer<AlgoEngine> self = this;
    (void)QtConcurrent::run([self]() {
        auto db = fincept::Database::instance().connection();
        QSqlQuery q(db);
        if (!q.exec(QStringLiteral("SELECT * FROM algo_deployments ORDER BY created_at DESC")))
            LOG_WARN("AlgoEngine", logf("Deployments could not be listed", {{"error", q.lastError().text()}}));

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
            d.updated_at = q.value("updated_at").toString(); // last status change (UTC)
            d.paper_portfolio_id = q.value("paper_portfolio_id").toString();
            // v057 (absent → empty objects)
            d.risk_limits = QJsonDocument::fromJson(q.value("risk_json").toString().toUtf8()).object();
            d.cost_model = QJsonDocument::fromJson(q.value("cost_model_json").toString().toUtf8()).object();
            deployments.append(d);
        }

        // Load live metrics for running deployments
        QSqlQuery mq(db);
        if (!mq.exec(QStringLiteral("SELECT * FROM algo_metrics")))
            LOG_WARN("AlgoEngine", logf("Deployment metrics could not be read", {{"error", mq.lastError().text()}}));
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
                    // v057 risk state (absent → defaults)
                    deployments[i].total_fees = mq.value("total_fees").toDouble();
                    deployments[i].daily_pnl = mq.value("daily_pnl").toDouble();
                    deployments[i].risk_halted = mq.value("risk_halted").toInt() != 0;
                    deployments[i].halt_reason = mq.value("halt_reason").toString();
                    break;
                }
            }
        }

        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, deployments]() {
                if (!self)
                    return;
                emit self->deployments_loaded(deployments);
            },
            Qt::QueuedConnection);
    });
}

void AlgoEngine::recover_orphaned() {
    // On restart, deployments that were active (or stuck in error from the prior
    // no-data bug) should resume rather than silently die — that's what users
    // expect of a deployed algo. Reload each one + its strategy and restart it.
    auto db = fincept::Database::instance().connection();
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral(
            "SELECT * FROM algo_deployments WHERE status IN ('running','starting','error','crashed')")))
        LOG_ERROR("AlgoEngine", logf("Deployments to resume could not be read", {{"error", q.lastError().text()}}));

    // Stored risk limits / cost model: a corrupt cell resumes the deployment with
    // EMPTY limits — say so (never for an empty or NULL cell, the pre-v057 case).
    auto stored_json_object = [&q](const QString& deployment_id, const char* column) {
        const QString text = q.value(QLatin1String(column)).toString();
        QJsonParseError perr{};
        const QJsonObject obj = QJsonDocument::fromJson(text.toUtf8(), &perr).object();
        if (!text.trimmed().isEmpty() && perr.error != QJsonParseError::NoError)
            LOG_ERROR("AlgoEngine", logf("Stored deployment settings are invalid JSON, resuming without them",
                                         {{"deployment", deployment_id},
                                          {"field", column},
                                          {"bytes", text.size()},
                                          {"error", perr.errorString()}}));
        return obj;
    };

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
        // Risk limits + cost model (v057): a resumed deployment keeps the limits
        // it was deployed with (absent → none, the pre-v057 behaviour).
        d.risk_limits = stored_json_object(d.id, "risk_json");
        d.cost_model = stored_json_object(d.id, "cost_model_json");
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
        LOG_INFO("AlgoEngine",
                 QString("Resuming deployment %1 ('%2' on %3) after restart").arg(d.id, strat.name, d.symbol));
        start_deployment(d, strat);
    }
}

bool AlgoEngine::has_active_duplicate(const QString& strategy_id, const QString& symbol, const QString& mode,
                                      const QString& entry_side) const {
    auto db = fincept::Database::instance().connection();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM algo_deployments WHERE strategy_id=? AND symbol=? AND mode=? "
                             "AND entry_side=? AND status IN ('running','starting')"));
    q.addBindValue(strategy_id);
    q.addBindValue(symbol);
    q.addBindValue(mode);
    q.addBindValue(entry_side);
    if (q.exec() && q.next())
        return q.value(0).toInt() > 0;
    if (q.lastError().isValid())
        LOG_WARN("AlgoEngine", logf("Duplicate-deployment check failed, assuming none",
                                    {{"strategy", strategy_id},
                                     {"symbol", symbol},
                                     {"mode", mode},
                                     {"error", q.lastError().text()}}));
    return false;
}

void AlgoEngine::remove_deployment(const QString& deployment_id) {
    // Stop the runner first if it happens to be live (REMOVE is normally only shown
    // for already-stopped rows, but guard anyway).
    {
        QMutexLocker lock(&mutex_);
        if (auto* r = runners_.value(deployment_id, nullptr)) {
            r->request_stop(); // no order may leave it while the rows are deleted
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
            LOG_ERROR("AlgoEngine", logf("Deployment rows could not be deleted",
                                         {{"deployment", deployment_id},
                                          {"statement", QString::fromLatin1(sql).section(QLatin1Char(' '), 0, 2)},
                                          {"error", q.lastError().text()}}));
    }
    // Monitoring history (v057 tables; absent on an older schema — best effort).
    for (const char* sql : {"DELETE FROM algo_equity_snapshots WHERE deployment_id = ?",
                            "DELETE FROM algo_events WHERE deployment_id = ?"}) {
        QSqlQuery q(db);
        q.prepare(QString::fromLatin1(sql));
        q.addBindValue(deployment_id);
        if (!q.exec())
            LOG_DEBUG("AlgoEngine", logf("Monitoring history could not be deleted",
                                         {{"deployment", deployment_id},
                                          {"statement", QString::fromLatin1(sql).section(QLatin1Char(' '), 0, 2)},
                                          {"error", q.lastError().text()}}));
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
    // A corrupt rule cell yields an empty rule set for a strategy that may trade live — say so
    // (never for an empty or NULL cell).
    auto stored_json = [&q, &strategy_id](const char* column) {
        const QString text = q.value(QLatin1String(column)).toString();
        QJsonParseError perr{};
        QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &perr);
        if (!text.trimmed().isEmpty() && perr.error != QJsonParseError::NoError)
            LOG_ERROR("AlgoEngine", logf("Stored strategy JSON is invalid", {{"strategy", strategy_id},
                                                                            {"field", column},
                                                                            {"bytes", text.size()},
                                                                            {"error", perr.errorString()}}));
        return doc;
    };
    const bool loaded = q.exec();
    if (!loaded)
        LOG_ERROR("AlgoEngine", logf("Strategy could not be loaded",
                                     {{"strategy", strategy_id}, {"error", q.lastError().text()}}));
    if (loaded && q.next()) {
        s.id = q.value("id").toString();
        s.name = q.value("name").toString();
        s.description = q.value("description").toString();
        s.timeframe = q.value("timeframe").toString();
        s.entry_conditions = stored_json("entry_conditions").array();
        s.exit_conditions = stored_json("exit_conditions").array();
        s.entry_logic = q.value("entry_logic").toString();
        s.exit_logic = q.value("exit_logic").toString();
        s.stop_loss = q.value("stop_loss").toDouble();
        s.take_profit = q.value("take_profit").toDouble();
        s.trailing_stop = q.value("trailing_stop").toDouble();
        // F&O strategies: the leg RULES live in legs_json. Without them a deployment
        // resumed after a restart had strategy.legs == [] — resolve_entry_legs() then
        // found no legs and the multi-leg runner could never enter again (silently).
        const QString inst = q.value("instrument_type").toString();
        s.instrument_type = inst.isEmpty() ? QStringLiteral("equity") : inst;
        s.legs = stored_json("legs_json").array();
        // v057 (absent → defaults): the cost model drives paper fills / estimated fees.
        s.cost_model = stored_json("cost_model_json").object();
        const QString side = q.value("entry_side").toString();
        s.entry_side = side.isEmpty() ? QStringLiteral("BUY") : side;
        const double size = q.value("position_size_pct").toDouble();
        s.position_size_pct = size > 0 ? size : 100.0;
        s.risk_free_pct = q.value("risk_free_pct").toDouble();
    }
    return s;
}

} // namespace fincept::algo
