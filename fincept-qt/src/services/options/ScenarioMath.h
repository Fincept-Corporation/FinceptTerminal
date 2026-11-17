#pragma once
// ScenarioMath — what-if analysis of an option position (StrategyMath::Spec) in pure
// C++ (std only): P&L under a shock (spot %, vol points, rate bp, days forward),
// spot × vol P&L grids, named stress presets and the lognormal probability cone.
// P&L is measured against the position's CURRENT model value (no shock, today), so
// the unshocked cell is exactly 0. Legs are valued with Black-Scholes-Merton at
// their own implied volatility (StrategyMath's model).

#include "services/options/StrategyMath.h"

#include <atomic>
#include <string>
#include <vector>

namespace fincept::services::options::scenario {

struct Shock {
    double spot_pct = 0; ///< −20 = spot down 20 %
    double vol_pts = 0;  ///< +10 = every leg's IV up 10 vol points (floored at 0.01 %)
    double rate_bp = 0;  ///< +100 = rates up 1 %
    double days = 0;     ///< calendar days forward (time decay)
};

struct ShockResult {
    double pnl = 0;   ///< shocked value − current value
    double value = 0; ///< shocked position value
    double delta = 0; ///< shares-equivalent at the shocked state
    double gamma = 0;
    double vega_pt = 0;
    double theta_day = 0;
};

ShockResult evaluate(const strategy::Spec& spec, const strategy::Market& m, const Shock& shock);

struct Grid {
    std::vector<double> spot_pcts; ///< rows
    std::vector<double> vol_pts;   ///< columns
    std::vector<std::vector<double>> pnl; ///< [row][col]
    bool cancelled = false;
};

/// P&L over spot shocks × vol shocks at `days` forward with `rate_bp`. Checks
/// `cancel` between rows (may be null).
Grid pnl_grid(const strategy::Spec& spec, const strategy::Market& m, const std::vector<double>& spot_pcts,
              const std::vector<double>& vol_pts, double days, double rate_bp, const std::atomic<bool>* cancel);

/// Evenly spaced values from −range to +range (`steps` per side; 2·steps + 1 values).
std::vector<double> symmetric_axis(double range, int steps);

struct Preset {
    std::string name;
    Shock shock;
};
/// −20 / −10 / +10 / +20 % spot, vol crush / spike, rates ±100 bp, one week of decay,
/// crash (−20 % and +25 vol points), rally (+10 % and −5 vol points).
const std::vector<Preset>& stress_presets();

struct Cone {
    std::vector<double> days;
    std::vector<double> p2, p16, p50, p84, p98; ///< spot percentiles (±2σ, ±1σ, median)
};
/// Lognormal cone of the underlying from today to `max_days` (n points), drift r − q.
Cone probability_cone(double spot, double r, double q, double sigma, double max_days, int n);

} // namespace fincept::services::options::scenario
