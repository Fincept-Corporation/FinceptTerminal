#include "services/options/OptionPricing.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::services::options::pricing {

namespace {

constexpr double kInvSqrt2 = 0.70710678118654752440;
constexpr double kInvSqrt2Pi = 0.39894228040143267794; // 1/√(2π)
constexpr double kSqrtPi = 1.77245385090551602730;
constexpr double kSqrt2OverPi = 0.79788456080286535588; // √(2/π)
constexpr double kSqrt3 = 1.73205080756887729353;
constexpr double kTwoPiOverSqrt27 = 1.20919957615614523373; // 2π/√27
constexpr double kIvEps = std::numeric_limits<double>::epsilon();
constexpr int kIvMaxIterations = 100;

inline double max0(double v) {
    return std::max(v, 0.0);
}

// ── Normalised Black machinery for the implied-volatility solve ──────────────
//
// The solve works on the normalised out-of-the-money call
//     b(x, s) = e^{x/2}·Φ(h + t) − e^{−x/2}·Φ(h − t),   h = x/s, t = s/2,
// with x = ln(F/K) ≤ 0 and s = σ√t_expiry the total volatility. b rises from 0 to
// b_max = e^{x/2}, convex below the inflection point s_c = √(2|x|) and concave
// above it. ∂b/∂s (the normalised vega) is e^{−(h²+t²)/2}/√(2π).

/// erfcx(z) = e^{z²}·erfc(z), for z > −1.
double iv_erfcx(double z) {
    if (z < 10.0)
        return std::exp(z * z) * std::erfc(z);
    // Asymptotic series 1/(z√π)·Σ (−1)ⁿ(2n−1)!!/(2z²)ⁿ — its terms shrink while n < z².
    const double w = 0.5 / (z * z);
    double term = 1.0;
    double sum = 1.0;
    for (int n = 1; n < 40; ++n) {
        term *= -(2.0 * n - 1.0) * w;
        sum += term;
        if (std::fabs(term) < 1e-17 * sum)
            break;
    }
    return sum / (z * kSqrtPi);
}

struct NbEval {
    double b = 0;           ///< b(x, s) — may underflow to 0 deep out of the money
    double ln_b = 0;        ///< ln b, finite even when b underflows
    double vega = 0;        ///< ∂b/∂s
    double vega_over_b = 0; ///< (∂b/∂s)/b, finite even when b underflows
};

NbEval nb_eval(double x, double s) {
    const double h = x / s;
    const double t = 0.5 * s;
    NbEval e;
    if (h + t > 0.85) {
        // Dominated by the first term: the plain difference loses nothing.
        e.b = std::exp(0.5 * x) * normal_cdf(h + t) - std::exp(-0.5 * x) * normal_cdf(h - t);
        e.vega = kInvSqrt2Pi * std::exp(-0.5 * (h * h + t * t));
        e.ln_b = std::log(e.b);
        e.vega_over_b = e.vega / e.b;
        return e;
    }
    // b = ½·e^{−(h²+t²)/2}·[erfcx(−(h+t)/√2) − erfcx(−(h−t)/√2)]: the common Gaussian
    // factor is taken out, so neither term underflows before the difference is formed.
    const double d = iv_erfcx(-(h + t) * kInvSqrt2) - iv_erfcx(-(h - t) * kInvSqrt2);
    const double g = -0.5 * (h * h + t * t);
    e.b = 0.5 * std::exp(g) * d;
    e.vega = kInvSqrt2Pi * std::exp(g);
    e.ln_b = g + std::log(0.5 * d);
    e.vega_over_b = kSqrt2OverPi / d;
    return e;
}

/// b_max − b(x, s) as a sum of two positive terms — no cancellation as b → b_max.
double nb_gap_to_max(double x, double s) {
    const double h = x / s;
    const double t = 0.5 * s;
    return std::exp(0.5 * x) * normal_cdf(-(h + t)) + std::exp(-0.5 * x) * normal_cdf(h - t);
}

double nb_vega(double x, double s) {
    const double h = x / s;
    const double t = 0.5 * s;
    return kInvSqrt2Pi * std::exp(-0.5 * (h * h + t * t));
}

/// Φ⁻¹(p) to ~5e-4 (Abramowitz & Stegun 26.2.23). Only seeds the Newton iteration.
double iv_rough_inverse_cdf(double p) {
    const bool lower = p < 0.5;
    const double tail = std::clamp(lower ? p : 1.0 - p, 1e-300, 0.5);
    const double u = std::sqrt(-2.0 * std::log(tail));
    const double z =
        u - (2.515517 + u * (0.802853 + u * 0.010328)) / (1.0 + u * (1.432788 + u * (0.189269 + u * 0.001308)));
    return lower ? -z : z;
}

/// Total volatility s with b(x, s) = beta, for x ≤ 0 and 0 < beta < e^{x/2}.
///
/// Newton on an objective chosen per segment (the transformations of Jäckel's
/// "Let's Be Rational", which keep Newton near-linear): 1/ln b below the inflection
/// point, b itself in the middle, ln(b_max − b) near the maximum. Every iterate
/// tightens a bracket [lo, hi] around the root, and a step that leaves the bracket
/// is replaced by bisection, so the iteration cannot diverge.
double nb_implied_total_vol(double beta, double x) {
    const double b_max = std::exp(0.5 * x);
    const double s_c = std::sqrt(2.0 * std::fabs(x));
    const NbEval at_c = s_c > 0 ? nb_eval(x, s_c) : NbEval{};

    enum class Segment { Lower, Middle, Upper };
    Segment seg = Segment::Middle;
    double lo = 0.0;
    double hi = std::numeric_limits<double>::infinity();
    double s = 0.0;
    if (beta < at_c.b) {
        seg = Segment::Lower;
        hi = s_c;
        // Lowest part: invert Jäckel's lower map f(s) = (2π|x|/√27)·Φ(−|x|/(√3·s))³ ≈ b.
        const double p = std::cbrt(beta / (kTwoPiOverSqrt27 * std::fabs(x)));
        s = p < 0.5 ? std::fabs(x) / (kSqrt3 * std::fabs(iv_rough_inverse_cdf(p)))
                    : s_c - (at_c.b - beta) / at_c.vega; // Newton step from s_c on b itself
        if (!(s > lo && s < hi))
            s = 0.5 * hi;
    } else {
        lo = s_c;
        if (beta > 0.5 * b_max) {
            seg = Segment::Upper;
            // Far right b_max − b ≈ (e^{x/2} + e^{−x/2})·Φ(−s/2).
            s = -2.0 * iv_rough_inverse_cdf((b_max - beta) / (b_max + 1.0 / b_max));
        } else {
            seg = Segment::Middle;
            s = s_c > 0 ? s_c + (beta - at_c.b) / at_c.vega : beta / kInvSqrt2Pi;
        }
        if (!(s > lo))
            s = s_c > 0 ? 2.0 * s_c : 1.0;
    }

    const double ln_beta = std::log(beta);
    const double gap = b_max - beta; // exact in the Upper segment (beta > b_max/2)
    const double ln_gap = std::log(gap);
    for (int it = 0; it < kIvMaxIterations; ++it) {
        bool below = false; // b(x, s) < beta
        double ds = 0;
        switch (seg) {
            case Segment::Lower: {
                // g(s) = 1/ln b(s) − 1/ln β  →  Newton step (ln β − ln b)·ln b/ln β / (b'/b)
                const NbEval e = nb_eval(x, s);
                if (e.ln_b == ln_beta)
                    return s;
                below = !(e.ln_b > ln_beta);
                ds = (ln_beta - e.ln_b) * e.ln_b / ln_beta / e.vega_over_b;
                break;
            }
            case Segment::Middle: {
                // g(s) = b(s) − β
                const NbEval e = nb_eval(x, s);
                if (e.b == beta)
                    return s;
                below = e.b < beta;
                ds = (beta - e.b) / e.vega;
                break;
            }
            case Segment::Upper: {
                // g(s) = ln(b_max − β) − ln(b_max − b(s))
                const double gap_s = nb_gap_to_max(x, s);
                if (gap_s == gap)
                    return s;
                below = gap_s > gap;
                ds = (std::log(gap_s) - ln_gap) * gap_s / nb_vega(x, s);
                break;
            }
        }
        if (below)
            lo = std::max(lo, s);
        else
            hi = std::min(hi, s);

        double next = s + ds;
        if (!(next > lo && next < hi)) // left the bracket, or a non-finite step
            next = std::isfinite(hi) ? 0.5 * (lo + hi) : 2.0 * s;
        if (std::fabs(next - s) <= 4.0 * kIvEps * next || hi - lo <= 4.0 * kIvEps * next)
            return next;
        s = next;
    }
    return s;
}

} // namespace

