// src/services/algo_trading/AlgoTradingService.h
#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QObject>

#include <atomic>
#include <memory>

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
    // when one is connected, otherwise native Yahoo Finance. The engine runs on a
    // worker thread: backtest_started → backtest_progress* → exactly one of
    // backtest_result / backtest_cancelled / error_occurred("backtest", …).
    // `exchange_hint` names the listing venue of a bare symbol ("NSE", "NASDAQ",
    // "LSE"); empty = the connected broker's first exchange, else the user's market
    // region (Settings) — never an implicit NSE.
    void run_backtest(const fincept::services::algo::AlgoStrategy& strategy, const QString& symbol,
                      const QString& start_date, const QString& end_date, double capital,
                      const QString& exchange_hint = {});
    /// Cancel the backtest in flight (fetch or run). Emits backtest_cancelled.
    void cancel_backtest();
    bool backtest_running() const { return backtest_running_; }

    /// Parameter sweep: stop-loss × take-profit grid on the IN-SAMPLE part of the
    /// range (the first 1 − oos_fraction of the bars), best cell by in-sample
    /// Sharpe, then that cell re-run on the untouched OUT-OF-SAMPLE tail. Same data,
    /// costs and fill model as run_backtest(); worker thread; cancellable.
    /// optimization_result: {sl: [..], tp: [..], net_return / sharpe / max_drawdown /
    /// trades: [[row = sl][col = tp]], best: {sl, tp, in_sample: {..}, out_of_sample: {..}},
    /// split_time, bars_in_sample, bars_out_of_sample}.
    void run_optimization(const fincept::services::algo::AlgoStrategy& strategy, const QString& symbol,
                          const QString& start_date, const QString& end_date, double capital,
                          const QVector<double>& stop_losses, const QVector<double>& take_profits,
                          double oos_fraction);
    void cancel_optimization();

    /// Exchange hint run_backtest() would use for `symbol` (see above). GUI thread.
    static QString default_exchange_hint(const QString& symbol);

    /// The compact summary persisted as a strategy's last backtest: headline
    /// metrics, the run's inputs and a thinned equity curve (<= 120 points).
    static QJsonObject backtest_summary(const QJsonObject& result);

    // Scanner is now in AlgoScanner (src/algo_engine/AlgoScanner.h).

  signals:
    void strategy_saved(QString id);
    void strategies_loaded(QVector<fincept::services::algo::AlgoStrategy> strategies);
    void strategy_deleted(QString id);
    void backtest_started(QString symbol, QString yahoo_symbol);
    void backtest_progress(int percent, QString stage);
    void backtest_result(QJsonObject data);
    void backtest_cancelled();
    /// A strategy's persisted last-backtest summary changed.
    void strategy_backtest_saved(QString strategy_id, QJsonObject summary);
    void optimization_progress(int percent, QString stage);
    void optimization_result(QJsonObject result);
    void optimization_failed(QString message);
    void error_occurred(QString context, QString message);

  private:
    explicit AlgoTradingService(QObject* parent = nullptr);
    void seed_library(); // idempotently seeds the curated C++ DSL library
    void persist_last_backtest(const QString& strategy_id, const QJsonObject& result);
    Q_DISABLE_COPY(AlgoTradingService)

    int backtest_seq_ = 0; // latest run_backtest() request; older async results are dropped
    bool backtest_running_ = false;
    std::shared_ptr<std::atomic<bool>> backtest_cancel_;
    int optimize_seq_ = 0;
    std::shared_ptr<std::atomic<bool>> optimize_cancel_;
};

} // namespace fincept::services::algo
