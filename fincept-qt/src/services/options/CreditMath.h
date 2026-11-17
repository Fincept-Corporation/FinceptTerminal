#pragma once
// CreditMath — single-name credit default swap maths for the Derivatives tab's CREDIT
// page, in pure C++ (std only, plus rates::ZeroCurve for discounting): hazard-rate
// bootstrap from a CDS par-spread term structure, survival curve, risky annuity,
// upfront <-> running-spread conversion, CS01 (parallel and per tenor), recovery and
// interest-rate sensitivities, jump-to-default.
//
// MODEL — the ISDA CDS Standard Model, simplified (the simplifications are stated on the page):
//   * default intensity: piecewise constant ("flat forward" hazard) between the quoted
//     tenors, flat beyond the last one; survival Q(t) = exp(-∫ h);
//   * time: years from today (ACT/365F); the premium schedule is rolled BACK from the
//     maturity in steps of 1/frequency, so the first period is a short stub starting
//     today (no IMM-date roll, no step-in / cash-settlement dates, no accrued premium
//     paid at entry);
//   * premium leg: spread x accrual fraction x DF(t_i) x Q(t_i) per period, paid in
//     arrears; accrual fraction = period length x 365/360 for ACT/360 (the CDS market
//     convention, default) or the period length for ACT/365F;
//   * accrual on default: the premium accrued from the period start to the default time
//     is paid at default — integrated EXACTLY under a flat hazard and a flat forward
//     rate on each integration piece (ISDA's method);
//   * protection leg: (1 - R) x ∫_0^T DF(t) dQ(t) — integrated exactly the same way;
//   * discounting: a rates::ZeroCurve (the Treasury curve as a proxy for the risk-free
//     curve) or a flat continuous rate (flat_curve()); between grid points the forward
//     rate is taken flat (pieces of at most Conventions::max_step years), which is
//     exact for a flat rate;
//   * bootstrap: tenor by tenor, each quote's par spread is matched to machine precision
//     by a Brent root search on that segment's hazard; a quote that would need a
//     NEGATIVE hazard (too low after the shorter tenors) is reported, never forced.
// Units: spreads, coupons, rates, recovery, hazards are decimals (0.01 = 100 bp); money
// in the notional's currency; sensitivities are P&L of the position for the bump.
//
// References (test values recomputed independently in Python, see tests/tst_deriv_credit.cpp):
//   O'Kane, D. (2008) Modelling Single-name and Multi-name Credit Derivatives, Wiley,
//     ch. 5-6: survival curve, premium and protection legs, accrual on default, the risky
//     PV01, piecewise-flat bootstrap, the "credit triangle" S ≈ λ(1 - R), upfront =
//     (S - C) x RPV01.
//   ISDA / Markit (2009) "The CDS Standard Model": flat-forward hazard and interest
//     rates between knots, exact integration of the legs.
//   Hull, J. (2018) Options, Futures and Other Derivatives, 10th ed., ch. 25 "Credit
//     derivatives": the CDS valuation example (2 % conditional default probability a
//     year, R = 40 %, r = 5 %, 5 years, annual payments → 124 bp).

#include "services/options/RatesMath.h"

#include <functional>
#include <string>
#include <vector>

