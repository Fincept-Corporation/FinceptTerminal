// src/services/backtesting/BacktestRequest.h
//
// The request the Backtesting screen sends to a provider, built from one typed
// "setup" (engine, strategy + parameters, symbols, period, account, costs,
// execution) plus the settings of the analysis that runs it.
//
// Keys are exactly what the providers read (scripts/Analytics/backtesting):
//   common       symbols[], startDate, endDate, interval, allowSyntheticData, weights[] (hand-off)
//   strategy     initialCapital, commission / slippage (FRACTIONS, read by engines not yet on the shared
//                cost model), costModel {preset | commissionType, commissionValue, slippageType,
//                slippageValue} (base/costs.py), fillTiming next_open|next_close|same_close
//                (base/execution.py), riskFreeSource auto|rate (base/result_normalizer.py: "auto" = the
//                T-bill series for USD runs), riskFreeRate (fraction), benchmarkSymbol, leverage,
//                positionSizing, positionSizeValue, allowShort, stopLoss / takeProfit (fractions, only
//                when set), strategy {type, name, category, params}
//   optimize     optimizeObjective, optimizeMethod, maxIterations, paramRanges {name: {min,max,step}},
//                holdoutFraction, seed, maxWorkers
//   walk_forward wfSplits, wfTrainRatio, wfMode rolling|anchored, purgeBars, embargoBars,
//                optimizeObjective, paramRanges, maxWorkers
//   monte_carlo  mcSimulations, mcMethod trade_bootstrap|trade_shuffle|returns_bootstrap|block_bootstrap,
//                mcSeed, mcBlockSize, mcRuinLevel
//
// Pure QtCore: unit-tested by tests/tst_backtest_request.cpp.
#pragma once

#include <QDate>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::services::backtest {

/// A choice shown in the UI: id sent to the engine, short label, one-line detail.
struct BtChoice {
    QString id;
    QString label;
    QString detail;
};

/// Cost presets of base/costs.py, plus "custom" (the commission / slippage fields).
QVector<BtChoice> cost_presets();
/// The preset the engines pick for a symbol when none is given (mirrors costs.py default_preset_for).
QString default_cost_preset_for(const QString& symbol);
/// Commission / slippage (fractions per fill) a preset stands for — what engines that do not read
/// `costModel` yet receive in `commission` / `slippage`. False for "custom" / unknown ids.
bool cost_preset_fractions(const QString& preset, double* commission, double* slippage);
/// "10 bp + 5 bp slip", "GROSS — no costs", "0.005/sh (min 1) + 2 bp slip".
QString cost_preset_short(const QString& preset, double commission_pct, double slippage_pct);

/// next_open (default) | next_close | same_close.
QVector<BtChoice> fill_timings();
/// "Signal at close → fill at next open".
QString fill_timing_text(const QString& id);

/// auto (T-bill series) | rate (the entered rate).
QVector<BtChoice> risk_free_sources();

/// trade_bootstrap | trade_shuffle | returns_bootstrap | block_bootstrap.
QVector<BtChoice> monte_carlo_methods();

/// Optimisation objectives / methods / position sizing methods.
QVector<BtChoice> optimize_objective_choices();
QVector<BtChoice> optimize_method_choices();
QVector<BtChoice> sizing_method_choices();

/// Everything shared by every analysis of one setup.
struct BtSetup {
    QString engine = QStringLiteral("vectorbt");
    QStringList symbols;
    QDate start;
    QDate end;
    QString interval = QStringLiteral("1d");
    double capital = 100000.0;
    QString benchmark;                                       ///< empty = none
    QString cost_preset = QStringLiteral("us_equity_retail"); ///< preset id, or "custom"
    double commission_pct = 0.10;                            ///< percent of notional per fill (custom)
    double slippage_pct = 0.05;                              ///< percent of price per fill (custom)
    QString fill_timing = QStringLiteral("next_open");
    QString risk_free_source = QStringLiteral("auto");       ///< auto | rate
    double risk_free_pct = 4.0;
    QString sizing = QStringLiteral("percent");
    double sizing_value = 100.0;
    double leverage = 1.0;
    bool allow_short = false;
    double stop_loss_pct = 0.0;   ///< 0 = off
    double take_profit_pct = 0.0; ///< 0 = off
    bool allow_synthetic = false;
    QJsonArray weights;           ///< portfolio hand-off weights (sent when one per symbol)
    QString strategy_id;
    QString strategy_name;
    QString strategy_category;
    QJsonObject strategy_params;  ///< typed values (int / double / bool / string / objects)
};

struct BtOptimizeSettings {
    QString objective = QStringLiteral("sharpe");
    QString method = QStringLiteral("grid");
    int max_iterations = 500;
    double holdout_fraction = 0.0; ///< 0 = no hold-out
    int seed = 7;
    QJsonObject param_ranges;      ///< {name: {min, max, step}}
};

struct BtWalkForwardSettings {
    int windows = 5;
    double train_ratio = 0.7;
    QString mode = QStringLiteral("rolling"); ///< rolling | anchored
    int purge_bars = 0;
    int embargo_bars = 0;
    QString objective = QStringLiteral("sharpe");
    QJsonObject param_ranges;
};

struct BtMonteCarloSettings {
    QString method = QStringLiteral("trade_bootstrap");
    int simulations = 1000;
    int block_size = 20;      ///< bars (block_bootstrap)
    double ruin_level = 0.5;  ///< fraction of initial capital
    int seed = 42;
};

/// Python worker processes an optimisation may use on this machine: min(4, cores - 2), at least 1.
int default_max_workers();

/// The request for a backtest (`command` = "backtest") or the strategy part shared by optimise /
/// walk-forward / Monte Carlo.
QJsonObject build_backtest_request(const BtSetup& setup);
QJsonObject build_optimize_request(const BtSetup& setup, const BtOptimizeSettings& s);
QJsonObject build_walk_forward_request(const BtSetup& setup, const BtWalkForwardSettings& s);
QJsonObject build_monte_carlo_request(const BtSetup& setup, const BtMonteCarloSettings& s);

/// A setup back from a stored request (LOAD INPUTS / RE-RUN of a saved run). Fields the request does
/// not carry keep their defaults; `engine` is not in the request and is left as given.
BtSetup setup_from_request(const QJsonObject& request, const QString& engine);

/// Stable hash of a request (SHA-1 hex of its compact JSON, volatile keys removed: cancelFile,
/// brokerCandles, brokerDataSource, maxWorkers). Equal requests -> equal signatures.
QString request_signature(const QJsonObject& request);
/// Signature of one run: engine + command + request_signature(request).
QString run_signature(const QString& engine, const QString& command, const QJsonObject& request);

/// Validation of a setup before RUN: empty when it can run, else the first problem as a sentence.
QString validate_setup(const BtSetup& setup);

} // namespace fincept::services::backtest
