// src/algo_engine/AlgoEngine.h
#pragma once
#include "algo_engine/AlgoEngineTypes.h"
#include "algo_engine/DeploymentRunner.h"
#include "algo_engine/fno/FnoDataBridge.h"
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QThread>
#include <QTimer>

namespace fincept::algo {

class AlgoEngine : public QObject {
    Q_OBJECT
public:
    static AlgoEngine& instance();

    void start_deployment(const fincept::services::algo::AlgoDeployment& deployment,
                          const fincept::services::algo::AlgoStrategy& strategy);
    void stop_deployment(const QString& deployment_id);
    void pause_deployment(const QString& deployment_id);
    void resume_deployment(const QString& deployment_id);
    // Permanently delete a deployment and all its rows (deployments, metrics,
    // heartbeat, order signals, trades), stopping the runner first if live. Then
    // refreshes the deployment list. This is what the Dashboard's REMOVE invokes.
    void remove_deployment(const QString& deployment_id);
    void stop_all();

    bool is_running(const QString& deployment_id) const;
    QStringList active_deployment_ids() const;
    AlgoMetrics metrics(const QString& deployment_id) const;
    int active_count() const;

    void list_deployments();
    void recover_orphaned();
    // True if a running/starting deployment with the same strategy+symbol+mode+side
    // already exists — used to confirm before deploying an exact duplicate.
    bool has_active_duplicate(const QString& strategy_id, const QString& symbol,
                              const QString& mode, const QString& entry_side) const;

    // GUI-thread bridge for option-chain data. Created before moveToThread so it
    // stays on the main thread. Accessible to callers that need to check its state
    // from the main thread (e.g. the F&O Algo deploy dialog).
    fincept::algo::fno::FnoDataBridge* fno_bridge() const { return fno_bridge_; }

signals:
    void deployment_started(const QString& deployment_id);
    void deployment_stopped(const QString& deployment_id);
    void deployment_crashed(const QString& deployment_id, const QString& reason);
    void deployments_loaded(QVector<fincept::services::algo::AlgoDeployment> deployments);
    void trade_executed(const fincept::algo::AlgoTradeRecord& trade);
    void metrics_updated(const QString& deployment_id, const fincept::algo::AlgoMetrics& metrics);
    void error_occurred(const QString& deployment_id, const QString& error);
    // Real-time per-tick snapshot relayed from each runner to the Dashboard.
    void live_update(const QString& deployment_id, const fincept::algo::AlgoLiveSnapshot& snap);

private slots:
    void on_order_requested(const fincept::algo::AlgoOrderSignal& signal);

private:
    AlgoEngine();
    ~AlgoEngine() override;
    Q_DISABLE_COPY(AlgoEngine)

    void execute_order(const AlgoOrderSignal& signal);
    // Multi-leg F&O basket execution (P3.4). Dispatched from execute_order when
    // signal.legs is non-empty. Paper (signal.mode != "live") routes every leg to
    // the deployment's paper portfolio via pt_place_order — NEVER the broker, even
    // on a live account (the safety gate). Live (signal.mode == "live" only) sends
    // a broker basket and rolls back filled legs if any leg fails on entry.
    void execute_basket(const AlgoOrderSignal& signal);
    // Resolve the paper portfolio for a basket: the signal's paper_portfolio_id,
    // else the deployment account's, else a reused/created "F&O Paper" portfolio.
    QString resolve_paper_portfolio_id(const AlgoOrderSignal& signal);
    // Writes the deployment row to algo_deployments so the Dashboard (which reads
    // that table) can see it. Without this the runner starts in memory only and
    // the deploy is invisible to the UI.
    void persist_deployment(const fincept::services::algo::AlgoDeployment& deployment);
    // Loads a full AlgoStrategy (incl. parsed entry/exit conditions) from the
    // algo_strategies table — used to resume deployments after an app restart.
    fincept::services::algo::AlgoStrategy load_strategy(const QString& strategy_id);

    QThread engine_thread_;
    mutable QMutex mutex_;
    QHash<QString, DeploymentRunner*> runners_;
    fincept::algo::fno::FnoDataBridge* fno_bridge_ = nullptr;
};

} // namespace fincept::algo
