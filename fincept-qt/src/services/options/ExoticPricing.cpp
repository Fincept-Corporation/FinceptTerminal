#include "services/options/ExoticPricing.h"

#include "services/options/OptionModels.h"

#include <algorithm>
#include <cmath>

namespace fincept::services::options::exotic {

namespace {

constexpr double kInvSqrt2 = 0.70710678118654752440;
constexpr double kMinTotalVol = 1e-10;

inline double ex_cdf(double x) {
    return 0.5 * std::erfc(-x * kInvSqrt2);
}

inline bool ex_ok(double S, double K, double T, double sigma) {
    return std::isfinite(S) && std::isfinite(K) && std::isfinite(T) && std::isfinite(sigma) && S > 0 && K > 0 &&
           T >= 0 && sigma >= 0;
}

/// Generalised BSM with an explicit cost of carry b (Haug §1.1).
double ex_gbs(bool is_call, double S, double K, double T, double r, double b, double sigma) {
    return models::bsm_full(is_call, S, K, T, r, r - b, sigma).price;
}

/// (H/S)^p computed through logs; 0 / large values stay finite.
inline double ex_pow_ratio(double H, double S, double p) {
    const double v = std::exp(p * std::log(H / S));
    return std::isfinite(v) ? v : 0.0;
}

} // namespace

double digital_cash_or_nothing(bool is_call, double S, double K, double T, double r, double q, double sigma,
                               double cash) {
    if (!ex_ok(S, K, T, sigma) || !std::isfinite(r) || !std::isfinite(q) || !std::isfinite(cash))
        return 0.0;
    const double df = std::exp(-r * T);
    const double st = sigma * std::sqrt(T);
    if (T <= 0 || st < kMinTotalVol) {
        const double fwd = S * std::exp((r - q) * T);
        return (is_call ? fwd > K : fwd < K) ? cash * df : 0.0;
    }
    const double d2 = (std::log(S / K) + (r - q - 0.5 * sigma * sigma) * T) / st;
    return cash * df * ex_cdf(is_call ? d2 : -d2);
}

double digital_asset_or_nothing(bool is_call, double S, double K, double T, double r, double q, double sigma) {
    if (!ex_ok(S, K, T, sigma) || !std::isfinite(r) || !std::isfinite(q))
        return 0.0;
    const double cf = std::exp(-q * T);
    const double st = sigma * std::sqrt(T);
    if (T <= 0 || st < kMinTotalVol) {
        const double fwd = S * std::exp((r - q) * T);
        return (is_call ? fwd > K : fwd < K) ? S * cf : 0.0;
    }
    const double d1 = (std::log(S / K) + (r - q + 0.5 * sigma * sigma) * T) / st;
    return S * cf * ex_cdf(is_call ? d1 : -d1);
}

bool barrier_breached(BarrierKind kind, double S, double H) {
    const bool down = kind == BarrierKind::DownIn || kind == BarrierKind::DownOut;
    return down ? S <= H : S >= H;
}

double barrier(BarrierKind kind, bool is_call, double S, double K, double H, double rebate, double T, double r,
               double q, double sigma) {
    if (!ex_ok(S, K, T, sigma) || !(H > 0) || !std::isfinite(H) || !std::isfinite(r) || !std::isfinite(q) ||
        !std::isfinite(rebate))
        return 0.0;
    const bool knock_in = kind == BarrierKind::DownIn || kind == BarrierKind::UpIn;
    const bool down = kind == BarrierKind::DownIn || kind == BarrierKind::DownOut;
    const double b = r - q;
    const double df = std::exp(-r * T);
    if (barrier_breached(kind, S, H))
        return knock_in ? ex_gbs(is_call, S, K, T, r, b, sigma) : rebate;

    const double st = sigma * std::sqrt(T);
    if (T <= 0 || st < kMinTotalVol) {
        // Deterministic path S·e^{bt}: monotone, so it crosses H iff the end point is beyond it.
        const double s_end = S * std::exp(b * T);
        const bool hit = down ? s_end <= H : s_end >= H;
        const double payoff = std::max(is_call ? s_end - K : K - s_end, 0.0) * df;
        if (knock_in)
            return hit ? payoff : rebate * df;
        if (!hit)
            return payoff;
        const double t_hit = (std::fabs(b) > 1e-14) ? std::log(H / S) / b : T;
        return rebate * std::exp(-r * std::clamp(t_hit, 0.0, T));
    }

    const double mu = (b - 0.5 * sigma * sigma) / (sigma * sigma);
    const double lam = std::sqrt(mu * mu + 2.0 * r / (sigma * sigma));
    const double x1 = std::log(S / K) / st + (1.0 + mu) * st;
    const double x2 = std::log(S / H) / st + (1.0 + mu) * st;
    const double y1 = std::log(H * H / (S * K)) / st + (1.0 + mu) * st;
    const double y2 = std::log(H / S) / st + (1.0 + mu) * st;
    const double z = std::log(H / S) / st + lam * st;
    const double phi = is_call ? 1.0 : -1.0;
    const double eta = down ? 1.0 : -1.0;
    const double cf = std::exp((b - r) * T);
    const double hs_2mu1 = ex_pow_ratio(H, S, 2.0 * (mu + 1.0));
    const double hs_2mu = ex_pow_ratio(H, S, 2.0 * mu);

    const double A = phi * S * cf * ex_cdf(phi * x1) - phi * K * df * ex_cdf(phi * x1 - phi * st);
    const double B = phi * S * cf * ex_cdf(phi * x2) - phi * K * df * ex_cdf(phi * x2 - phi * st);
    const double C = phi * S * cf * hs_2mu1 * ex_cdf(eta * y1) - phi * K * df * hs_2mu * ex_cdf(eta * y1 - eta * st);
    const double D = phi * S * cf * hs_2mu1 * ex_cdf(eta * y2) - phi * K * df * hs_2mu * ex_cdf(eta * y2 - eta * st);
    const double E = rebate * df * (ex_cdf(eta * x2 - eta * st) - hs_2mu * ex_cdf(eta * y2 - eta * st));
    const double F = rebate * (ex_pow_ratio(H, S, mu + lam) * ex_cdf(eta * z) +
                               ex_pow_ratio(H, S, mu - lam) * ex_cdf(eta * z - 2.0 * eta * lam * st));
    const bool k_above_h = K > H;

    double v = 0;
    switch (kind) {
        case BarrierKind::DownIn:
            v = is_call ? (k_above_h ? C + E : A - B + D + E) : (k_above_h ? B - C + D + E : A + E);
            break;
        case BarrierKind::UpIn:
            v = is_call ? (k_above_h ? A + E : B - C + D + E) : (k_above_h ? A - B + D + E : C + E);
            break;
        case BarrierKind::DownOut:
            v = is_call ? (k_above_h ? A - C + F : B - D + F) : (k_above_h ? A - B + C - D + F : F);
            break;
        case BarrierKind::UpOut:
            v = is_call ? (k_above_h ? F : A - B + C - D + F) : (k_above_h ? B - D + F : A - C + F);
            break;
    }
    return std::isfinite(v) ? std::max(v, 0.0) : 0.0;
}

double asian_geometric(bool is_call, double S, double K, double T, double r, double q, double sigma) {
    if (!ex_ok(S, K, T, sigma) || !std::isfinite(r) || !std::isfinite(q))
        return 0.0;
    const double b = r - q;
    const double b_a = 0.5 * (b - sigma * sigma / 6.0);
    const double s_a = sigma / std::sqrt(3.0);
    return ex_gbs(is_call, S, K, T, r, b_a, s_a);
}

double asian_arithmetic_tw(bool is_call, double S, double K, double T, double r, double q, double sigma) {
    if (!ex_ok(S, K, T, sigma) || !std::isfinite(r) || !std::isfinite(q))
        return 0.0;
    const double b = r - q;
    if (T <= 0 || sigma * std::sqrt(T) < kMinTotalVol) {
        // Deterministic path: the average of S·e^{bt} over [0, T].
        const double avg = std::fabs(b * T) > 1e-12 ? S * (std::exp(b * T) - 1.0) / (b * T) : S;
        return std::exp(-r * T) * std::max(is_call ? avg - K : K - avg, 0.0);
    }
    const double v2 = sigma * sigma;
    double m1, m2;
    if (std::fabs(b) < 1e-7) {
        // b → 0 limits of the Turnbull-Wakeman moments (series of the exact forms).
        m1 = 1.0;
        const double x = v2 * T;
        m2 = x < 1e-6 ? 1.0 + x / 3.0 : 2.0 * (std::exp(x) - 1.0 - x) / (x * x);
    } else {
        m1 = (std::exp(b * T) - 1.0) / (b * T);
        m2 = 2.0 * std::exp((2.0 * b + v2) * T) / ((b + v2) * (2.0 * b + v2) * T * T) +
             2.0 / (b * T * T) * (1.0 / (2.0 * b + v2) - std::exp(b * T) / (b + v2));
    }
    if (!(m1 > 0) || !(m2 > 0) || !std::isfinite(m1) || !std::isfinite(m2))
        return asian_geometric(is_call, S, K, T, r, q, sigma); // never NaN: fall back to the exact geometric value
    const double b_a = std::log(m1) / T;
    const double var_a = std::log(m2) / T - 2.0 * b_a;
    const double s_a = var_a > 0 ? std::sqrt(var_a) : sigma / std::sqrt(3.0);
    return ex_gbs(is_call, S, K, T, r, b_a, s_a);
}

double asian_arithmetic_tw_seasoned(bool is_call, double S, double K, double T, double r, double q, double sigma,
                                    double avg_so_far, double elapsed) {
    if (!(elapsed > 0) || !(avg_so_far > 0) || !std::isfinite(avg_so_far) || !std::isfinite(elapsed))
        return asian_arithmetic_tw(is_call, S, K, T, r, q, sigma);
    if (!std::isfinite(K) || !std::isfinite(r) || !std::isfinite(q))
        return 0.0;
    if (!(T > 0)) // averaging over: the payoff on the observed average
        return std::max(is_call ? avg_so_far - K : K - avg_so_far, 0.0);
    if (!(S > 0) || !(sigma >= 0) || !std::isfinite(S) || !std::isfinite(sigma))
        return 0.0;
    const double L = elapsed + T;
    const double k_star = (L * K - elapsed * avg_so_far) / T;
    if (k_star <= 0) {
        if (!is_call)
            return 0.0;
        const double b = r - q;
        const double m1 = std::fabs(b * T) > 1e-12 ? (std::exp(b * T) - 1.0) / (b * T) : 1.0;
        return std::exp(-r * T) * (elapsed / L * avg_so_far + T / L * S * m1 - K);
    }
    return T / L * asian_arithmetic_tw(is_call, S, k_star, T, r, q, sigma);
}

double spread_kirk(bool is_call, double F1, double F2, double K, double T, double r, double sigma1, double sigma2,
                   double rho) {
    if (!std::isfinite(F1) || !std::isfinite(F2) || !std::isfinite(K) || !(F1 > 0) || !(F2 > 0) || !(F2 + K > 0) ||
        !(T >= 0) || sigma1 < 0 || sigma2 < 0 || !std::isfinite(rho))
        return 0.0;
    const double rr = std::clamp(rho, -1.0, 1.0);
    const double w = F2 / (F2 + K);
    const double var = sigma1 * sigma1 + (sigma2 * w) * (sigma2 * w) - 2.0 * rr * sigma1 * sigma2 * w;
    const double s = std::sqrt(std::max(var, 0.0));
    const double f = F1 / (F2 + K);
    return (F2 + K) * models::black76_price(is_call, f, 1.0, T, r, s);
}

double exchange_margrabe(double S1, double S2, double T, double q1, double q2, double sigma1, double sigma2,
                         double rho) {
    if (!std::isfinite(S1) || !std::isfinite(S2) || !(S1 > 0) || !(S2 > 0) || !(T >= 0) || sigma1 < 0 || sigma2 < 0)
        return 0.0;
    const double rr = std::clamp(rho, -1.0, 1.0);
    const double s = std::sqrt(std::max(sigma1 * sigma1 + sigma2 * sigma2 - 2.0 * rr * sigma1 * sigma2, 0.0));
    // Price of S1 in units of S2: a BSM call with "rate" q2, "dividend" q1, strike 1, on spot S1/S2.
    return S2 * models::bsm_price(true, S1 / S2, 1.0, T, q2, q1, s);
}

} // namespace fincept::services::options::exotic
