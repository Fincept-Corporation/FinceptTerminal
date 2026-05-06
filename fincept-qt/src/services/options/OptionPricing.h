#pragma once
// OptionPricing — synchronous Black-Scholes / Black-Scholes-Merton pricers
// in pure C++. Used by StrategyAnalytics for payoff curve evaluation at
// hundreds of spot points per scrub frame, where the async py_vollib daemon
// would be far too slow. Greeks for the analytics ribbon still come from
// the chain producer's Greeks worker (live, populated on row.ce_greeks /
// row.pe_greeks); these helpers exist purely for the curve.
//
// Math reference (BSM with continuous dividend yield q):
//
//   d1 = ( ln(S/K) + (r − q + σ²/2)·t ) / (σ·√t)
//   d2 = d1 − σ·√t
//   call = S·e^(−qt)·N(d1) − K·e^(−rt)·N(d2)
//   put  = K·e^(−rt)·N(−d2) − S·e^(−qt)·N(−d1)
//
// Edge cases (intrinsic + discount):
//   - t ≤ 0  → call = max(S − K, 0), put = max(K − S, 0)
//   - σ ≤ 0  → forward intrinsic value (deterministic future)

namespace fincept::services::options::pricing {

/// Standard normal CDF via std::erf — accurate to ~1e-9.
double normal_cdf(double x);

/// Black-Scholes (no dividends, q = 0).
double bs_call(double S, double K, double t, double r, double sigma);
double bs_put(double S, double K, double t, double r, double sigma);

/// Black-Scholes-Merton (continuous dividend yield q).
double bsm_call(double S, double K, double t, double r, double sigma, double q);
double bsm_put(double S, double K, double t, double r, double sigma, double q);

/// Black model (futures / forwards — no spot, no dividend).
double black_call(double F, double K, double t, double r, double sigma);
double black_put(double F, double K, double t, double r, double sigma);

} // namespace fincept::services::options::pricing
