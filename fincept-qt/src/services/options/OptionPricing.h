#pragma once
// OptionPricing — synchronous Black-Scholes / Black-Scholes-Merton maths in pure
// C++: prices, analytical Greeks and implied volatility. Used by
// StrategyAnalytics for payoff curves (hundreds of spot points per scrub frame),
// by OptionChainService for the chain's per-leg IV + Greeks, and by the
// Derivatives screen's calculators.
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

/// Standard normal CDF, 0.5·erfc(−x/√2) — relative accuracy holds in the left tail too.
double normal_cdf(double x);

/// Standard normal density.
double normal_pdf(double x);

/// Black-Scholes (no dividends, q = 0).
double bs_call(double S, double K, double t, double r, double sigma);
double bs_put(double S, double K, double t, double r, double sigma);

/// Black-Scholes-Merton (continuous dividend yield q).
double bsm_call(double S, double K, double t, double r, double sigma, double q);
double bsm_put(double S, double K, double t, double r, double sigma, double q);

/// Black model (futures / forwards — no spot, no dividend).
double black_call(double F, double K, double t, double r, double sigma);
double black_put(double F, double K, double t, double r, double sigma);

/// Analytical BSM Greeks, per share, in the units OptionGreeks documents:
///   theta — per CALENDAR day (annual theta / 365)
///   vega  — per 1.00 of σ (divide by 100 for per vol point)
///   rho   — per 1.00 of r (divide by 100 for per 1% rate)
/// Requires S, K, t, sigma > 0.
struct BsmGreeks {
    double delta = 0;
    double gamma = 0;
    double theta = 0;
    double vega = 0;
    double rho = 0;
};
BsmGreeks bsm_greeks(bool is_call, double S, double K, double t, double r, double sigma, double q);

/// Outcome of an implied-volatility solve.
enum class IvStatus {
    Ok,             ///< `sigma` holds the implied volatility (0 when the price carries no time value)
    BelowIntrinsic, ///< price below the (forward) intrinsic value — no volatility reproduces it
    AboveMaximum,   ///< price at or above the no-arbitrage maximum (S·e^(−qt) call, K·e^(−rt) put)
    InvalidInput,   ///< S, K or t not positive, or a non-finite input
};

struct ImpliedVol {
    double sigma = 0; ///< annualised, decimal (0.142 = 14.2%); meaningful only when status == Ok
    IvStatus status = IvStatus::InvalidInput;
};

/// BSM implied volatility from a (discounted) option price. As in Jäckel's "Let's
/// Be Rational": the price is undiscounted onto the forward F = S·e^((r−q)t),
/// checked against the forward intrinsic value and maximum, an in-the-money option
/// is mapped to its out-of-the-money counterpart (time value only), and the
/// normalised Black equation is solved — here by a bracketed Newton iteration, to
/// full double precision.
ImpliedVol bsm_implied_vol(double price, double S, double K, double t, double r, double q, bool is_call);

} // namespace fincept::services::options::pricing
