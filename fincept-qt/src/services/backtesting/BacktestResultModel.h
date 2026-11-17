// src/services/backtesting/BacktestResultModel.h
//
// Typed view of the provider results (contract v2: scripts/Analytics/backtesting/base/contract.py),
// parsed ONCE, off the GUI thread, and shared read-only (std::shared_ptr<const ...>).
//
// Units (contract R3): returns, rates, weights, exposure and drawdowns are FRACTIONS (0.12 = 12 %);
// maxDrawdown and drawdowns[].depth are POSITIVE; equity drawdown is <= 0; holding periods are in BARS
// of the run's interval; money is in the account currency = the first symbol's trading currency.
// A value the engine could not supply is NaN here (null in the payload); why_blank() gives the reason.
//
// v1 payloads (no schemaVersion) are read with fallbacks: cagr <- annualizedReturn, timeInMarket <-
// exposureTime / equity exposure / trades, drawdowns[] computed from the equity curve, yearly returns
// from monthlyReturns[].yearTotal, attribution grouped from trades[], costs from statistics.totalFees.
//
// Pure QtCore: unit-tested by tests/tst_backtest_result_model.cpp on tests/data/backtesting/*.json.
#pragma once

#include <QHash>
#include <QJsonObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

#include <limits>
#include <memory>

