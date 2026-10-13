#pragma once
// ArenaMetrics — leaderboard performance statistics for one agent, from its
// per-round equity snapshots. Pure functions (selftest-covered).
//
// Conventions, stated once because they decide what the numbers mean:
//   • Return    = (equity − starting capital) / starting capital, in %.
//   • Max DD    = largest peak-to-trough fall as a % of the running peak; the
//                 starting capital is the first peak, so an agent that only
//                 ever lost money shows its full loss as drawdown. Reported ≥ 0.
//   • Sharpe    = mean / sample-stdev of the per-round simple returns between
//                 consecutive snapshots, annualised by sqrt(rounds per year)
//                 with rounds per year taken from the snapshots' ACTUAL average
//                 spacing (cadence changes and pauses included), 365-day year
//                 (crypto perps trade around the clock), risk-free rate 0.
//                 Undefined — shown as "—" — until kArenaMinSharpeReturns
//                 returns exist or when returns have zero dispersion: a Sharpe
//                 from three data points is noise presented as a statistic.

#include <QPointF>
#include <QVector>

#include <algorithm>
#include <cmath>

namespace fincept::arena {

inline constexpr int kArenaMinSharpeReturns = 10;

struct ArenaPerformance {
    double return_pct = 0;
    double max_drawdown_pct = 0;
    double sharpe = 0;
    bool sharpe_valid = false;
    int returns_used = 0; // per-round returns behind the Sharpe estimate
};

/// `snapshots`: x = timestamp (ms since epoch), y = equity, oldest first.
/// `current_equity`: live equity when the competition is running (counts for
/// return and drawdown, not for Sharpe — its interval is not a full round);
/// pass NaN when unknown.
inline ArenaPerformance compute_arena_performance(const QVector<QPointF>& snapshots, double capital,
                                                  double current_equity) {
    ArenaPerformance p;
    if (capital <= 0)
        return p;

    // Return: live equity when known, else the last settled round, else flat.
    double last = capital;
    if (!snapshots.isEmpty())
        last = snapshots.last().y();
    if (std::isfinite(current_equity))
        last = current_equity;
    p.return_pct = (last - capital) / capital * 100.0;

    // Max drawdown over capital → snapshots → live equity.
    double peak = capital;
    double worst = 0;
    auto visit = [&peak, &worst](double e) {
        peak = std::max(peak, e);
        if (peak > 0)
            worst = std::max(worst, (peak - e) / peak);
    };
    for (const auto& s : snapshots)
        visit(s.y());
    if (std::isfinite(current_equity))
        visit(current_equity);
    p.max_drawdown_pct = worst * 100.0;

    // Sharpe from consecutive snapshots.
    QVector<double> rets;
    rets.reserve(snapshots.size());
    for (int i = 1; i < snapshots.size(); ++i) {
        const double prev = snapshots[i - 1].y();
        if (prev > 0)
            rets.append(snapshots[i].y() / prev - 1.0);
    }
    p.returns_used = int(rets.size());
    if (rets.size() < kArenaMinSharpeReturns)
        return p;
    double mean = 0;
    for (double r : rets)
        mean += r;
    mean /= double(rets.size());
    double var = 0;
    for (double r : rets)
        var += (r - mean) * (r - mean);
    var /= double(rets.size() - 1);
    const double sd = std::sqrt(var);
    const double span_ms = snapshots.last().x() - snapshots.first().x();
    if (sd <= 1e-12 || span_ms <= 0)
        return p;
    const double avg_interval_ms = span_ms / double(snapshots.size() - 1);
    const double periods_per_year = 365.0 * 24.0 * 3600.0 * 1000.0 / avg_interval_ms;
    p.sharpe = mean / sd * std::sqrt(periods_per_year);
    p.sharpe_valid = std::isfinite(p.sharpe);
    return p;
}

} // namespace fincept::arena
