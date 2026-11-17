// src/services/portfolio/PortfolioPerformance.h
//
// Realised performance of a book from its NAV history and its external flows:
// time-weighted return (TWR), money-weighted return (XIRR), period windows and
// the risk/return statistics every portfolio view shows. Pure (Qt Core only), so
// it is unit-testable and runs on a worker thread.
//
// TWR flow timing (one rule for every period, Modified Dietz day weights):
//   r = (V1 − V0 − ΣF) / (V0 + Σ wᵢ·Fᵢ)
//   inflow  (F > 0): wᵢ = min(1, (days(dᵢ→d1) + 1) / L)  — enters at the START of its day
//   outflow (F < 0): wᵢ = days(dᵢ→d1) / L                 — leaves at the END of its day
//   L = calendar days d0 → d1. For consecutive snapshots this is exact for a trade at
//   its own price: a first BUY of q at p closing at c returns c/p − 1, and a sale's
//   proceeds count until the close.
//   A period whose denominator is ≤ 0 has no capital at risk: its return is undefined
//   and it is marked invalid (perf_excluded). A period with |r| > 25 % is KEPT and
//   flagged `suspect` (likely an unrecorded deposit/withdrawal or a bad price).
//
// Statistics (see ComputedMetrics in PortfolioTypes.h for the definitions): the
// TWR periods are compounded onto the trading days of one calendar venue (the
// benchmark's), annualised with that venue's trading days per year.
#pragma once

#include "screens/portfolio/PortfolioTypes.h"
#include "services/portfolio/PortfolioLots.h"

#include <QDate>
#include <QPair>
#include <QStringList>
#include <QVector>

#include <functional>
#include <optional>

namespace fincept::portfolio {

/// "MTD", "QTD", "YTD", "1Y", "3Y", "5Y", "ALL".
QString perf_window_key(PerfWindow w);
/// Inverse of perf_window_key (case-insensitive; also "1M"→MTD is NOT accepted). Unknown -> All.
PerfWindow perf_window_from_key(const QString& key);
/// Date whose close is the window's base point (MTD: last day of the previous month,
/// QTD: of the previous quarter, YTD: Dec 31, nY: same date n years back). Invalid for All.
QDate perf_window_base_date(PerfWindow w, const QDate& as_of);

/// Flow-adjusted return series (rules above). Snapshots in any order; one per date
/// (the last for a date wins). Flows outside (first, last] snapshot are ignored.
QVector<TwrPoint> build_twr(const QVector<PortfolioSnapshot>& snapshots, const QVector<ExternalFlow>& flows);

/// Internal rate of return of dated cash flows (investor's view: contributions
/// negative, receipts positive), annual, compounding on 365.25-day years.
/// nullopt when there is no sign change or no root in (-99.99 %, 1e6 %).
std::optional<double> xirr(const QVector<QPair<QDate, double>>& flows);

struct PerfInputs {
    QVector<PortfolioSnapshot> snapshots; ///< whole stored history
    QVector<ExternalFlow> flows;          ///< from replay_ledger()
    PerfWindow window = PerfWindow::All;
    QDate as_of; ///< invalid = last snapshot date

    /// Benchmark level in the base currency, ascending dates (may be empty).
    QStringList benchmark_dates;
    QVector<double> benchmark_levels;

    double risk_free = 0;        ///< annual fraction
    double periods_per_year = 252;
    QString calendar_id;         ///< e.g. "NYSE" — reported only
    /// Observation days. Default: Monday–Friday.
    std::function<bool(const QDate&)> is_trading_day;
};

/// Fills the realised (TWR / MWR / benchmark-relative) fields of ComputedMetrics:
/// sharpe, sortino, realised_volatility, max_drawdown, total_return,
/// annualized_return, calmar, current_drawdown, win_rate, best/worst_period,
/// benchmark_return, excess_return, tracking_error, information_ratio, treynor,
/// r_squared, realised_beta, alpha, up/down_capture, mwr, mwr_period, window,
/// periods_per_year, calendar, perf_* counts and dates, notes, twr_series (window).
/// Leaves every ex-ante / holdings-based field untouched.
void compute_realised_metrics(const PerfInputs& in, ComputedMetrics& m);

/// Minimum observations before vol / Sharpe / Sortino / beta are shown.
inline constexpr int kPerfMinObservations = 20;
/// Minimum observations before max drawdown / win rate are shown.
inline constexpr int kPerfMinDrawdownObservations = 5;
/// Annualised figures only for windows of at least this many calendar days (GIPS).
inline constexpr int kPerfMinAnnualiseDays = 365;
/// |period return| above this is flagged suspect (kept).
inline constexpr double kPerfSuspectReturn = 0.25;

} // namespace fincept::portfolio