namespace fincept::services::backtest {

inline constexpr double kBtNaN = std::numeric_limits<double>::quiet_NaN();

/// "YYYY-MM-DD" / "YYYY-MM-DD HH:MM:SS" (or with 'T') read as UTC -> epoch ms; NaN when it is not a date.
double bt_time_ms(const QString& s);
/// True for a stamp with a time of day other than 00:00:00.
bool bt_is_intraday(const QString& s);

struct BtWarning {
    QString code;     ///< SYNTHETIC_DATA, SHORT_HISTORY, NO_COSTS, FEW_TRADES, ...
    QString severity; ///< info | warn | error
    QString message;  ///< a sentence
    QString symbol;   ///< empty = run-wide
};
/// error 0, warn 1, info 2 (sort key: most severe first).
int bt_warning_rank(const QString& severity);

/// x = epoch ms, v = value.
struct BtSeries {
    QVector<double> t;
    QVector<double> v;
    bool empty() const { return t.size() < 2; }
};

struct BtTrade {
    QString id;
    QString symbol;
    QString side;        ///< long | short
    QString entry_date;  ///< as sent
    QString exit_date;   ///< empty for an open position
    double entry_t = kBtNaN;
    double exit_t = kBtNaN;
    double entry_price = kBtNaN;
    double exit_price = kBtNaN;  ///< NaN for an open position (mark_price then)
    double mark_price = kBtNaN;
    double quantity = kBtNaN;
    double pnl = kBtNaN;         ///< NET of costs, money (unrealised for an open position)
    double pnl_pct = kBtNaN;     ///< fraction of entry notional
    double gross_pnl = kBtNaN;
    double commission = kBtNaN;  ///< both legs
    double slippage = kBtNaN;
    double borrow = kBtNaN;
    int holding_bars = -1;
    int holding_days = -1;
    double mae = kBtNaN;         ///< <= 0 fraction
    double mfe = kBtNaN;         ///< >= 0 fraction
    QString exit_reason;         ///< signal, stop_loss, take_profit, trailing_stop, end_of_data, rebalance, open, ...
    int entry_bar = -1;
    int exit_bar = -1;
    bool open = false;
    double costs() const;        ///< commission + slippage + borrow (NaN parts skipped; NaN when all are)
};

struct BtDrawdown {
    int rank = 0;
    double depth = kBtNaN;        ///< POSITIVE fraction
    QString peak_date, trough_date, recovery_date; ///< recovery empty = not recovered
    double peak_t = kBtNaN, trough_t = kBtNaN, recovery_t = kBtNaN;
    int to_trough_bars = -1;
    int recovery_bars = -1;       ///< -1 = not recovered
    int duration_bars = -1;
    int duration_days = -1;
    double peak_equity = kBtNaN;
    double trough_equity = kBtNaN;
    bool recovered() const { return !recovery_date.isEmpty(); }
};

struct BtMonthRow {
    int year = 0;
    double months[12] = {kBtNaN, kBtNaN, kBtNaN, kBtNaN, kBtNaN, kBtNaN,
                         kBtNaN, kBtNaN, kBtNaN, kBtNaN, kBtNaN, kBtNaN}; ///< fractions
    double year_total = kBtNaN;
};

struct BtYear {
    int year = 0;
    double strategy = kBtNaN;
    double benchmark = kBtNaN;
    double excess = kBtNaN;
};

struct BtCostYear {
    int year = 0;
    double commission = kBtNaN;
    double slippage = kBtNaN;
    double notional = kBtNaN;
    double turnover = kBtNaN;
};

struct BtAttribution {
    QString symbol;
    int trades = 0;
    double win_rate = kBtNaN;
    double net_pnl = kBtNaN;
    double gross_pnl = kBtNaN;
    double costs = kBtNaN;
    double contribution = kBtNaN; ///< fraction of initial capital
    double avg_hold_bars = kBtNaN;
    double time_in_market = kBtNaN;
    bool computed = false;        ///< grouped from trades (v1)
};

struct BtPosition {
    QString symbol;
    double quantity = kBtNaN;
    double avg_price = kBtNaN;
    double last_price = kBtNaN;
    double market_value = kBtNaN;
    double weight = kBtNaN;
    double unrealized = kBtNaN;
};

struct BtCostModel {
    QString preset;           ///< request | none | generic_bps | us_equity_retail | ...
    QString source;           ///< sentence
    QString commission_type;  ///< pct_notional | per_share | per_trade
    double commission_value = kBtNaN;
    double min_commission = kBtNaN;
    QString slippage_type;    ///< pct_price | vol_scaled | none
    double slippage_value = kBtNaN;
    double spread_bps = kBtNaN;
    double borrow_rate = kBtNaN;
    double financing_rate = kBtNaN;
    QString day_count;
    bool itemised_commission = true;
    bool itemised_slippage = true;
    bool itemised_borrow = true;
    bool present = false;
};

struct BtCosts {
    bool present = false;              ///< the v2 costs{} block exists
    double total_commission = kBtNaN;
    double total_slippage = kBtNaN;    ///< NaN when not itemised (inside the fill prices)
    double total_borrow = kBtNaN;
    double total_costs = kBtNaN;
    double gross_pnl = kBtNaN;
    double net_pnl = kBtNaN;
    double cost_drag = kBtNaN;         ///< annualised return lost to costs (fraction)
    double traded_notional = kBtNaN;
    double annual_turnover = kBtNaN;
    double pct_of_gross_profit = kBtNaN;
    int fills = -1;
    QVector<BtCostYear> by_year;
    BtCostModel model;
};

struct BtProvSymbol {
    QString symbol;
    QString currency;
    QString source;
    QString first_bar;
    QString last_bar;
    QString requested_start;
    QString cache_as_of;
    int bars = -1;
    int missing_bars = -1;
};

struct BtProvenance {
    bool present = false;
    QString engine;              ///< vectorbt | backtestingpy | bt | zipline | fincept
    QString engine_implementation;
    QString engine_version;
    QString library;
    QString library_version;
    QString python_version;
    QString run_id;
    QString started_at;          ///< ISO UTC
    qint64 duration_ms = -1;
    QString data_source;         ///< yahoo | broker:<id> | synthetic
    QString adjustment;          ///< split+dividend | split | none
    QString interval;
    int bars = -1;
    QString first_bar;
    QString last_bar;
    QString requested_start;
    QString requested_end;
    int warmup_bars = -1;
    QString strategy_type;
    QString strategy_name;
    QJsonObject parameters;      ///< as used
    QString currency;
    double seed = kBtNaN;
    QVector<BtProvSymbol> symbols;
};

struct BtAssumptions {
    QString engine;              ///< sentence
    QString execution;           ///< sentence
    QString data_source;
    QString costs;
    QString position_sizing;
    QString annualization;
    QString survivorship;
    QString warm_up;
    double risk_free_rate = kBtNaN;   ///< fraction (average when a series was used)
    QString risk_free_source;         ///< "^IRX", "rate", ...
    QString risk_free_description;
    QString fill_signal_at;           ///< close
    QString fill_at;                  ///< next_open | next_close | same_close (empty = not reported)
    QString fill_description;
    QStringList not_modelled;
    QStringList ignored_settings;
    QStringList notes;
};

/// A parsed backtest (run_backtest / execute_fincept_strategy).
struct BtRun {
    int schema_version = 1;
    QString id;
    QString status;
    QString engine;                   ///< slug
    QString engine_implementation;
    QString engine_version;
    bool synthetic = false;
    bool intraday = false;
    double periods_per_year = kBtNaN;
    QString currency;                 ///< ISO code of the money (provenance.currency / first symbol), may be empty
    QString benchmark_symbol;

