// src/services/algo_trading/AlgoTradingService.h
#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QObject>

namespace fincept::services::algo {

/// Singleton service for Algo Trading — strategy CRUD, deployment, scanning, backtesting.
class AlgoTradingService : public QObject {
    Q_OBJECT
  public:
    static AlgoTradingService& instance();

    // ── Strategy CRUD ───────────────────────────────────────────────────────
    void save_strategy(const AlgoStrategy& strategy);
    void list_strategies();
    void delete_strategy(const QString& id);

    // ── Deployment lifecycle ────────────────────────────────────────────────
    void deploy_strategy(const QString& strategy_id, const QString& symbol, const QString& mode,
                         const QString& timeframe, double quantity);
    void stop_deployment(const QString& deployment_id);
    void stop_all_deployments();
    void list_deployments();

    // ── Backtesting ─────────────────────────────────────────────────────────
    void run_backtest(const QString& strategy_id, const QString& symbol, const QString& start_date,
                      const QString& end_date, double capital);

    // ── Scanner ─────────────────────────────────────────────────────────────
    void run_scan(const QJsonArray& conditions, const QStringList& symbols, const QString& timeframe,
                  int lookback_days, const QString& logic = "AND");

  signals:
    void strategy_saved(QString id);
    void strategies_loaded(QVector<fincept::services::algo::AlgoStrategy> strategies);
    void strategy_deleted(QString id);
    void deployment_started(QString deployment_id);
    void deployments_loaded(QVector<fincept::services::algo::AlgoDeployment> deployments);
    void deployment_stopped(QString deployment_id);
    void backtest_result(QJsonObject data);
    void scan_result(QJsonObject data);
    void error_occurred(QString context, QString message);

  private:
    explicit AlgoTradingService(QObject* parent = nullptr);
    Q_DISABLE_COPY(AlgoTradingService)

    void run_python(const QString& script, const QStringList& args, const QString& context,
                    std::function<void(bool, const QString&)> cb);
};

} // namespace fincept::services::algo