double normal_cdf(double x) {
    return 0.5 * std::erfc(-x * kInvSqrt2);
}

double normal_pdf(double x) {
    return kInvSqrt2Pi * std::exp(-0.5 * x * x);
}

double bsm_call(double S, double K, double t, double r, double sigma, double q) {
    if (t <= 0)
        return max0(S - K);
    if (sigma <= 0) {
        // Deterministic forward — discount everything.
        return max0(S * std::exp(-q * t) - K * std::exp(-r * t));
    }
    if (S <= 0 || K <= 0)
        return 0.0;
    const double sqrt_t = std::sqrt(t);
    const double d1 = (std::log(S / K) + (r - q + 0.5 * sigma * sigma) * t) / (sigma * sqrt_t);
    const double d2 = d1 - sigma * sqrt_t;
    return S * std::exp(-q * t) * normal_cdf(d1) - K * std::exp(-r * t) * normal_cdf(d2);
}

double bsm_put(double S, double K, double t, double r, double sigma, double q) {
    if (t <= 0)
        return max0(K - S);
    if (sigma <= 0)
        return max0(K * std::exp(-r * t) - S * std::exp(-q * t));
    if (S <= 0 || K <= 0)
        return 0.0;
    const double sqrt_t = std::sqrt(t);
    const double d1 = (std::log(S / K) + (r - q + 0.5 * sigma * sigma) * t) / (sigma * sqrt_t);
    const double d2 = d1 - sigma * sqrt_t;
    return K * std::exp(-r * t) * normal_cdf(-d2) - S * std::exp(-q * t) * normal_cdf(-d1);
}

