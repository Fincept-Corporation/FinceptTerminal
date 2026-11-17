// src/algo_engine/PerformanceMetrics.h
#pragma once
// PerformanceMetrics — the one set of performance definitions used by the
// backtester AND the live / paper monitor, so a deployment's numbers mean the
// same thing as its backtest's.
//
// Pure computation, Qt Core only (unit-tested in tests/tst_algo_performance_metrics.cpp).
//
// Definitions
//   period returns   r_i = E_i / E_{i-1} - 1 over consecutive curve points
//   volatility       sample standard deviation (n - 1) of r, × sqrt(periods/yr)
//   Sharpe           mean(r - rf_p) / sd(r) × sqrt(periods/yr), where rf_p is the
//                    annual risk-free rate compounded down to one period
//   Sortino          mean(r - rf_p) / downside deviation × sqrt(periods/yr), the
//                    downside deviation being sqrt(mean(min(0, r - rf_p)^2)) over
//                    ALL periods (the usual target semideviation)
//   CAGR             (E_end / E_start)^(1 / years) - 1, years = calendar time
//                    between the first and the last point / 365.25 days
//   Calmar           CAGR / |max drawdown| (both as fractions)
//   max drawdown     largest fall from a running peak of the FULL-resolution curve
//   DD duration      longest stretch from a peak until the curve regains it (or
//                    the end), in points and calendar days
//   expectancy       mean net P&L per closed trade (= win% × avg win − loss% × avg loss)
//   profit factor    gross profit / gross loss of net trade P&L (infinite when
//                    there is no loss: `profit_factor_infinite`)
//
// A curve point may carry the exchange trading date (`day`, yyyymmdd); when it
// is 0 the UTC date of `t_ms` is used. Monthly returns are bucketed by that day.

#include <QDate>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace fincept::algo::perf {

struct Point {
    qint64 t_ms = 0; ///< epoch ms
    double v = 0;    ///< equity (or cumulative P&L for live curves)
    int day = 0;     ///< exchange trading date as yyyymmdd; 0 = UTC date of t_ms
};

struct ClosedTrade {
    qint64 entry_ms = 0;
    qint64 exit_ms = 0;
    double net_pnl = 0;   ///< after fees and slippage
    double gross_pnl = 0; ///< price move only
    double fees = 0;
    double ret_pct = 0;   ///< net P&L over entry notional, percent
    int bars_held = 0;
    double mae_pct = 0;   ///< maximum adverse excursion, percent of entry (<= 0)
    double mfe_pct = 0;   ///< maximum favourable excursion, percent of entry (>= 0)
};

struct Drawdown {
    double max_pct = 0;     ///< largest peak-to-trough fall, percent (>= 0)
    double max_abs = 0;     ///< the same fall in currency (>= 0)
    int peak = -1;          ///< index of the peak before the deepest trough
    int trough = -1;        ///< index of the deepest trough
    int recovery = -1;      ///< first index back at the peak after the trough; -1 = not recovered
    int longest_points = 0; ///< longest time under water, in points
    double longest_days = 0;
    qint64 longest_start_ms = 0;
    qint64 longest_end_ms = 0; ///< end of the stretch (recovery, or the last point)
};

struct Ratios {
    int n = 0;              ///< number of period returns
    double mean = 0;        ///< mean period return (fraction)
    double stdev = 0;       ///< sample standard deviation (fraction)
    double ann_vol_pct = 0; ///< annualised volatility, percent
    double sharpe = 0;
    double sortino = 0;
};

struct TradeStats {
    int trades = 0;
    int wins = 0;
    int losses = 0; ///< net P&L < 0 (break-even trades are neither)
    double win_rate_pct = 0;
    double gross_profit = 0; ///< sum of winning net P&L
    double gross_loss = 0;   ///< sum of |losing net P&L|
    double net_pnl = 0;
    double total_fees = 0;
    double avg_win = 0;
    double avg_loss = 0; ///< positive number
    double largest_win = 0;
    double largest_loss = 0; ///< positive number
    double payoff_ratio = 0; ///< avg win / avg loss
    double profit_factor = 0;
    bool profit_factor_infinite = false;
    double expectancy = 0;     ///< net P&L per trade
    double expectancy_pct = 0; ///< mean ret_pct per trade
    double avg_bars_held = 0;
    double avg_mae_pct = 0;
    double avg_mfe_pct = 0;
    int max_consecutive_wins = 0;
    int max_consecutive_losses = 0;
};

/// yyyymmdd key of a date (0 for an invalid date) and back.
int day_key(const QDate& d);
QDate day_from_key(int key);
/// The point's trading date (its `day`, else the UTC date of t_ms).
QDate point_date(const Point& p);

QVector<double> period_returns(const QVector<Point>& curve);
/// Drawdown of each point from the running peak, percent (<= 0).
QVector<double> underwater_pct(const QVector<Point>& curve);
Drawdown drawdown(const QVector<Point>& curve);

/// Sharpe / Sortino / volatility of period returns. `periods_per_year` <= 0 → 252.
Ratios ratios(const QVector<double>& returns, double periods_per_year, double risk_free_annual_pct);

/// The returns the risk ratios of a curve are measured on.
struct RatioBasis {
    QVector<double> returns;     ///< period returns (fractions)
    double periods_per_year = 0; ///< annualisation of those periods
    bool daily = false;          ///< true: returns of the trading-day closes of an intraday curve
};
/// An intraday curve spanning at least `min_days` trading days is measured on its trading-day
/// closes (daily_close), annualised with `trading_days_per_year` (<= 0 → 252) — the convention of
/// every other Sharpe in the terminal (Backtesting tab, portfolio analytics): daily returns ×
/// √trading days. Bar returns of an intraday session mix 5-minute moves with the overnight gap
/// and pick up the bar-to-bar autocorrelation of whatever the strategy does, so their √(bars per
/// year) scaling is not comparable. A daily-or-longer curve, or a short intraday one, is measured
/// on its own points, annualised with `periods_per_year`.
RatioBasis ratio_basis(const QVector<Point>& curve, bool intraday, double periods_per_year,
                       double trading_days_per_year, int min_days = 20);

/// Calendar years between two instants (365.25-day years).
double years_between(qint64 t0_ms, qint64 t1_ms);
/// CAGR in percent; NaN when the span or the start value is not positive.
double cagr_pct(double start_value, double end_value, qint64 t0_ms, qint64 t1_ms);

/// Points per calendar year implied by the curve's own spacing (fallback when no
/// venue calendar is known); 0 for fewer than two points.
double estimate_periods_per_year(const QVector<Point>& curve);

/// The last point of each trading day, in order.
QVector<Point> daily_close(const QVector<Point>& curve);

/// Month (yyyymm key) → return in percent, from the curve's month-end values;
/// the first month is measured from `base_value` (initial capital).
QVector<QPair<int, double>> monthly_returns(const QVector<Point>& curve, double base_value);

TradeStats trade_stats(const QVector<ClosedTrade>& trades);

/// TradeStats as the flat JSON keys the reports read (win_rate, profit_factor, ...).
QJsonObject trade_stats_json(const TradeStats& s);

} // namespace fincept::algo::perf