namespace fincept::services::options::credit {

enum class DayCount { Act360, Act365F };

/// 365/360 for ACT/360, 1 for ACT/365F (times are ACT/365F years).
double accrual_factor(DayCount dc);

struct Conventions {
    int frequency = 4;                    ///< premium payments a year: 1, 2, 4 or 12
    DayCount day_count = DayCount::Act360;
    bool accrual_on_default = true;
    double max_step = 1.0 / 24.0;         ///< longest integration piece (years) on a non-flat curve
};

/// A flat continuous rate as a curve (discounting with exp(-r t)).
rates::ZeroCurve flat_curve(double rate);

/// Payment times of the premium leg: maturity, maturity - 1/f, ... (> 0), ascending.
std::vector<double> premium_schedule(double maturity, int frequency);

struct HazardCurve {
    std::vector<double> t; ///< segment end times (years), ascending
    std::vector<double> h; ///< hazard on (t[k-1], t[k]] (t[-1] = 0), per year; the last one applies beyond
    bool empty() const { return t.empty() || h.empty(); }
    double hazard_at(double T) const;
    /// ∫_0^T h(u) du.
    double cumulative(double T) const;
    double survival(double T) const;
    double default_probability(double T) const { return 1.0 - survival(T); }
};

HazardCurve flat_hazard(double h);

/// Both legs per 1 of notional.
struct Legs {
    double protection = 0;         ///< (1 - R) ∫ DF dQ
    double rpv01 = 0;              ///< risky annuity: PV of 1.00 of running spread (incl. accrual on default)
    double accrual_on_default = 0; ///< the part of rpv01 that is accrual on default
    double par_spread() const { return rpv01 > 0 ? protection / rpv01 : 0.0; }
};

/// Legs of a CDS maturing `maturity` years from today.
Legs cds_legs(const HazardCurve& hazard, const rates::ZeroCurve& discount, double recovery, double maturity,
              const Conventions& cv = {});

/// Why a calculation could not be done (the page words each kind in the user's language).
enum class ErrorKind {
    None,
    NoQuotes,       ///< no tenor / spread pair
    BadTenor,       ///< a tenor that is not a positive number of years (index)
    DuplicateTenor, ///< the same tenor twice (index)
    BadSpread,      ///< a spread that is not positive (index)
    BadRecovery,    ///< recovery outside [0, 99 %]
    BadMaturity,    ///< maturity not positive
    NegativeHazard, ///< the quote is too low after the shorter tenors (index)
    NoSolution,     ///< no hazard / spread reproduces the input (index or -1)
    UpfrontRange,   ///< the upfront is outside what any positive spread gives
    Cancelled,
};

struct Bootstrap {
    bool ok = false;
    ErrorKind error = ErrorKind::None;
    int index = -1;                 ///< the quote the error is about (sorted order), else -1
    std::string message;            ///< English sentence (logs, tests)
    HazardCurve curve;
    std::vector<double> tenors;     ///< the quotes used, ascending by tenor
    std::vector<double> spreads;
    std::vector<double> repriced;   ///< par spread of each tenor off the fitted curve
    std::vector<double> rpv01;      ///< risky annuity of each tenor
};

/// Fit piecewise-constant hazards to par spreads (decimals) at `tenors` (years; any
/// order — sorted here). Validation errors and quotes that cannot be fitted give
/// `ok = false` with the kind and the quote's index.
Bootstrap bootstrap(const std::vector<double>& tenors, const std::vector<double>& spreads, double recovery,
                    const rates::ZeroCurve& discount, const Conventions& cv = {});

/// The flat curve whose par spread at `maturity` is `spread` (the market's "quoted
/// spread" convention for upfront conversion).
Bootstrap flat_from_spread(double spread, double maturity, double recovery, const rates::ZeroCurve& discount,
                           const Conventions& cv = {});

struct Conversion {
    bool ok = false;
    ErrorKind error = ErrorKind::None;
    std::string message;
    double spread = 0;  ///< quoted (flat) spread
    double upfront = 0; ///< fraction of notional paid by the protection BUYER (negative: received)
    double rpv01 = 0;   ///< risky annuity of the flat curve at the maturity
    double hazard = 0;  ///< the flat hazard
};

/// Upfront of a contract with running `coupon` for a quoted flat `spread`:
/// (S - C) x RPV01 on the flat curve fitted to S.
Conversion upfront_from_spread(double spread, double coupon, double maturity, double recovery,
                               const rates::ZeroCurve& discount, const Conventions& cv = {});
/// The inverse: the flat spread whose upfront at `coupon` is `upfront`.
Conversion spread_from_upfront(double upfront, double coupon, double maturity, double recovery,
                               const rates::ZeroCurve& discount, const Conventions& cv = {});

struct Position {
    double notional = 10e6;
    double coupon = 0.01;      ///< running coupon (decimal): 0.01 or 0.05 for standard contracts
    double maturity = 5.0;     ///< years from today
    bool buy_protection = true;
};

struct Risk {
    bool ok = false;
    ErrorKind error = ErrorKind::None;
    int index = -1;
    std::string message;
    Bootstrap fit;
    double par_spread = 0;     ///< at the position's maturity off the fitted curve
    double rpv01 = 0;          ///< per 1 notional
    double protection_leg = 0; ///< amount (notional x protection per unit)
    double premium_leg = 0;    ///< amount (notional x coupon x rpv01)
    double pv = 0;             ///< to the holder (buyer: protection - premium; seller: the opposite)
    double upfront = 0;        ///< fraction of notional the BUYER pays at the coupon (= (S - C) x RPV01)
    double upfront_amount = 0; ///< notional x upfront (buyer pays when positive)
    double survival = 1;       ///< to the maturity
    double default_probability = 0;
    double cs01 = 0;           ///< PV change for +1 bp on every quote (re-bootstrapped)
    double recovery01 = 0;     ///< PV change for +1 % recovery (re-bootstrapped at the same spreads)
    double ir_dv01 = 0;        ///< PV change for +1 bp on the discount curve (re-bootstrapped)
    double jtd = 0;            ///< P&L if the name defaulted today: ±N(1 - R) - pv
    std::vector<double> bucket_cs01; ///< PV change for +1 bp on each quote alone (NaN: bump not fittable)
};

/// The position's value and risks off the curve fitted to the quotes. `cancelled` (may
/// be empty) is polled between re-bootstraps; a cancelled run returns error Cancelled.
Risk position_risk(const std::vector<double>& tenors, const std::vector<double>& spreads, double recovery,
                   const rates::ZeroCurve& discount, const Conventions& cv, const Position& position,
                   const std::function<bool()>& cancelled = {});

} // namespace fincept::services::options::credit
