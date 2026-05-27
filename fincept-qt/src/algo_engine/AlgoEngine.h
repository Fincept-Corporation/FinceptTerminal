// src/algo_engine/AlgoEngine.h
#pragma once
#include "algo_engine/AlgoEngineTypes.h"
#include "algo_engine/DeploymentRunner.h"
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
    void stop_all();

    bool is_running(const QString& deployment_id) const;
    QStringList active_deployment_ids() const;
    AlgoMetrics metrics(const QString& deployment_id) const;
    int active_count() const;

    void list_deployments();
    void recover_orphaned();

signals:
    void deployment_started(const QString& deployment_id);
    void deployment_stopped(const QString& deployment_id);
    void deployment_crashed(const QString& deployment_id, const QString& reason);
    void deployments_loaded(QVector<fincept::services::algo::AlgoDeployment> deployments);
    void trade_executed(const fincept::algo::AlgoTradeRecord& trade);
    void metrics_updated(const QString& deployment_id, const fincept::algo::AlgoMetrics& metrics);
    void error_occurred(const QString& deployment_id, const QString& error);

private slots:
    void on_order_requested(const fincept::algo::AlgoOrderSignal& signal);

private:
    AlgoEngine();
    ~AlgoEngine() override;
    Q_DISABLE_COPY(AlgoEngine)

    void execute_order(const AlgoOrderSignal& signal);

    QThread engine_thread_;
    mutable QMutex mutex_;
    QHash<QString, DeploymentRunner*> runners_;
};

} // namespace fincept::algo
