#include "services/options/ScenarioMath.h"

#include "services/options/OptionModels.h"

#include <algorithm>
#include <cmath>

namespace fincept::services::options::scenario {

namespace {

/// Normal quantiles of the cone bands: Φ⁻¹(0.02275), Φ⁻¹(0.15866) — exactly −2 and −1.
constexpr double kZ2 = 2.0;
constexpr double kZ1 = 1.0;

} // namespace

ShockResult evaluate(const strategy::Spec& spec, const strategy::Market& m, const Shock& shock) {
    ShockResult out;
    const double base = strategy::position_value(spec, m, m.spot, 0.0, 0.0, 0.0);
    const double S = m.spot * (1.0 + shock.spot_pct / 100.0);
    const double t = std::max(shock.days, 0.0) / 365.0;
    const double dv = shock.vol_pts / 100.0;
    const double dr = shock.rate_bp / 10000.0;
    out.value = strategy::position_value(spec, m, S, t, dv, dr);
    out.pnl = out.value - base;
    // Greeks at the shocked state: each leg aged by t, vol and rate shifted.
    for (const strategy::Leg& l : spec.legs) {
        if (!l.active)
            continue;
        const double u = strategy::leg_units(l, spec.multiplier);
        if (l.kind == strategy::LegKind::Underlying) {
            out.delta += u;
            continue;
        }
        const double tau = l.expiry_years - t;
        if (tau <= 0 || !(S > 0))
            continue;
        const auto g = models::bsm_full(l.kind == strategy::LegKind::Call, S, l.strike, tau, m.r + dr, m.q,
                                        std::max(l.iv + dv, 1e-4));
        if (!g.valid)
            continue;
        out.delta += u * g.delta;
        out.gamma += u * g.gamma;
        out.vega_pt += u * g.vega / 100.0;
        out.theta_day += u * g.theta / 365.0;
    }
    return out;
}

Grid pnl_grid(const strategy::Spec& spec, const strategy::Market& m, const std::vector<double>& spot_pcts,
              const std::vector<double>& vol_pts, double days, double rate_bp, const std::atomic<bool>* cancel) {
    Grid g;
    g.spot_pcts = spot_pcts;
    g.vol_pts = vol_pts;
    const double base = strategy::position_value(spec, m, m.spot, 0.0, 0.0, 0.0);
    const double t = std::max(days, 0.0) / 365.0;
    const double dr = rate_bp / 10000.0;
    g.pnl.reserve(spot_pcts.size());
    for (double sp : spot_pcts) {
        if (cancel && cancel->load(std::memory_order_relaxed)) {
            g.cancelled = true;
            return g;
        }
        std::vector<double> row;
        row.reserve(vol_pts.size());
        const double S = m.spot * (1.0 + sp / 100.0);
        for (double vp : vol_pts)
            row.push_back(strategy::position_value(spec, m, S, t, vp / 100.0, dr) - base);
        g.pnl.push_back(std::move(row));
    }
    return g;
}

std::vector<double> symmetric_axis(double range, int steps) {
    steps = std::clamp(steps, 1, 50);
    std::vector<double> v;
    v.reserve(static_cast<size_t>(2 * steps + 1));
    for (int i = -steps; i <= steps; ++i)
        v.push_back(range * i / steps);
    return v;
}

const std::vector<Preset>& stress_presets() {
    static const std::vector<Preset> kPresets = {
        {"Spot -20%", {-20, 0, 0, 0}},
        {"Spot -10%", {-10, 0, 0, 0}},
        {"Spot +10%", {10, 0, 0, 0}},
        {"Spot +20%", {20, 0, 0, 0}},
        {"Vol crush -10 pts", {0, -10, 0, 0}},
        {"Vol spike +10 pts", {0, 10, 0, 0}},
        {"Rates +100 bp", {0, 0, 100, 0}},
        {"Rates -100 bp", {0, 0, -100, 0}},
        {"One week of decay", {0, 0, 0, 7}},
        {"Crash: -20% and +25 vol pts", {-20, 25, 0, 0}},
        {"Rally: +10% and -5 vol pts", {10, -5, 0, 0}},
    };
    return kPresets;
}

Cone probability_cone(double spot, double r, double q, double sigma, double max_days, int n) {
    Cone c;
    n = std::max(n, 2);
    if (!(spot > 0) || !(sigma > 0) || !(max_days > 0))
        return c;
    for (int i = 0; i < n; ++i) {
        const double d = max_days * i / (n - 1);
        const double t = d / 365.0;
        const double mu = std::log(spot) + (r - q - 0.5 * sigma * sigma) * t;
        const double sd = sigma * std::sqrt(t);
        c.days.push_back(d);
        c.p2.push_back(std::exp(mu - kZ2 * sd));
        c.p16.push_back(std::exp(mu - kZ1 * sd));
        c.p50.push_back(std::exp(mu));
        c.p84.push_back(std::exp(mu + kZ1 * sd));
        c.p98.push_back(std::exp(mu + kZ2 * sd));
    }
    return c;
}

} // namespace fincept::services::options::scenario
