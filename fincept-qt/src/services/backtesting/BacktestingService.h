// src/services/backtesting/BacktestingService.h
#pragma once
#include "services/backtesting/BacktestingTypes.h"

#include <QObject>

namespace fincept::services::backtest {

/// Singleton service dispatching backtesting commands to Python providers.
class BacktestingService : public QObject {
    Q_OBJECT
  public:
    static BacktestingService& instance();

    /// Execute a backtesting command on a specific provider
    void execute(const QString& provider, const QString& command,
                 const QJsonObject& args);

    /// List available strategies from Fincept provider (dynamic)
    void list_strategies();

  signals:
    void result_ready(QString provider, QString command, QJsonObject data);
    void strategies_loaded(QJsonObject strategies);
    void error_occurred(QString context, QString message);

  private:
    explicit BacktestingService(QObject* parent = nullptr);
    Q_DISABLE_COPY(BacktestingService)
};

} // namespace fincept::services::backtest
