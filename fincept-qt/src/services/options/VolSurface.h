#pragma once
// VolSurface — implied-volatility smile and surface analytics in pure C++ (std only):
// raw-SVI smiles fitted per expiry, no-arbitrage sanity checks, smile statistics
// (ATM vol, 25-delta risk reversal and butterfly, expected move) and forward
// volatilities between expiries. Used by the Derivatives tab's CHAIN & VOL page on
// a worker thread.
//
// Notation: k = ln(K / F) (log-moneyness against the expiry's forward), T in years,
// total implied variance w(k) = σ_imp(k)²·T.
//
// References:
//   Gatheral, J. (2004) "A parsimonious arbitrage-free implied volatility
//     parameterization ..." (raw SVI), Global Derivatives, Madrid.
//   Gatheral, J., Jacquier, A. (2014) "Arbitrage-free SVI volatility surfaces",
//     Quantitative Finance 14(1): the density function g(k) (eq. 2.1) whose sign
//     tests butterfly arbitrage, and Vogt's arbitrageable parameter set (Example
//     3.1) used in the unit test; calendar arbitrage = total variance decreasing
//     in T at fixed k (Lemma 2.1).
//   Zeliade Systems (2009) "Quasi-explicit calibration of Gatheral's SVI model":
//     for fixed (m, σ) the remaining parameters solve a linear least-squares
//     problem; the outer two-parameter search here is Nelder-Mead.

#include <string>
#include <vector>

namespace fincept::services::options::vol {

struct SviParams {
    double a = 0;
    double b = 0;
    double rho = 0;
    double m = 0;
    double sigma = 0.1;
};

/// w(k) = a + b·(ρ(k − m) + √((k − m)² + σ²)).
double svi_w(const SviParams& p, double k);
double svi_w1(const SviParams& p, double k); ///< ∂w/∂k
double svi_w2(const SviParams& p, double k); ///< ∂²w/∂k²
/// Implied vol √(w/T) (0 when w ≤ 0 or T ≤ 0).
double svi_vol(const SviParams& p, double k, double T);

struct SviFit {
    SviParams params;
    bool ok = false;
    double rmse_vol = 0;  ///< root-mean-square error in implied-vol units (0.01 = 1 vol point)
    double max_err_vol = 0;
    int points = 0;
    std::string message; ///< why it failed ("fewer than 5 quotes" …)
};

/// Fit raw SVI to (k, implied vol) points of one expiry with optional weights
/// (e.g. vega or 1 / bid-ask spread). Needs at least 5 points spanning both sides
/// of the money. Deterministic; a few milliseconds for a few hundred points.
SviFit fit_svi(const std::vector<double>& k, const std::vector<double>& iv, const std::vector<double>& weights,
               double T);

/// Gatheral-Jacquier density factor g(k); g < 0 somewhere = butterfly arbitrage.
double svi_density_g(const SviParams& p, double k);

struct ArbitrageCheck {
    bool ok = true;         ///< no violation found on the grid
    int violations = 0;     ///< grid points that violate
    double worst = 0;       ///< most negative g (butterfly) / largest variance drop (calendar)
    double worst_k = 0;     ///< where
};

/// Butterfly check of one SVI slice on a uniform grid of `n` points over [k_lo, k_hi].
ArbitrageCheck butterfly_check(const SviParams& p, double k_lo, double k_hi, int n = 201);

/// Calendar check between a nearer slice (T1) and a later one (T2 > T1): the later
/// total variance must not fall below the nearer one at any k of the grid.
ArbitrageCheck calendar_check(const SviParams& near, const SviParams& far, double k_lo, double k_hi, int n = 101);

/// One option of a smile: strike, log-moneyness against the expiry forward, implied
/// vol (decimal) and the forward (undiscounted) delta of THAT option (call > 0, put < 0).
struct SmileQuote {
    double strike = 0;
    double k = 0;
    double iv = 0;
    double fwd_delta = 0;
    bool is_call = true;
};

struct SmileStats {
    double atm_iv = 0;      ///< at k = 0 (forward ATM), interpolated
    double iv_25c = 0;      ///< 25-delta call vol (0 when the smile does not reach it)
    double iv_25p = 0;      ///< 25-delta put vol
    double rr25 = 0;        ///< iv_25c − iv_25p
    double bf25 = 0;        ///< (iv_25c + iv_25p)/2 − atm_iv
    double move_1sd = 0;    ///< spot · atm_iv · √T
    bool has_25d = false;
};

/// Smile statistics from out-of-the-money quotes (puts below the forward, calls above).
SmileStats smile_stats(const std::vector<SmileQuote>& otm, double spot, double T);

/// σ_fwd between two expiries from ATM total variances; negative variance → −1 (calendar arbitrage).
double forward_vol(double atm_iv1, double T1, double atm_iv2, double T2);

/// Linear interpolation of y(x) at x0 over x ascending; outside the range → NaN
/// unless `extrapolate_flat`.
double interp_linear(const std::vector<double>& x, const std::vector<double>& y, double x0, bool extrapolate_flat);

} // namespace fincept::services::options::vol
