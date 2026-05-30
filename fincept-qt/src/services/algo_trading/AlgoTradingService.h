// src/services/algo_trading/AlgoTradingService.h
#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QObject>

namespace fincept::services::algo {

/// Singleton service for Algo Trading — strategy CRUD and native C++ backtesting.
/// Deployment lifecycle lives in AlgoEngine; scanning in AlgoScanner.
class AlgoTradingService : public QObject {
    Q_OBJECT
  public:
    static AlgoTradingService& instance();

    // ── Strategy CRUD ───────────────────────────────────────────────────────
    void save_strategy(const AlgoStrategy& strategy);
    void list_strategies();
    void delete_strategy(const QString& id);

    // Deployment lifecycle is now in AlgoEngine (src/algo_engine/AlgoEngine.h).

    // ── Backtesting (native C++ engine) ──────────────────────────────────────
    // Backtests the given strategy's current conditions directly (no DB round-trip),
    // so unsaved builder edits are testable. Candles come from the connected broker
    // when one is connected, otherwise native Yahoo Finance.
    void run_backtest(const fincept::services::algo::AlgoStrategy& strategy, const QString& symbol,
                      const QString& start_date, const QString& end_date, double capital);

    // Scanner is now in AlgoScanner (src/algo_engine/AlgoScanner.h).

  signals:
    void strategy_saved(QString id);
    void strategies_loaded(QVector<fincept::services::algo::AlgoStrategy> strategies);
    void strategy_deleted(QString id);
    void backtest_result(QJsonObject data);
    void error_occurred(QString context, QString message);

  private:
    explicit AlgoTradingService(QObject* parent = nullptr);
    void seed_library(); // idempotently seeds the curated C++ DSL library
    Q_DISABLE_COPY(AlgoTradingService)
};

} // namespace fincept::services::algo
