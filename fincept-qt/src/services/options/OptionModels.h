#pragma once
// OptionModels — the option models of the Derivatives tab in pure C++ (std only,
// no Qt): full Black-Scholes-Merton Greeks up to third order, Black-76 on a
// forward, Bachelier (normal) model, American exercise by Cox-Ross-Rubinstein
// binomial tree (with discrete cash dividends) and the Barone-Adesi-Whaley
// approximation, and robust implied-volatility solvers for each.
//
// Conventions (every function):
//   * rates, yields and volatilities are decimals (0.05 = 5 %), times in years;
//   * q is a continuous dividend yield; the cost of carry is b = r − q;
//   * Greeks are RAW derivatives in these units: vega per 1.00 of σ, rho per
//     1.00 of r, theta per YEAR — the screen scales them (per vol point, per
//     calendar day). Time Greeks (theta, charm, colour, veta) are derivatives
//     with respect to calendar time passing, i.e. −∂/∂T;
//   * nothing returns NaN for finite inputs: degenerate limits (T → 0, σ → 0,
//     deep in / out of the money) collapse to the intrinsic / forward-intrinsic
//     value with zero curvature Greeks; invalid inputs give `valid = false`.
//
// References (formulas and the published test values in tests/tst_deriv_pricing.cpp):
//   Haug, E.G. (2007) The Complete Guide to Option Pricing Formulas, 2nd ed., McGraw-Hill:
//     §1.1 generalised BSM, §2 Greeks (incl. vanna, volga, charm, speed, colour,
//     zomma, ultima, veta, dual delta/gamma), §3.1 Barone-Adesi & Whaley.
//   Barone-Adesi, G., Whaley, R. (1987) "Efficient analytic approximation of
//     American option values", Journal of Finance 42(2).
//   Cox, J., Ross, S., Rubinstein, M. (1979) "Option pricing: a simplified
//     approach", Journal of Financial Economics 7.
//   Hull, J. (2018) Options, Futures and Other Derivatives, 10th ed., ch. 21
//     (binomial trees, escrowed discrete dividends).
//   Bachelier (normal) model: e.g. Schachermayer & Teichmann (2008) "How close are
//     the option pricing formulas of Bachelier and Black-Merton-Scholes?",
//     Mathematical Finance 18(1).
//   Brent, R. (1973) Algorithms for Minimization Without Derivatives, ch. 4.

#include <functional>
#include <vector>