    /// Every numeric scalar of performance, statistics, tradeStats, advancedMetrics, periodStats,
    /// exposure, costs, benchmark, drawdownAnalysis and returnsAnalysis, keyed "<block>.<key>"
    /// ("performance.sharpeRatio"); booleans as 1 / 0. v1 fallbacks are written into the v2 keys.
    QHash<QString, double> metrics;
    /// leaf metric key -> why it is blank (undefinedMetrics{} of the payload, plus the parser's own).
    QHash<QString, QString> undefined;

    // Equity curve (one row per bar). Absent columns are empty vectors.
    QVector<double> t;                ///< epoch ms
    QVector<double> equity;
    QVector<double> drawdown;         ///< <= 0
    QVector<double> returns;          ///< per bar
    QVector<double> benchmark;        ///< account-currency scale; NaN gaps; empty when none
    QVector<double> exposure;         ///< gross exposure / equity; empty when not reported
    QVector<double> net_exposure;
    QVector<double> cash;
    QStringList dates;                ///< the equity rows' date strings

    QVector<BtTrade> trades;          ///< closed in entry order, open positions last
    int open_trades = 0;
    QVector<BtDrawdown> drawdowns;    ///< top 10 by depth
    bool drawdowns_computed = false;  ///< computed here from the equity curve (v1)
    QVector<BtMonthRow> monthly;
    QVector<BtMonthRow> benchmark_monthly;
    QVector<BtYear> yearly;
    bool yearly_computed = false;
    int rolling_window = 0;
    BtSeries rolling_sharpe;
    BtSeries rolling_volatility;
    BtSeries rolling_beta;
    BtSeries rolling_drawdown;
    QVector<BtAttribution> attribution;
    QVector<BtPosition> positions;
    BtCosts costs;
    BtProvenance provenance;
    BtAssumptions assumptions;
    QVector<BtWarning> warnings;      ///< most severe first
    QStringList logs;
    /// Disclosures of data the engine derived instead of reading quotes (Fincept engine: model-priced
    /// options, derived futures contracts, universe selection with survivorship bias, custom data).
    QStringList data_disclosures;
    /// The payload as received (implicitly shared; priceData, fills and exports read it).
    QJsonObject payload;

