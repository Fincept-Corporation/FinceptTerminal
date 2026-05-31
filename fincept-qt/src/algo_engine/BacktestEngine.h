// src/algo_engine/BacktestEngine.h
#pragma once
#include "algo_engine/AlgoEngineTypes.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace fincept::algo {

/// Event-driven, single-symbol, long-only backtester.
///
/// Pure computation: given candles + strategy parameters it returns a FLAT
/// metrics JSON object matching exactly what
/// `StrategyBuilderPanel::display_backtest_result` reads
/// (total_return, sharpe_ratio, max_drawdown, total_trades, win_rate,
///  profit_factor, final_value), plus equity_curve and the raw trade list.
///
/// Fill model (no look-ahead bias):
///   - Entry/exit SIGNALS are evaluated on the close of bar i.
///   - A signal fills at the OPEN of bar i+1.
///   - Stop-loss / take-profit (incl. trailing) are checked intrabar against
///     each bar's high/low, filling at the stop/target price.
///
/// On insufficient data the returned object has {"success": false, "error": …}.
class BacktestEngine {
public:
    static QJsonObject run(const QVector<OhlcvCandle>& candles,
                           const QJsonArray& entry_conditions, const QString& entry_logic,
                           const QJsonArray& exit_conditions, const QString& exit_logic,
                           double stop_loss_pct, double take_profit_pct, double trailing_stop_pct,
                           double initial_capital, const QString& timeframe,
                           double position_size_pct = 100.0);
};

} // namespace fincept::algo
