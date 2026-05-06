#include "services/options/OptionPricing.h"

#include <algorithm>
#include <cmath>

namespace fincept::services::options::pricing {

namespace {

constexpr double kSqrt2 = 1.4142135623730951;

inline double max0(double v) { return std::max(v, 0.0); }

}  // namespace

double normal_cdf(double x) {
    return 0.5 * (1.0 + std::erf(x / kSqrt2));
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

} // namespace fincept::services::options::pricing