namespace fincept::services::options::models {

// ── Full Greeks ──────────────────────────────────────────────────────────────

struct Greeks {
    double price = 0;
    // first order
    double delta = 0; ///< ∂V/∂S (∂V/∂F for Black-76 and Bachelier)
    double vega = 0;  ///< ∂V/∂σ, per 1.00 σ (Bachelier: per 1.00 of absolute normal vol)
    double theta = 0; ///< −∂V/∂T, per year
    double rho = 0;   ///< ∂V/∂r, per 1.00 r (Black-76 / Bachelier: discounting only)
    double phi = 0;   ///< ∂V/∂q, per 1.00 q (BSM only)
    // second order
    double gamma = 0; ///< ∂²V/∂S²
    double vanna = 0; ///< ∂²V/∂S∂σ = ∂Δ/∂σ
    double volga = 0; ///< ∂²V/∂σ² (vomma)
    double charm = 0; ///< −∂Δ/∂T, per year (delta decay)
    double veta = 0;  ///< −∂vega/∂T, per year
    // third order
    double speed = 0;  ///< ∂Γ/∂S
    double colour = 0; ///< −∂Γ/∂T, per year (gamma decay)
    double zomma = 0;  ///< ∂Γ/∂σ
    double ultima = 0; ///< ∂volga/∂σ
    // strike / elasticity
    double dual_delta = 0; ///< ∂V/∂K
    double dual_gamma = 0; ///< ∂²V/∂K²
    double lambda = 0;     ///< Δ·S/V (elasticity); 0 when V = 0
    bool valid = false;
};

/// Generalised Black-Scholes-Merton (Haug §1.1, b = r − q) price and every Greek above.
Greeks bsm_full(bool is_call, double S, double K, double T, double r, double q, double sigma);

/// Black (1976) option on a forward / futures price F, discounted at r. Delta etc.
/// are with respect to F. Identical to bsm_full(S = F, q = r).
Greeks black76_full(bool is_call, double F, double K, double T, double r, double sigma);

/// Bachelier (arithmetic Brownian motion) option on a forward F with absolute normal
/// volatility sigma_n (price units per √year), discounted at r. Works for zero and
/// negative forwards / strikes. Greeks: delta, gamma, vega (per 1.00 of sigma_n),
/// theta, rho, vanna, volga, dual delta / gamma.
Greeks bachelier_full(bool is_call, double F, double K, double T, double r, double sigma_n);

double bsm_price(bool is_call, double S, double K, double T, double r, double q, double sigma);
double black76_price(bool is_call, double F, double K, double T, double r, double sigma);
double bachelier_price(bool is_call, double F, double K, double T, double r, double sigma_n);

// ── American exercise ────────────────────────────────────────────────────────

/// A cash dividend paid `time` years from now (0 < time ≤ T counts).
struct CashDividend {
    double time = 0;
    double amount = 0;
};

struct TreeResult {
    double price = 0;
    double delta = 0;
    double gamma = 0;
    double theta = 0;               ///< per year, from the tree's first steps
    double early_exercise_premium = 0; ///< American − European on the same tree (0 for European)
    bool valid = false;
};

/// Cox-Ross-Rubinstein binomial tree, European or American. Discrete cash dividends
/// use the escrowed-dividend model (Hull ch. 21): the tree runs on S − PV(dividends)
/// and the dividends still to be paid are added back at every node. `steps` is
/// clamped to [3, 5000]; 300–800 is a good speed / accuracy balance.
TreeResult crr_price(bool is_call, bool american, double S, double K, double T, double r, double q, double sigma,
                     int steps, const std::vector<CashDividend>& dividends = {});

/// Barone-Adesi & Whaley (1987) quadratic approximation of an American option with a
/// continuous dividend yield. Never below the European value or the intrinsic value.
double baw_price(bool is_call, double S, double K, double T, double r, double q, double sigma);

/// BAW Greeks by central finite differences of baw_price (delta, gamma, vega, theta, rho).
Greeks baw_greeks(bool is_call, double S, double K, double T, double r, double q, double sigma);

/// Present value of the dividends paid within (0, T].
double pv_dividends(const std::vector<CashDividend>& dividends, double T, double r);

// ── Implied volatility ───────────────────────────────────────────────────────

enum class SolveStatus {
    Ok,             ///< `sigma` reproduces the price
    BelowIntrinsic, ///< price under the no-arbitrage minimum: no volatility gives it
    AboveMaximum,   ///< price over the no-arbitrage maximum
    NoSolution,     ///< the model never reaches the price inside the search range
    InvalidInput,   ///< non-positive spot / strike / time, or a non-finite input
};

struct VolSolve {
    double sigma = 0; ///< meaningful only when status == Ok
    SolveStatus status = SolveStatus::InvalidInput;
    int iterations = 0;
};

/// Root of price_of(σ) = target for a price that rises with σ, by bracketing and a
/// Brent (inverse-quadratic / secant / bisection) iteration with a Newton polish
/// when `vega_of` is given. [lo, hi] is expanded (up to `hi_cap`) until it brackets
/// the target; "no solution" is reported, never NaN. tol is on σ.
VolSolve solve_increasing(const std::function<double(double)>& price_of, double target, double lo, double hi,
                          double hi_cap, double tol = 1e-10);

/// Plain-language text of a status ("price below intrinsic value" …).
const char* solve_status_text(SolveStatus status);

VolSolve bsm_implied_vol(double price, bool is_call, double S, double K, double T, double r, double q);
VolSolve black76_implied_vol(double price, bool is_call, double F, double K, double T, double r);
/// Implied absolute normal volatility.
VolSolve bachelier_implied_vol(double price, bool is_call, double F, double K, double T, double r);

enum class AmericanModel { Binomial, BaroneAdesiWhaley };
/// American implied volatility on the chosen model (binomial: `steps`, dividends honoured).
VolSolve american_implied_vol(double price, AmericanModel model, bool is_call, double S, double K, double T, double r,
                              double q, int steps = 400, const std::vector<CashDividend>& dividends = {});

} // namespace fincept::services::options::models
