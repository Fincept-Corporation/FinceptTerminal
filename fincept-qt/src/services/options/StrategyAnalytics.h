#pragma once
// StrategyAnalytics — pure functions that turn a `Strategy` (+ a live chain
// snapshot for current Greeks) into the numbers the F&O Builder ribbon and
// payoff chart need to render.
//
// All math is synchronous and in-process. The async Greeks worker is only
// involved at chain refresh time, when individual leg row.ce_greeks / .pe_greeks
// get filled in; this module reads those, sums them by signed lot_size, and
// computes the rest (target-day curve via OptionPricing, POP via lognormal
// CDF, breakevens via sign-change interpolation).
//
// Pricing model — Phase 5 ships BSM with q=0 for both index and stock
// legs. Calendar / diagonal strategies are out of scope (single-expiry
// only, see StrategyTemplates).

#include "services/options/OptionChainTypes.h"

#include <QVector>

namespace fincept::services::options::analytics {

/// Knobs the caller can tune. All defaults are picker-friendly.
struct PayoffComputeOptions {
    /// Number of curve sample points across [spot_min, spot_max].
    int n_points = 201;

    /// Lower / upper bounds for the spot axis. 0 = auto: `current_spot ± 30%`.
    double spot_min = 0.0;
    double spot_max = 0.0;

    /// Target date for the dashed pre-expiry curve, expressed as calendar
    /// days from "today". 0 = T+0 (today's curve). When this equals or
    /// exceeds days-to-expiry, the target curve collapses onto the expiry
    /// curve.
    int days_to_target = 0;

    /// Risk-free rate (decimal). Default mirrors fno.risk_free_rate.
    double risk_free_rate = 0.067;

    /// IV used to price legs whose `iv_at_entry == 0` (Greeks worker hadn't
    /// solved them by the time they were captured). Decimal.
    double fallback_iv = 0.20;

    /// Current spot — ignored when spot_min/spot_max are explicit, used for
    /// auto-bounds otherwise. Drives breakeven crosshair positioning.
    double current_spot = 0.0;
};

struct MaxPnL {
    double max_profit = 0.0;
    double max_loss = 0.0;          // negative for losing strategies
    bool profit_unbounded = false;
    bool loss_unbounded = false;
};

/// Net premium (₹). Positive = net debit (paid); negative = net credit (received).
double net_premium(const Strategy& s);

/// Sample the payoff curve across `[spot_min, spot_max]` with `n_points`
/// linearly-spaced spots. Each point carries pnl_expiry (intrinsic) and
/// pnl_target (BSM at the target date).
QVector<PayoffPoint> compute_payoff(const Strategy& s, const PayoffComputeOptions& opts);

/// Spots where pnl_expiry crosses zero (sorted ascending). Linear
/// interpolation between consecutive samples.
QVector<double> compute_breakevens(const QVector<PayoffPoint>& curve);

/// Min / max of pnl_expiry across the curve, plus tail-direction flags
/// derived from the strategy's net CE position (calls dominate the
/// asymptotic behaviour on the upside).
MaxPnL compute_max_pnl(const QVector<PayoffPoint>& curve, const Strategy& s);

/// Probability of profit under a GBM model with current spot, time t,
/// risk-free r, and volatility sigma. Integrates the lognormal density
/// over the profitable regions between (and outside) breakevens.
double compute_pop(const Strategy& s, double current_spot, double t, double r, double sigma);

/// Sum each active leg's live Greeks × signed (lots × lot_size). Looks up
/// each leg in `chain` by token; legs with no match contribute 0. Vega
/// and rho remain in the C++ struct's "per 1.00 σ / r" convention.
OptionGreeks combined_greeks(const Strategy& s, const OptionChain& chain);

/// One-shot bundle — fills in everything `StrategyAnalytics` carries.
/// Margin stays 0 in Phase 5 (Phase 6 wires IBroker::get_basket_margins).
StrategyAnalytics compute_all(const Strategy& s, const OptionChain& chain,
                              const PayoffComputeOptions& opts);

} // namespace fincept::services::options::analytics