double bs_call(double S, double K, double t, double r, double sigma) {
    return bsm_call(S, K, t, r, sigma, 0.0);
}

double bs_put(double S, double K, double t, double r, double sigma) {
    return bsm_put(S, K, t, r, sigma, 0.0);
}

double black_call(double F, double K, double t, double r, double sigma) {
    if (t <= 0)
        return max0(F - K);
    if (sigma <= 0)
        return max0((F - K) * std::exp(-r * t));
    if (F <= 0 || K <= 0)
        return 0.0;
    const double sqrt_t = std::sqrt(t);
    const double d1 = (std::log(F / K) + 0.5 * sigma * sigma * t) / (sigma * sqrt_t);
    const double d2 = d1 - sigma * sqrt_t;
    return std::exp(-r * t) * (F * normal_cdf(d1) - K * normal_cdf(d2));
}

double black_put(double F, double K, double t, double r, double sigma) {
    if (t <= 0)
        return max0(K - F);
    if (sigma <= 0)
        return max0((K - F) * std::exp(-r * t));
    if (F <= 0 || K <= 0)
        return 0.0;
    const double sqrt_t = std::sqrt(t);
    const double d1 = (std::log(F / K) + 0.5 * sigma * sigma * t) / (sigma * sqrt_t);
    const double d2 = d1 - sigma * sqrt_t;
    return std::exp(-r * t) * (K * normal_cdf(-d2) - F * normal_cdf(-d1));
}

BsmGreeks bsm_greeks(bool is_call, double S, double K, double t, double r, double sigma, double q) {
    const double sqrt_t = std::sqrt(t);
    const double d1 = (std::log(S / K) + ((r - q) + sigma * sigma / 2.0) * t) / (sigma * sqrt_t);
    const double d2 = d1 - sigma * sqrt_t;
    const double eqt = std::exp(-q * t);
    const double ert = std::exp(-r * t);
    const double pdf_d1 = normal_pdf(d1);
    const double decay = (S * eqt * pdf_d1 * sigma) / (2.0 * sqrt_t);

    BsmGreeks g;
    g.gamma = eqt * pdf_d1 / (S * sigma * sqrt_t);
    g.vega = S * eqt * pdf_d1 * sqrt_t;
    if (is_call) {
        g.delta = eqt * normal_cdf(d1);
        g.theta = -(decay - q * S * eqt * normal_cdf(d1) + r * K * ert * normal_cdf(d2)) / 365.0;
        g.rho = t * K * ert * normal_cdf(d2);
    } else {
        g.delta = -eqt * normal_cdf(-d1);
        g.theta = (-decay - q * S * eqt * normal_cdf(-d1) + r * K * ert * normal_cdf(-d2)) / 365.0;
        g.rho = -t * K * ert * normal_cdf(-d2);
    }
    return g;
}

ImpliedVol bsm_implied_vol(double price, double S, double K, double t, double r, double q, bool is_call) {
    ImpliedVol out;
    if (!(S > 0) || !(K > 0) || !(t > 0) || !std::isfinite(S) || !std::isfinite(K) || !std::isfinite(t) ||
        !std::isfinite(price) || !std::isfinite(r) || !std::isfinite(q))
        return out; // InvalidInput

    // Undiscount onto the forward: price / e^(−rt), F = S·e^((r−q)t).
    const double undiscounted = price / std::exp(-r * t);
    const double F = S * std::exp((r - q) * t);
    if (!std::isfinite(undiscounted) || !std::isfinite(F) || !(F > 0))
        return out;

    const double intrinsic = std::fabs(max0(is_call ? F - K : K - F));
    if (undiscounted < intrinsic) {
        out.status = IvStatus::BelowIntrinsic;
        return out;
    }
    if (undiscounted >= (is_call ? F : K)) {
        out.status = IvStatus::AboveMaximum;
        return out;
    }

    // In the money → its out-of-the-money counterpart by put-call parity (time value
    // only), then express everything as a normalised out-of-the-money call (x ≤ 0).
    const double x = std::log(F / K);
    bool call = is_call;
    double p = undiscounted;
    if ((call ? x : -x) > 0) {
        p = std::fabs(max0(p - intrinsic));
        call = !call;
    }
    const double xn = call ? x : -x;
    const double beta = p / (std::sqrt(F) * std::sqrt(K));

    out.status = IvStatus::Ok;
    if (beta <= 0) // no time value at all
        return out;
    if (beta >= std::exp(0.5 * xn)) {
        out.status = IvStatus::AboveMaximum;
        return out;
    }
    out.sigma = nb_implied_total_vol(beta, xn) / std::sqrt(t);
    return out;
}

} // namespace fincept::services::options::pricing
