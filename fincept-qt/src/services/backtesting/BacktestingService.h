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
    void execute(const QString& provider, const QString& command, const QJsonObject& args);

    /// Load strategies dynamically from any provider
    void load_strategies(const QString& provider);

    /// Load command option lists (label_types, splitter_types, etc.) for a provider
    void load_command_options(const QString& provider);

    /// Legacy: load strategies from fincept provider only
    void list_strategies();

    /// Store a portfolio config for BacktestingScreen to pick up on next show.
    void set_pending_portfolio_config(const QJsonObject& config);
    /// Take (and clear) the pending config. Returns empty if none pending.
    QJsonObject take_pending_portfolio_config();

  signals:
    void result_ready(QString provider, QString command, QJsonObject data);
    void strategies_loaded(QJsonObject strategies);
    void command_options_loaded(QString provider, QJsonObject options);
    void error_occurred(QString context, QString message);

  private:
    explicit BacktestingService(QObject* parent = nullptr);
    Q_DISABLE_COPY(BacktestingService)

    /// Run the provider script via PythonRunner (the pre-broker-injection dispatch).
    void dispatch_python(const QString& provider, const QString& command, const QJsonObject& args);

    QJsonObject pending_portfolio_config_;
};

} // namespace fincept::services::backtest