    /// NaN when absent / null.
    double metric(const QString& path) const;
    bool has_metric(const QString& path) const;
    /// Why `path`'s value is blank ("performance.alpha" -> "No benchmark in this run"); empty when it is not blank.
    QString why_blank(const QString& path) const;
    /// Symbols the run traded (provenance, else the trades), first one first.
    QStringList symbols() const;
    double initial_capital() const;
    double final_equity() const;
    /// Years between the first and last bar (calendar), NaN when unknown.
    double years() const;
    int closed_trades() const;
    bool has_benchmark() const { return !benchmark.isEmpty(); }
    bool has_exposure() const { return !exposure.isEmpty(); }
    /// warnings[] count of severity warn + error.
    int serious_warning_count() const;
};
using BtRunPtr = std::shared_ptr<const BtRun>;

BtRun parse_backtest(const QJsonObject& data);

/// One evaluated parameter set of an optimisation.
struct BtOptRow {
    QJsonObject params;
    double objective = kBtNaN;
    QHash<QString, double> perf; ///< totalReturn, cagr, sharpeRatio, sortinoRatio, calmarRatio, maxDrawdown, volatility, totalTrades, winRate, profitFactor
    double perf_value(const QString& key) const { return perf.value(key, kBtNaN); }
};

struct BtOptimizeResult {
    int schema_version = 1;
    QString objective;
    QString method;
    int iterations = 0;
    int total_combinations = 0;
    bool truncated = false;
    double seed = kBtNaN;
    QString sampling;
    QJsonObject best_params;
    double best_objective = kBtNaN;
    QHash<QString, double> best_perf;
    QVector<BtOptRow> rows;              ///< best first
    QStringList param_names;             ///< varying parameters
    QHash<QString, QVector<double>> param_grid;
    QHash<QString, double> sensitivity;  ///< name -> correlation with the objective
    QHash<QString, double> spread;       ///< name -> objective spread across its values
    double plateau_score = kBtNaN;
    int neighbours = -1;
    // overfitting{}
    bool has_overfitting = false;
    int n_trials = -1;
    double sharpe_std = kBtNaN;
    double expected_max_sharpe = kBtNaN;
    double best_sharpe = kBtNaN;
    double deflated_sharpe = kBtNaN;     ///< probability the best set's true Sharpe > 0 after deflation
    double probabilistic_sharpe = kBtNaN;
    double pbo = kBtNaN;                 ///< probability of backtest overfitting
    double prob_oos_loss = kBtNaN;
    double effective_trials = kBtNaN;
    QString pbo_reason;                  ///< why PBO is null
    QString overfitting_method;
    // holdout{}
    bool has_holdout = false;
    QString holdout_split_date;
    double holdout_fraction = kBtNaN;
    QJsonObject holdout_best_params;
    QHash<QString, double> holdout_in_sample;
    QHash<QString, double> holdout_out_sample;
    // statisticalSignificance{} (legacy, shown only without overfitting{})
    bool has_significance = false;
    QString significance_method;
    double p_value = kBtNaN;
    bool significant = false;
    QString significance_note;
    int workers = -1;
    double elapsed_sec = kBtNaN;
    QVector<BtWarning> warnings;
    QStringList logs;
    BtProvenance provenance;
    BtAssumptions assumptions;
    QJsonObject payload;
};
using BtOptimizePtr = std::shared_ptr<const BtOptimizeResult>;
BtOptimizeResult parse_optimize(const QJsonObject& data);

/// Objective surface over two parameters: cell = best value of `metric` over the other parameters
/// (rows = y values, cols = x values); NaN where no set was evaluated. `metric` is "objective" or a
/// performance key ("sharpeRatio", "totalReturn", "maxDrawdown"...). With x == y (one parameter) the
/// surface has one row.
struct BtSurface {
    QString x_param;
    QString y_param;
    QVector<double> x_values;
    QVector<double> y_values;
    QVector<QVector<double>> values; ///< [row = y][col = x]
    int best_row = -1;
    int best_col = -1;
    bool higher_is_better = true;
};
BtSurface optimize_surface(const BtOptimizeResult& r, const QString& x_param, const QString& y_param,
                           const QString& metric = QStringLiteral("objective"));

struct BtWfWindow {
    int window = 0;
    QString train_start, train_end, test_start, test_end;
    int train_bars = -1;
    int test_bars = -1;
    QJsonObject best_params;
    double is_return = kBtNaN;
    double is_sharpe = kBtNaN;
    double oos_return = kBtNaN;
    double oos_sharpe = kBtNaN;
    double oos_max_dd = kBtNaN;
    double oos_trades = kBtNaN;
    double oos_cagr = kBtNaN;
};

struct BtWalkForwardResult {
    int schema_version = 1;
    QString mode;            ///< rolling | anchored | empty (not reported)
    QString objective;
    int n_windows = 0;
    int purge_bars = -1;
    int embargo_bars = -1;
    int combinations_per_window = -1;
    QVector<BtWfWindow> windows;
    BtSeries oos_equity;
    double oos_coverage = kBtNaN;          ///< share of the period that is out-of-sample
    double avg_oos_return = kBtNaN;
    double avg_oos_sharpe = kBtNaN;
    double avg_is_sharpe = kBtNaN;
    double oos_return_std = kBtNaN;
    double compounded_oos_return = kBtNaN;
    double oos_sharpe = kBtNaN;
    double robustness = kBtNaN;            ///< share of windows with OOS return > 0
    double degradation = kBtNaN;           ///< mean OOS Sharpe / mean IS Sharpe
    double wf_efficiency = kBtNaN;         ///< annualised OOS return / annualised IS return
    /// name -> (value per window, std across windows)
    QHash<QString, QPair<QVector<double>, double>> param_stability;
    int workers = -1;
    QVector<BtWarning> warnings;
    QStringList logs;
    BtProvenance provenance;
    BtAssumptions assumptions;
    QJsonObject payload;
};
using BtWalkForwardPtr = std::shared_ptr<const BtWalkForwardResult>;
BtWalkForwardResult parse_walk_forward(const QJsonObject& data);

struct BtMonteCarloResult {
    int schema_version = 1;
    QString method;          ///< label as sent
    QString method_id;       ///< trade_bootstrap | trade_shuffle | returns_bootstrap | block_bootstrap
    int n_simulations = 0;
    double seed = kBtNaN;
    double block_size = kBtNaN;
    int horizon_bars = -1;
    int n_trades = -1;
    double initial_capital = kBtNaN;
    QVector<double> x;       ///< step index
    QStringList x_dates;     ///< optional dates of x
    QVector<double> p5, p25, p50, p75, p95; ///< equity percentile bands on x
    QVector<double> original;               ///< the realised path on x
    QVector<QVector<double>> sample_paths;
    QVector<double> final_values;           ///< one per simulation (<= 2,000)
    QVector<double> drawdown_values;        ///< max drawdown per simulation (POSITIVE fractions)
    QHash<QString, double> stats;           ///< finalP5 .. cagrP95, probLoss, probRuin, ruinLevel, ddP50 ...
    QHash<QString, double> backtest;        ///< summary of the resampled run
    QVector<BtWarning> warnings;
    QStringList logs;
    BtProvenance provenance;
    BtAssumptions assumptions;
    QJsonObject payload;
    double stat(const QString& key) const { return stats.value(key, kBtNaN); }
};
using BtMonteCarloPtr = std::shared_ptr<const BtMonteCarloResult>;
BtMonteCarloResult parse_monte_carlo(const QJsonObject& data);

/// Shared readers (also used by the screen for research payloads).
BtProvenance parse_provenance(const QJsonObject& prov);
BtAssumptions parse_assumptions(const QJsonObject& a);
QVector<BtWarning> parse_warnings(const QJsonValue& v);

/// Every `step`-th point so at most ~max_points remain (first and last kept) — for display only.
void bt_thin(const QVector<double>& x, const QVector<double>& y, int max_points, QVector<double>* out_x,
             QVector<double>* out_y);

} // namespace fincept::services::backtest
