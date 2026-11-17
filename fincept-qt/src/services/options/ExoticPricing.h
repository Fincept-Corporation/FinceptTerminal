#pragma once
// ExoticPricing — closed-form and approximate exotic option values in pure C++
// (std only): digitals, single barriers, Asian (average-price) options, spread and
// exchange options. Same conventions as OptionModels.h (decimals, years,
// continuous dividend yield q, cost of carry b = r − q). Every function returns a
// finite number for finite inputs; invalid inputs (non-positive spot / strike where
// the formula needs them) return 0.
//
// References (formulas and the published test values in tests/tst_deriv_pricing.cpp):
//   Haug, E.G. (2007) The Complete Guide to Option Pricing Formulas, 2nd ed.:
//     §4.19 cash-or-nothing / asset-or-nothing, §4.17.1 standard barrier options
//     (Reiner & Rubinstein 1991; Table 4-13 values), §4.20.1 geometric average
//     (Kemna & Vorst 1990), §4.20.2 arithmetic average (Turnbull & Wakeman 1991),
//     §5.2 exchange option (Margrabe 1978), §5.3 spread option (Kirk 1995).

namespace fincept::services::options::exotic {

/// Pays `cash` at expiry if the option finishes in the money.
double digital_cash_or_nothing(bool is_call, double S, double K, double T, double r, double q, double sigma,
                               double cash);
/// Pays the asset itself at expiry if the option finishes in the money.
double digital_asset_or_nothing(bool is_call, double S, double K, double T, double r, double q, double sigma);

enum class BarrierKind { DownIn, DownOut, UpIn, UpOut };

/// Continuously monitored single barrier (Reiner-Rubinstein). `rebate` is paid at
/// expiry for knock-in options that never knock in, and at the hit for knock-out
/// options. A barrier already breached at valuation: knock-out = the rebate now,
/// knock-in = the vanilla option.
double barrier(BarrierKind kind, bool is_call, double S, double K, double H, double rebate, double T, double r,
               double q, double sigma);

/// True when `S` is already on the far side of the barrier (down: S ≤ H, up: S ≥ H).
bool barrier_breached(BarrierKind kind, double S, double H);

/// Continuous geometric-average price option (Kemna-Vorst): exact under GBM.
double asian_geometric(bool is_call, double S, double K, double T, double r, double q, double sigma);

/// Arithmetic-average price option averaged continuously from now to expiry,
/// Turnbull-Wakeman moment-matching approximation (the average is treated as
/// lognormal with its exact first two moments). Accurate to a few tenths of a
/// percent for volatilities up to ~40 % and a year; documented as an approximation.
double asian_arithmetic_tw(bool is_call, double S, double K, double T, double r, double q, double sigma);

/// Arithmetic-average price option whose averaging period has ALREADY STARTED: the
/// average runs continuously over [−elapsed, T], `avg_so_far` is the average observed
/// over the `elapsed` years already run, T the years left (Haug 2007 §4.20.2). With
/// L = elapsed + T the remaining part is a fresh Turnbull-Wakeman option on the
/// adjusted strike K* = (L·K − elapsed·A) / T, scaled by T / L. When K* ≤ 0 the call is
/// certain to be exercised and is worth exactly e^{−rT}·(elapsed/L·A + T/L·E[average
/// left] − K); the put is worthless. elapsed ≤ 0 (or no average) = asian_arithmetic_tw;
/// T ≤ 0 = the payoff on the observed average.
double asian_arithmetic_tw_seasoned(bool is_call, double S, double K, double T, double r, double q, double sigma,
                                    double avg_so_far, double elapsed);

/// Spread option on two forwards / futures paying max(±(F1 − F2 − K), 0), Kirk's
/// approximation (exact when K = 0). Requires F2 + K > 0. Discounted at r.
double spread_kirk(bool is_call, double F1, double F2, double K, double T, double r, double sigma1, double sigma2,
                   double rho);

/// Exchange option max(S1 − S2, 0) (Margrabe), with continuous yields q1, q2.
double exchange_margrabe(double S1, double S2, double T, double q1, double q2, double sigma1, double sigma2,
                         double rho);

} // namespace fincept::services::options::exotic
