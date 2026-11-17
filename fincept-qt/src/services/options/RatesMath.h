#pragma once
// RatesMath — yield-curve maths for the Derivatives tab in pure C++ (std only):
// bootstrap of a par yield curve (U.S. Treasury CMT par yields, semi-annual) into
// continuously compounded zero rates and discount factors, interpolation, forward
// rates. The option pages read the risk-free rate for an expiry from it.
//
// Conventions: par yields in PERCENT (bond-equivalent, semi-annual coupons, as the
// Treasury publishes them); zero rates continuous decimals; times in years
// (ACT/365). Bills (≤ 1 year) are read as zero-coupon bond-equivalent yields; from
// 1 year on, par bonds paying semi-annual coupons on a 6-month grid whose par
// yields are interpolated linearly in maturity (Hull ch. 4 bootstrap method).
// Interpolation of zero rates is linear in time with flat extrapolation.

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace fincept::services::options::rates {

struct ZeroCurve {
    std::vector<double> t; ///< years, ascending
    std::vector<double> r; ///< continuous zero rate (decimal) at t
    bool empty() const { return t.empty(); }
    /// Zero rate at T (linear in T, flat outside the curve). 0 for an empty curve.
    double rate_at(double T) const;
    /// e^{−r(T)·T}.
    double df(double T) const;
};

/// Bootstrap par yields (percent) at `tenors` (years). Needs ≥ 2 points.
ZeroCurve bootstrap_par_curve(const std::vector<double>& tenors, const std::vector<double>& par_pct);

/// Continuously compounded forward rate between t1 < t2.
double forward_rate(const ZeroCurve& c, double t1, double t2);

/// Bond-equivalent (semi-annual) yield in percent → continuous decimal.
double bey_to_continuous(double y_pct);

/// The curve with every zero rate shifted by `bp` basis points (parallel shock).
ZeroCurve shifted(const ZeroCurve& c, double bp);

// ── Instruments off the curve (single-curve: the same curve projects and discounts;
//    accrual fractions are the schedule's year fractions, ACT/365) ──────────────────

struct SwapResult {
    double pv = 0;        ///< to the side chosen (payer of fixed when `payer`)
    double par_rate = 0;  ///< decimal
    double annuity = 0;   ///< Σ τ·DF, per 1 of notional
    double fixed_pv = 0;
    double float_pv = 0;
    double dv01 = 0;      ///< PV change for a +1 bp parallel shift of the curve (signed)
    int payments = 0;
};
/// Fixed-for-floating swap starting `start` years from now for `tenor` years, fixed
/// rate decimal paid `freq` times a year; float leg = DF(start) − DF(end).
SwapResult swap_on_curve(const ZeroCurve& c, double start, double tenor, double fixed_rate, int freq,
                         double notional, bool payer);

/// Forward rate agreement on [t1, t2] at `strike` (decimal, simple): value to the payer
/// of the fixed rate = N·τ·(F − K)·DF(t2). `fwd` gets the simple forward rate.
double fra_value(const ZeroCurve& c, double t1, double t2, double strike, double notional, bool pay_fixed,
                 double* fwd = nullptr);

struct CapResult {
    double value = 0;
    std::vector<double> caplet_values; ///< per period with a future fixing
    std::vector<double> forwards;      ///< simple forward of each period
    double dv01 = 0;                   ///< value change for +1 bp of the curve (vol fixed)
};
/// Cap (or floor) on the simple forward rate of each period of a `freq`-per-year
/// schedule from `start` to `start + tenor`, each caplet priced with Black-76 at a flat
/// lognormal `vol` (Bachelier when `normal_vol`, vol then in rate units). The first
/// period of a spot-starting cap (its rate is already fixed) is left out.
CapResult cap_floor(const ZeroCurve& c, double start, double tenor, double strike, double vol, int freq,
                    double notional, bool cap, bool normal_vol = false);

// ── Additions (D6): swap PV, key-rate DV01, swaptions, bonds off the curve, carry ──

/// PV of the swap of swap_on_curve() alone (no risk figures): cheap enough to re-price
/// on bumped curves.
double swap_pv(const ZeroCurve& c, double start, double tenor, double fixed_rate, int freq, double notional,
               bool payer);

/// Key-rate DV01: the PV change for +1 bp on ONE par point of the input par curve.
/// Method: that par yield is bumped by ±1 bp, the curve re-bootstrapped with
/// bootstrap_par_curve() and the instrument re-priced (central difference, half the
/// up − down change). Because the bootstrap interpolates par yields linearly between
/// tenors, bumping one point is a triangular ("tent") bump of the par curve between its
/// neighbouring tenors, so the buckets add up — to first order — to the PV change of a
/// +1 bp parallel shift of the PAR curve (`parallel`). Key tenors are the curve's own
/// tenors. `cancel` (optional) is polled between bumps; a cancelled run returns
/// `cancelled = true` and partial buckets.
struct KeyRateResult {
    std::vector<double> tenors;
    std::vector<double> dv01; ///< per key tenor, currency per +1 bp
    double total = 0;         ///< Σ buckets
    double parallel = 0;      ///< every par point +1 bp at once
    double base_pv = 0;
    bool cancelled = false;
};
using CurvePricer = std::function<double(const ZeroCurve&)>;
KeyRateResult key_rate_dv01(const std::vector<double>& tenors, const std::vector<double>& par_pct,
                            const CurvePricer& pv, const std::atomic<bool>* cancel = nullptr);

// ── Swaptions ────────────────────────────────────────────────────────────────
// European swaption exercisable at `expiry` into a swap starting then for `tenor`
// years, fixed `strike` paid `freq` times a year (schedule and accruals as
// swap_on_curve). Forward swap rate F = (DF(expiry) − DF(end)) / A, annuity
// A = Σ τ·DF (single curve). Black-76: premium = N·A·[F·N(d1) − K·N(d2)] (payer) with a
// lognormal vol; Bachelier: N·A·[(F − K)·N(d) + σn·√T·n(d)] with a normal vol in rate
// units (100 bp = 0.01). Payer = call on the swap rate. Hull, Options, Futures and
// Other Derivatives, "Swaptions" (Black's model for European swap options).
struct SwaptionResult {
    double premium = 0;       ///< currency, whole notional
    double forward_rate = 0;  ///< decimal
    double annuity = 0;       ///< Σ τ·DF per 1 of notional
    double intrinsic = 0;     ///< N·A·max(±(F − K), 0)
    double delta_bp = 0;      ///< premium change per +1 bp of the forward swap rate (annuity fixed)
    double vega = 0;          ///< per 1 vol point (Black: 1 % lognormal; Bachelier: 1 bp normal)
    double dv01 = 0;          ///< premium change for +1 bp parallel zero-curve shift, vol fixed
    int payments = 0;
    bool ok = false;
    std::string message;      ///< why no price (plain sentence) when !ok
};
SwaptionResult swaption_on_curve(const ZeroCurve& c, double expiry, double tenor, double strike, double vol, int freq,
                                 double notional, bool payer, bool normal_vol);

// ── Fixed-coupon bonds off the curve ─────────────────────────────────────────
// Coupons `coupon_pct` (annual, percent of face) paid `freq` times a year on a schedule
// rolled backward from maturity (the first period may be short). Dirty price =
// Σ CF·DF(t)·e^{−z·t} with a constant continuously compounded spread z over the zero
// curve (the Z-spread). Accrued interest is linear in time since the previous (rolled)
// coupon date; clean = dirty − accrued. Prices per 100 of face.
struct BondSpec {
    double coupon_pct = 5.0;
    int freq = 2;
    double maturity = 5.0; ///< years from today
    double face = 100.0;
};
struct BondCurveResult {
    double dirty = 0;
    double clean = 0;
    double accrued = 0;
    double duration = 0;  ///< effective: −(1/P)·∂P/∂z, years
    double convexity = 0; ///< (1/P)·∂²P/∂z²
    double annuity = 0;   ///< Σ τ·DF on the coupon schedule from today (τ of the first period from today), per 1 of face
    int payments = 0;
    bool ok = false;
};
BondCurveResult bond_on_curve(const ZeroCurve& c, const BondSpec& b, double z_spread = 0.0);

struct SpreadSolve {
    double spread = 0; ///< decimal (0.005 = 50 bp)
    bool ok = false;
    int iterations = 0;
    std::string message; ///< plain sentence when !ok
};
/// The Z-spread that reprices `dirty_price` (per 100 face): bracketing + bisection /
/// secant to 1e-12; "no spread reprices this price" is reported, never NaN.
SpreadSolve z_spread(const ZeroCurve& c, const BondSpec& b, double dirty_price);
/// Par-par asset-swap spread: (curve dirty price − market dirty price) / (face × A),
/// A the annuity of a swap leg on the bond's own coupon schedule (first accrual from
/// today). Positive = the bond is cheap to the curve. Decimal.
double asset_swap_spread(const ZeroCurve& c, const BondSpec& b, double market_dirty);

// ── Futures fair value: cost of carry (Hull, ch. 5) ──────────────────────────
// F = (S − PV(I))·e^{(r + c − y)·T}: r the financing rate (continuous), c a continuous
// carry / storage cost, y a continuous income / dividend / convenience yield, I discrete
// cash income (dividends, coupons; a storage cost paid in cash is negative income),
// discounted at r. Implied repo (continuous) is the r that makes the market futures
// price fair with c, y and I unchanged: ln(F_mkt / (S − PV(I)))/T − c + y; the simple-
// rate version is (F_mkt·e^{(y − c)T} / (S − PV(I)) − 1)/T. The implied yield is the y
// that makes F_mkt fair at r: r + c − ln(F_mkt / (S − PV(I)))/T.
struct CashFlowAt {
    double time = 0;   ///< years
    double amount = 0; ///< + income, − cost
};
struct CarryInputs {
    double spot = 100;
    double years = 0.25;
    double rate = 0.04;         ///< continuous
    double carry_cost = 0;      ///< continuous
    double income_yield = 0;    ///< continuous
    std::vector<CashFlowAt> income;
    double market_future = 0;   ///< 0 or less = none
};
struct CarryResult {
    double fair = 0;
    double pv_income = 0;
    double basis = 0;             ///< market − fair (when a market price is given)
    double spot_basis = 0;        ///< market − spot
    double implied_repo = 0;      ///< continuous
    double implied_repo_simple = 0;
    double implied_yield = 0;     ///< continuous
    bool has_market = false;
    bool ok = false;
    std::string message;
};
CarryResult futures_carry(const CarryInputs& in);

} // namespace fincept::services::options::rates
