// src/algo_engine/BacktestEngine.h
#pragma once
#include "algo_engine/AlgoEngineTypes.h"
#include "algo_engine/ExecutionModel.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include <atomic>
#include <functional>

namespace fincept::algo {

/// Everything one backtest run needs besides the candles.
struct BacktestConfig {
    QJsonArray entry_conditions;
    QString entry_logic = QStringLiteral("AND");
    QJsonArray exit_conditions;
    QString exit_logic = QStringLiteral("AND");
    double stop_loss_pct = 0;
    double take_profit_pct = 0;
    double trailing_stop_pct = 0;
    double initial_capital = 100000;
    double position_size_pct = 100; ///< % of equity committed per entry (1..100, no leverage)
    QString timeframe = QStringLiteral("1d"); ///< bar timeframe of `candles` (not "live")
    QString side = QStringLiteral("BUY");     ///< BUY = long entries, SELL = short entries
    CostModel costs;                          ///< commission / taxes / slippage / lot size
    double risk_free_pct = 0;                 ///< annual, percent (Sharpe / Sortino)
    double periods_per_year = 0;              ///< bars per year (MarketSessions); 0 = from the data
    /// Trading days per year of the venue (MarketSessions): annualises the daily returns the
    /// risk ratios of an intraday run are measured on (PerformanceMetrics::ratio_basis); 0 = 252.
    double trading_days_per_year = 0;
    QString symbol;                           ///< labels carried into the report
    QString venue;
    QString currency;
    int warmup_bars = 50;     ///< bars skipped for indicator warm-up before the first evaluation
    bool include_bars = true; ///< emit the evaluated OHLC range for the price chart
    /// Cooperative cancel, polled once per bar (may be null).
    const std::atomic<bool>* cancel = nullptr;
    /// Progress 0..100, called from the running thread (may be empty).
    std::function<void(int)> progress;
};

/// Event-driven, single-symbol backtester (long or short).
///
/// Fill model (no look-ahead):
///   - Entry / exit SIGNALS are evaluated on the close of bar i and fill at the
///     OPEN of bar i+1, moved adversely by the cost model's slippage.
///   - Stop-loss / trailing stop are resting stop orders checked against each
///     bar's range; a bar that gaps through the level fills at its open. The
///     trailing level uses the extreme reached BEFORE the bar (conservative:
///     the bar's own high is not assumed to come before its low). Stops pay
///     slippage; take-profit is a limit order (no slippage, a gap fills at the open).
///   - Commission and taxes come from ExecutionModel; quantities are lot-rounded
///     (fractional when the lot size is < 1). An entry that cannot afford one lot
///     is skipped and reported, never silently dropped.
///   - A short entry credits the sale proceeds; equity marks the short at the close.
///
/// The result is a flat JSON object (see BacktestEngine.cpp for the full key
/// list). On failure it has {"success": false, "error": …} (and "cancelled": true
/// when stopped through `cancel`).
class BacktestEngine {
  public:
    static QJsonObject run(const QVector<OhlcvCandle>& candles, const BacktestConfig& config);

    /// Legacy entry point (long-only, no costs).
    static QJsonObject run(const QVector<OhlcvCandle>& candles, const QJsonArray& entry_conditions,
                           const QString& entry_logic, const QJsonArray& exit_conditions, const QString& exit_logic,
                           double stop_loss_pct, double take_profit_pct, double trailing_stop_pct,
                           double initial_capital, const QString& timeframe, double position_size_pct = 100.0);
};

} // namespace fincept::algo
