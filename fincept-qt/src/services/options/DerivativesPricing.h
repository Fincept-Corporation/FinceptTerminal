#pragma once
// DerivativesPricing — the Derivatives screen's calculators in pure C++: fixed-rate
// bonds (price from yield, yield from price), European equity options (price, Greeks,
// implied vol), Garman-Kohlhagen FX vanillas, fixed/float swaps and CDS. Synchronous
// and side-effect free: microseconds per call, safe to run on the UI thread.
//
// Conventions (inputs in percent where noted, outputs unrounded):
//   bond_price_from_ytm / bond_ytm_from_clean_price
//       Coupon schedule rolled BACKWARD from maturity at `freq` per year (1, 2, 3, 4,
//       6 or 12), a short first period allowed. Yield compounded at the coupon
//       frequency; cash flows discounted at (w + k) periods, w = the fraction of the
//       current coupon period still to run. Accrued interest actual/actual within the
//       period, from max(last coupon, issue). Prices per 100 face, durations in years.
//   equity_option / equity_implied_vol
//       Black-Scholes-Merton, continuous rate and dividend yield. Greeks in
//       pricing::BsmGreeks units (theta per calendar day, vega per 1.00 σ, rho per
//       1.00 r) — divide vega and rho by 100 for per vol point / per 1% rate.
//   fx_option
//       Garman-Kohlhagen: BSM with the foreign rate as the dividend yield. Premium in
//       domestic currency for `notional` units of foreign; Greeks per 1 foreign unit.
//   swap_value
//       Fixed vs float on a flat continuously-compounded curve, valued at the
//       effective date to the payer of fixed. Payments roll FORWARD from the effective
//       date, the last one at maturity (a short final period is kept), act/365.25.
//   cds_value
//       Flat hazard rate calibrated so a CDS paying the market spread is worth zero,
//       flat continuously-compounded discounting, quarterly premiums rolled backward
//       from maturity with premium accrued to default (mid-period). Value to the
//       protection buyer. act/365.25.
//
// Error texts are the messages the screen shows verbatim.

#include "core/result/Result.h"
#include "services/options/OptionPricing.h"

#include <QDate>

namespace fincept::services::options::derivatives {

/// Python's round(x, ndigits) for 0 ≤ ndigits ≤ 15: x rounded to `ndigits` decimals on
/// its exact binary value, exact ties to even, as the nearest double. The screen
/// rounds every displayed figure through this so its numbers stay what they were.
double round_half_even(double x, int ndigits);

struct BondPrice {
    double clean_price = 0;       // per 100 face
    double dirty_price = 0;       // per 100 face
    double accrued_interest = 0;  // per 100 face
    double macaulay_duration = 0; // years
    double modified_duration = 0; // years
    double convexity = 0;         // years²
    int remaining_coupons = 0;
    QDate next_coupon_date;
};

Result<BondPrice> bond_price_from_ytm(const QDate& issue, const QDate& settlement, const QDate& maturity,
                                      double coupon_rate_pct, double ytm_pct, int freq);

/// Yield to maturity (percent, coupon-frequency compounding) reproducing `clean_price`,
/// searched over −5%…100%.
Result<double> bond_ytm_from_clean_price(const QDate& issue, const QDate& settlement, const QDate& maturity,
                                         double coupon_rate_pct, double clean_price, int freq);

struct OptionValue {
    double price = 0;
    pricing::BsmGreeks greeks;
};

Result<OptionValue> equity_option(double spot, double strike, double years, double rate_pct, double vol_pct,
                                  double div_yield_pct, bool is_call);

/// Implied volatility (decimal) from a market price, solved over 0.1%…1000%.
Result<double> equity_implied_vol(double spot, double strike, double years, double rate_pct, double market_price,
                                  double div_yield_pct, bool is_call);

struct FxOptionValue {
    double premium = 0;             // domestic currency, whole notional
    double premium_per_unit = 0;    // domestic currency per 1 foreign unit
    double premium_pct_foreign = 0; // premium as % of the foreign notional's domestic value
    pricing::BsmGreeks greeks;      // per 1 unit of foreign notional
};

Result<FxOptionValue> fx_option(double spot, double strike, double years, double domestic_rate_pct,
                                double foreign_rate_pct, double vol_pct, bool is_call, double notional);

struct SwapValue {
    double value = 0; // to the payer of fixed
    double fixed_leg_pv = 0;
    double floating_leg_pv = 0;
    double par_rate_pct = 0;
    double tenor_years = 0;
    int payments = 0;
};

Result<SwapValue> swap_value(const QDate& effective, const QDate& maturity, double fixed_rate_pct, int freq,
                             double notional, double discount_rate_pct);

struct CdsValue {
    double upfront = 0;        // protection leg − premium leg, to the protection buyer
    double upfront_points = 0; // upfront per 100 notional
    double premium_leg_pv = 0;
    double protection_leg_pv = 0;
    double breakeven_spread_bps = 0; // par spread
    double risky_pv01 = 0;           // premium-leg PV of 1bp on the notional
    double hazard_rate_pct = 0;      // per year
    double survival_pct = 0;         // to maturity
    double tenor_years = 0;
    int premium_payments = 0;
};

Result<CdsValue> cds_value(const QDate& valuation, const QDate& maturity, double recovery_pct, double notional,
                           double spread_bps, double risk_free_pct, double coupon_bps);

} // namespace fincept::services::options::derivatives
