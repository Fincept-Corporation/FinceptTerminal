#pragma once
// StrategyMath — multi-leg option strategy analytics for the Derivatives tab, in
// pure C++ (std only): legs on calls, puts and the underlying with their OWN
// expiries (calendars and diagonals), payoff at the first expiry and at any date
// before it, breakevens, maximum profit / loss with unbounded tails, probability
// of profit, aggregated Greeks, net debit / credit, a rule-based margin ESTIMATE,
// and a template catalogue (verticals, straddles, strangles, butterflies, condors,
// calendars, diagonals, ratios, collars, covered calls, protective puts).
//
// It is independent of the F&O tab's broker-chain StrategyAnalytics (single expiry,
// lot sizes, broker tokens), which stays untouched.
//
// Model and assumptions (shown to the user):
//   * every option leg is valued with Black-Scholes-Merton (European) at its own
//     implied volatility, rate r and dividend yield q; at and after its expiry a
//     leg is worth its intrinsic value;
//   * "at expiry" means at the FIRST option expiry of the strategy; legs expiring
//     later are valued with the model at that date;
//   * probability of profit: the underlying at that date is lognormal under the
//     risk-neutral drift (r − q) with the volatility given (the expiry's ATM IV);
//   * the margin is an estimate from Reg-T / Cboe strategy-based rules, not a
//     broker's or exchange's figure.
//
// Units: option quantity in contracts × multiplier (100 for US equity options);
// underlying quantity in shares. Prices per share. P&L in the price currency.

#include <functional>
#include <string>
#include <vector>

namespace fincept::services::options::strategy {

enum class LegKind { Call, Put, Underlying };

struct Leg {
    LegKind kind = LegKind::Call;
    double quantity = 1;     ///< signed: + long / − short (contracts; shares for the underlying)
    double strike = 0;       ///< options only
    double expiry_years = 0; ///< time from valuation to the leg's expiry (options only)
    double entry_price = 0;  ///< premium per share paid (+) or received for a short; underlying: entry price
    double iv = 0.2;         ///< decimal; values the leg before its expiry
    bool active = true;
    int expiry_index = -1;   ///< caller's expiry slot (UI); not used by the maths
};

struct Market {
    double spot = 100;
    double r = 0.04; ///< continuous
    double q = 0.0;  ///< continuous dividend yield
};

struct Spec {
    std::vector<Leg> legs;
    double multiplier = 100;
};

/// Units a leg moves P&L by per 1.00 of its price (contracts × multiplier; shares).
double leg_units(const Leg& leg, double multiplier);
/// Value per share of a leg at spot `S`, `t` years after valuation, with a vol shift.
double leg_value(const Leg& leg, double S, double t, double r, double q, double vol_shift = 0.0);

/// Positive = net debit paid, negative = net credit received (whole position).
double net_premium(const Spec& spec);
/// First option expiry (years) among active option legs; 0 when there is none.
double first_expiry(const Spec& spec);
/// Position P&L against entry prices at spot S, t years from valuation.
double pnl_at(const Spec& spec, const Market& m, double S, double t, double vol_shift = 0.0);
/// Current model value of the position (t = 0), whole position.
double position_value(const Spec& spec, const Market& m, double S, double t = 0, double vol_shift = 0.0,
                      double rate_shift = 0.0);

struct PayoffPoint {
    double spot = 0;
    double pnl_horizon = 0; ///< at the first expiry
    double pnl_target = 0;  ///< at the target date
};

/// Sampled P&L curves over [s_lo, s_hi] (n points, ≥ 2). `t_target` years after valuation.
std::vector<PayoffPoint> payoff_curve(const Spec& spec, const Market& m, double s_lo, double s_hi, int n,
                                      double t_target);

/// Spots where the P&L at the first expiry is zero, refined by bisection on the
/// exact function, ascending, within [s_lo, s_hi].
std::vector<double> breakevens(const Spec& spec, const Market& m, double s_lo, double s_hi);

struct Extremes {
    double max_profit = 0;
    double max_loss = 0; ///< ≤ 0
    bool profit_unbounded = false;
    bool loss_unbounded = false;
    double spot_at_max_profit = 0;
    double spot_at_max_loss = 0;
};

/// Max profit / loss at the first expiry over S ∈ [0, ∞): sampled densely up to well
/// beyond every strike, with the upside tail decided by the position's slope.
Extremes extremes(const Spec& spec, const Market& m);

/// Probability that the P&L at the first expiry is positive, S lognormal with drift
/// r − q and volatility `sigma` over that horizon. Also the expected P&L.
struct Probability {
    double pop = 0;          ///< [0, 1]
    double expected_pnl = 0; ///< under the same measure (undiscounted)
    bool valid = false;
};
Probability probability_of_profit(const Spec& spec, const Market& m, double sigma);

struct PositionGreeks {
    double delta = 0;     ///< shares-equivalent (Δ × units)
    double gamma = 0;     ///< per 1.00 spot, units
    double theta_day = 0; ///< currency per calendar day
    double vega_pt = 0;   ///< currency per 1 vol point
    double rho_pct = 0;   ///< currency per 1 % rate
    double dollar_delta = 0; ///< delta × spot
};
PositionGreeks greeks(const Spec& spec, const Market& m);

struct MarginEstimate {
    double requirement = 0; ///< currency
    std::string method;     ///< "defined risk: maximum loss", "naked short options (20 % rule)", ...
};
/// Reg-T / Cboe strategy-based estimate: defined-risk positions → maximum loss;
/// uncovered short options → the naked formula (premium + max(20 % of underlying −
/// out-of-the-money amount, 10 %), 15 % for broad-based indices; for short
/// straddles / strangles the larger side plus the other side's premium); long
/// options → premium; long / short stock → 50 % of its value.
MarginEstimate margin_estimate(const Spec& spec, const Market& m, bool broad_index);

// ── Templates ────────────────────────────────────────────────────────────────

struct LegRecipe {
    LegKind kind = LegKind::Call;
    double ratio = 1;    ///< signed per unit of the strategy (+ buy, − sell); underlying: × multiplier shares
    int strike_steps = 0; ///< offset from the ATM strike in units of `width` strike steps
    int expiry_slot = 0;  ///< 0 = the chosen expiry, 1 = the next later expiry (calendars)
};

struct Template {
    std::string id;
    std::string name;
    std::string outlook;
    std::vector<LegRecipe> legs;
};

const std::vector<Template>& templates();
const Template* find_template(const std::string& id);

/// What a template needs from a chain: the strikes listed for an expiry slot
/// (ascending), its time to expiry, and a quote (mid, iv) for a contract.
struct ChainAccess {
    std::function<std::vector<double>(int expiry_index)> strikes;
    std::function<double(int expiry_index)> years;
    std::function<bool(int expiry_index, double strike, bool is_call, double* mid, double* iv)> quote;
    int expiry_count = 0;
};

/// Build the legs of `tpl` around the strike nearest `atm_ref` at expiry
/// `expiry_index` (calendar legs use the next listed expiry). Empty with `error`
/// set when the chain cannot host it (no later expiry, strikes out of range).
std::vector<Leg> instantiate(const Template& tpl, const ChainAccess& chain, int expiry_index, double atm_ref,
                             int width, int quantity, double multiplier, std::string* error);

} // namespace fincept::services::options::strategy
