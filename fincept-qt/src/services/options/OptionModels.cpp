#include "services/options/OptionModels.h"

#include "services/options/OptionPricing.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::services::options::models {

namespace {

constexpr double kInvSqrt2Pi = 0.39894228040143267794;
constexpr double kInvSqrt2 = 0.70710678118654752440;
/// Below this total volatility σ√T the lognormal / normal density is a spike: the
/// option is valued at its (forward) intrinsic value with zero curvature Greeks.
constexpr double kMinTotalVol = 1e-10;

inline double om_cdf(double x) {
    return 0.5 * std::erfc(-x * kInvSqrt2);
}
inline double om_pdf(double x) {
    return kInvSqrt2Pi * std::exp(-0.5 * x * x);
}
inline bool om_finite(double a, double b, double c, double d, double e, double f) {
    return std::isfinite(a) && std::isfinite(b) && std::isfinite(c) && std::isfinite(d) && std::isfinite(e) &&
           std::isfinite(f);
}

/// Deterministic limit of the generalised BSM (σ√T → 0 or T → 0): the option is worth
/// its discounted forward intrinsic value; only first-order Greeks survive.
Greeks om_bsm_degenerate(bool is_call, double S, double K, double T, double r, double q) {
    Greeks g;
    const double cf = std::exp(-q * T);
    const double df = std::exp(-r * T);
    const double fwd_gap = S * cf - K * df; // call value if positive
    const bool itm = is_call ? fwd_gap > 0 : fwd_gap < 0;
    if (itm) {
        const double sgn = is_call ? 1.0 : -1.0;
        g.price = sgn * fwd_gap;
        g.delta = sgn * cf;
        g.theta = sgn * (q * S * cf - r * K * df); // −∂/∂T of ±(S e^{−qT} − K e^{−rT})
        g.rho = sgn * T * K * df;
        g.phi = -sgn * T * S * cf;
        g.dual_delta = -sgn * df;
        g.lambda = g.price > 0 ? g.delta * S / g.price : 0.0;
    }
    g.valid = true;
    return g;
}

/// Generalised BSM with an explicit cost of carry b (b = r − q; b = 0 for a forward).
Greeks om_gbs(bool is_call, double S, double K, double T, double r, double b, double sigma) {
    Greeks g;
    if (!om_finite(S, K, T, r, b, sigma) || !(S > 0) || !(K > 0) || T < 0 || sigma < 0)
        return g; // invalid
    const double q = r - b;
    const double sqrt_t = std::sqrt(std::max(T, 0.0));
    const double st = sigma * sqrt_t;
    if (T <= 0 || st < kMinTotalVol)
        return om_bsm_degenerate(is_call, S, K, std::max(T, 0.0), r, q);

    const double d1 = (std::log(S / K) + (b + 0.5 * sigma * sigma) * T) / st;
    const double d2 = d1 - st;
    const double cf = std::exp((b - r) * T); // e^{−qT}
    const double df = std::exp(-r * T);
    const double nd1 = om_pdf(d1);
    const double nd2 = om_pdf(d2);
    const double Nd1 = om_cdf(d1), Nmd1 = om_cdf(-d1);
    const double Nd2 = om_cdf(d2), Nmd2 = om_cdf(-d2);

    if (is_call) {
        g.price = S * cf * Nd1 - K * df * Nd2;
        g.delta = cf * Nd1;
        g.theta = -S * cf * nd1 * sigma / (2.0 * sqrt_t) - (b - r) * S * cf * Nd1 - r * K * df * Nd2;
        g.rho = T * K * df * Nd2;
        g.phi = -T * S * cf * Nd1;
        g.dual_delta = -df * Nd2;
    } else {
        g.price = K * df * Nmd2 - S * cf * Nmd1;
        g.delta = -cf * Nmd1;
        g.theta = -S * cf * nd1 * sigma / (2.0 * sqrt_t) + (b - r) * S * cf * Nmd1 + r * K * df * Nmd2;
        g.rho = -T * K * df * Nmd2;
        g.phi = T * S * cf * Nmd1;
        g.dual_delta = df * Nmd2;
    }
    g.price = std::max(g.price, 0.0); // rounding at the far wings must not print −0.000…

    const double dd1_dT = b / st - d2 / (2.0 * T); // ∂d1/∂T
    g.gamma = cf * nd1 / (S * st);
    g.vega = S * cf * nd1 * sqrt_t;
    g.vanna = -cf * nd1 * d2 / sigma;
    g.volga = g.vega * d1 * d2 / sigma;
    g.charm = is_call ? -cf * (nd1 * dd1_dT + (b - r) * Nd1) : -cf * (nd1 * dd1_dT - (b - r) * Nmd1);
    g.veta = g.vega * (r - b + b * d1 / st - (1.0 + d1 * d2) / (2.0 * T));
    g.speed = -g.gamma * (1.0 + d1 / st) / S;
    g.colour = g.gamma * (r - b + b * d1 / st + (1.0 - d1 * d2) / (2.0 * T));
    g.zomma = g.gamma * (d1 * d2 - 1.0) / sigma;
    g.ultima = -g.vega / (sigma * sigma) * (d1 * d2 * (1.0 - d1 * d2) + d1 * d1 + d2 * d2);
    g.dual_gamma = df * nd2 / (K * st);
    g.lambda = g.price > 1e-300 ? g.delta * S / g.price : 0.0;
    g.valid = true;
    return g;
}

/// Brent's root finder on g over [a, b] with g(a)·g(b) ≤ 0 (Brent 1973, ch. 4).
double om_brent(const std::function<double(double)>& g, double a, double b, double ga, double gb, double tol,
                int* iterations) {
    double c = a, gc = ga, d = b - a, e = d;
    for (int it = 0; it < 200; ++it) {
        if (iterations)
            *iterations = it + 1;
        if ((gb > 0 && gc > 0) || (gb < 0 && gc < 0)) {
            c = a;
            gc = ga;
            d = e = b - a;
        }
        if (std::fabs(gc) < std::fabs(gb)) {
            a = b;
            b = c;
            c = a;
            ga = gb;
            gb = gc;
            gc = ga;
        }
        const double tol1 = 2.0 * std::numeric_limits<double>::epsilon() * std::fabs(b) + 0.5 * tol;
        const double xm = 0.5 * (c - b);
        if (std::fabs(xm) <= tol1 || gb == 0.0)
            return b;
        if (std::fabs(e) >= tol1 && std::fabs(ga) > std::fabs(gb)) {
            double p, qq;
            const double s = gb / ga;
            if (a == c) {
                p = 2.0 * xm * s;
                qq = 1.0 - s;
            } else {
                const double qa = ga / gc;
                const double rr = gb / gc;
                p = s * (2.0 * xm * qa * (qa - rr) - (b - a) * (rr - 1.0));
                qq = (qa - 1.0) * (rr - 1.0) * (s - 1.0);
            }
            if (p > 0)
                qq = -qq;
            p = std::fabs(p);
            const double min1 = 3.0 * xm * qq - std::fabs(tol1 * qq);
            const double min2 = std::fabs(e * qq);
            if (2.0 * p < std::min(min1, min2)) {
                e = d;
                d = p / qq;
            } else {
                d = xm;
                e = d;
            }
        } else {
            d = xm;
            e = d;
        }
        a = b;
        ga = gb;
        b += std::fabs(d) > tol1 ? d : (xm > 0 ? tol1 : -tol1);
        gb = g(b);
    }
    return b;
}

// ── Barone-Adesi & Whaley (Haug 2007, §3.1.3) ─────────────────────────────────

double om_gbs_price(bool is_call, double S, double K, double T, double r, double b, double sigma) {
    return om_gbs(is_call, S, K, T, r, b, sigma).price;
}

double om_baw_call(double S, double K, double T, double r, double b, double sigma) {
    const double euro = om_gbs_price(true, S, K, T, r, b, sigma);
    if (b >= r) // no dividend drag: never optimal to exercise early
        return euro;
    const double st = sigma * std::sqrt(T);
    const double nn = 2.0 * b / (sigma * sigma);
    const double m = 2.0 * r / (sigma * sigma);
    const double kk = 1.0 - std::exp(-r * T);
    if (!(kk > 0))
        return euro;
    const double q2 = (-(nn - 1.0) + std::sqrt((nn - 1.0) * (nn - 1.0) + 4.0 * m / kk)) / 2.0;
    // Seed (Haug): the perpetual critical price, damped towards K for short maturities.
    const double q2u = (-(nn - 1.0) + std::sqrt((nn - 1.0) * (nn - 1.0) + 4.0 * m)) / 2.0;
    const double su = K / (1.0 - 1.0 / q2u);
    const double h2 = -(b * T + 2.0 * st) * K / (su - K);
    double si = K + (su - K) * (1.0 - std::exp(h2));
    const double cf = std::exp((b - r) * T);
    for (int it = 0; it < 100 && si > 0; ++it) {
        const double d1 = (std::log(si / K) + (b + 0.5 * sigma * sigma) * T) / st;
        const double rhs = om_gbs_price(true, si, K, T, r, b, sigma) + (1.0 - cf * om_cdf(d1)) * si / q2;
        const double lhs = si - K;
        if (std::fabs(lhs - rhs) / K < 1e-9)
            break;
        const double bi = cf * om_cdf(d1) * (1.0 - 1.0 / q2) + (1.0 - cf * om_pdf(d1) / st) / q2;
        const double next = (K + rhs - bi * si) / (1.0 - bi);
        if (!std::isfinite(next) || next <= 0)
            break;
        si = next;
    }
    if (!(si > 0) || !std::isfinite(si))
        return std::max(euro, S - K);
    if (S >= si)
        return S - K;
    const double d1 = (std::log(si / K) + (b + 0.5 * sigma * sigma) * T) / st;
    const double a2 = (si / q2) * (1.0 - cf * om_cdf(d1));
    return std::max({euro + a2 * std::pow(S / si, q2), euro, S - K});
}

double om_baw_put(double S, double K, double T, double r, double b, double sigma) {
    const double euro = om_gbs_price(false, S, K, T, r, b, sigma);
    if (r <= 0) // no interest to earn on the strike: never optimal to exercise early
        return std::max(euro, 0.0);
    const double st = sigma * std::sqrt(T);
    const double nn = 2.0 * b / (sigma * sigma);
    const double m = 2.0 * r / (sigma * sigma);
    const double kk = 1.0 - std::exp(-r * T);
    const double q1 = (-(nn - 1.0) - std::sqrt((nn - 1.0) * (nn - 1.0) + 4.0 * m / kk)) / 2.0;
    const double q1u = (-(nn - 1.0) - std::sqrt((nn - 1.0) * (nn - 1.0) + 4.0 * m)) / 2.0;
    const double su = K / (1.0 - 1.0 / q1u);
    const double h1 = (b * T - 2.0 * st) * K / (K - su);
    double si = su + (K - su) * std::exp(h1);
    const double cf = std::exp((b - r) * T);
    for (int it = 0; it < 100 && si > 0; ++it) {
        const double d1 = (std::log(si / K) + (b + 0.5 * sigma * sigma) * T) / st;
        const double rhs = om_gbs_price(false, si, K, T, r, b, sigma) - (1.0 - cf * om_cdf(-d1)) * si / q1;
        const double lhs = K - si;
        if (std::fabs(lhs - rhs) / K < 1e-9)
            break;
        const double bi = -cf * om_cdf(-d1) * (1.0 - 1.0 / q1) - (1.0 + cf * om_pdf(-d1) / st) / q1;
        const double next = (K - rhs + bi * si) / (1.0 + bi);
        if (!std::isfinite(next) || next <= 0)
            break;
        si = next;
    }
    if (!(si > 0) || !std::isfinite(si))
        return std::max(euro, K - S);
    if (S <= si)
        return K - S;
    const double d1 = (std::log(si / K) + (b + 0.5 * sigma * sigma) * T) / st;
    const double a1 = -(si / q1) * (1.0 - cf * om_cdf(-d1));
    return std::max({euro + a1 * std::pow(S / si, q1), euro, K - S});
}

VolSolve om_status(SolveStatus s) {
    VolSolve v;
    v.status = s;
    return v;
}

} // namespace

// ── Closed forms ─────────────────────────────────────────────────────────────

Greeks bsm_full(bool is_call, double S, double K, double T, double r, double q, double sigma) {
    return om_gbs(is_call, S, K, T, r, r - q, sigma);
}

double bsm_price(bool is_call, double S, double K, double T, double r, double q, double sigma) {
    return om_gbs(is_call, S, K, T, r, r - q, sigma).price;
}

Greeks black76_full(bool is_call, double F, double K, double T, double r, double sigma) {
    Greeks g = om_gbs(is_call, F, K, T, r, 0.0, sigma);
    if (g.valid) {
        // The forward is held fixed when the rate moves: only discounting reacts.
        g.rho = -std::max(T, 0.0) * g.price;
        g.phi = 0;
    }
    return g;
}

double black76_price(bool is_call, double F, double K, double T, double r, double sigma) {
    return om_gbs(is_call, F, K, T, r, 0.0, sigma).price;
}

Greeks bachelier_full(bool is_call, double F, double K, double T, double r, double sigma_n) {
    Greeks g;
    if (!om_finite(F, K, T, r, sigma_n, 0.0) || T < 0 || sigma_n < 0)
        return g;
    const double df = std::exp(-r * std::max(T, 0.0));
    const double sqrt_t = std::sqrt(std::max(T, 0.0));
    const double s = sigma_n * sqrt_t;
    const double sgn = is_call ? 1.0 : -1.0;
    if (T <= 0 || s < kMinTotalVol * std::max(1.0, std::fabs(F))) {
        const double intrinsic = std::max(sgn * (F - K), 0.0);
        g.price = df * intrinsic;
        if (intrinsic > 0) {
            g.delta = sgn * df;
            g.dual_delta = -sgn * df;
        }
        g.theta = r * g.price;
        g.rho = -std::max(T, 0.0) * g.price;
        g.valid = true;
        return g;
    }
    const double d = (F - K) / s;
    const double nd = om_pdf(d);
    const double u = sgn * (F - K) * om_cdf(sgn * d) + s * nd; // undiscounted value
    g.price = std::max(df * u, 0.0);
    g.delta = sgn * df * om_cdf(sgn * d);
    g.gamma = df * nd / s;
    g.vega = df * sqrt_t * nd;
    g.theta = r * g.price - df * nd * sigma_n / (2.0 * sqrt_t);
    g.rho = -T * g.price;
    g.vanna = -df * nd * d / sigma_n;
    g.volga = df * sqrt_t * nd * d * d / sigma_n;
    g.dual_delta = -sgn * df * om_cdf(sgn * d);
    g.dual_gamma = df * nd / s;
    g.lambda = (g.price > 1e-300 && F != 0) ? g.delta * F / g.price : 0.0;
    g.valid = true;
    return g;
}

double bachelier_price(bool is_call, double F, double K, double T, double r, double sigma_n) {
    return bachelier_full(is_call, F, K, T, r, sigma_n).price;
}

// ── Trees and American approximations ───────────────────────────────────────

double pv_dividends(const std::vector<CashDividend>& dividends, double T, double r) {
    double pv = 0;
    for (const CashDividend& d : dividends) {
        if (d.time > 0 && d.time <= T && std::isfinite(d.amount))
            pv += d.amount * std::exp(-r * d.time);
    }
    return pv;
}

TreeResult crr_price(bool is_call, bool american, double S, double K, double T, double r, double q, double sigma,
                     int steps, const std::vector<CashDividend>& dividends) {
    TreeResult out;
    if (!om_finite(S, K, T, r, q, sigma) || !(S > 0) || !(K > 0) || T < 0 || sigma < 0)
        return out;
    if (T <= 0 || sigma * std::sqrt(T) < kMinTotalVol) {
        const double pv = pv_dividends(dividends, T, r);
        const Greeks g = om_bsm_degenerate(is_call, S - pv, K, std::max(T, 0.0), r, q);
        out.price = g.price;
        out.delta = g.delta;
        out.theta = g.theta;
        if (american)
            out.price = std::max(out.price, is_call ? S - K : K - S);
        out.valid = true;
        return out;
    }
    int n = std::clamp(steps, 3, 5000);
    // u/d must straddle the drift: e^{(r−q)dt} inside (d, u). Refine the tree until it does.
    double dt = T / n;
    while (std::fabs(r - q) * std::sqrt(dt) >= sigma && n < 5000) {
        n = std::min(5000, n * 2);
        dt = T / n;
    }
    const double u = std::exp(sigma * std::sqrt(dt));
    const double d = 1.0 / u;
    const double growth = std::exp((r - q) * dt);
    const double p = (growth - d) / (u - d);
    if (!(p > 0 && p < 1))
        return out;
    const double disc = std::exp(-r * dt);
    const double pu = disc * p, pd = disc * (1.0 - p);

    // Escrowed dividends: the tree carries S* = S − PV(divs); a node at step i is worth
    // S* + PV_i, PV_i = value at t_i of the dividends still to come.
    std::vector<CashDividend> divs;
    for (const CashDividend& dv : dividends)
        if (dv.time > 0 && dv.time <= T && dv.amount > 0 && std::isfinite(dv.amount))
            divs.push_back(dv);
    auto pv_after = [&divs, r](double t) {
        double pv = 0;
        for (const CashDividend& dv : divs)
            if (dv.time > t + 1e-12)
                pv += dv.amount * std::exp(-r * (dv.time - t));
        return pv;
    };
    const double s_star = S - pv_after(0.0);
    if (!(s_star > 0))
        return out;
    const double sgn = is_call ? 1.0 : -1.0;
    const double u2 = u * u;

    auto run = [&](bool amer, double* v10, double* v11, double* v20, double* v21, double* v22) {
        std::vector<double> v(n + 1);
        double sj = s_star * std::pow(d, n);
        for (int j = 0; j <= n; ++j) {
            v[j] = std::max(sgn * (sj - K), 0.0);
            sj *= u2;
        }
        for (int i = n - 1; i >= 0; --i) {
            const double pv_i = amer ? pv_after(i * dt) : 0.0;
            double si = s_star * std::pow(d, i);
            for (int j = 0; j <= i; ++j) {
                double c = pu * v[j + 1] + pd * v[j];
                if (amer)
                    c = std::max(c, sgn * (si + pv_i - K));
                v[j] = c;
                si *= u2;
            }
            if (i == 2 && v20) {
                *v20 = v[0];
                *v21 = v[1];
                *v22 = v[2];
            }
            if (i == 1 && v10) {
                *v10 = v[0];
                *v11 = v[1];
            }
        }
        return v[0];
    };

    double f10 = 0, f11 = 0, f20 = 0, f21 = 0, f22 = 0;
    out.price = run(american, &f10, &f11, &f20, &f21, &f22);
    // Node prices including the dividends still to come (Hull ch. 21 delta / gamma / theta).
    const double s10 = s_star * d + pv_after(dt), s11 = s_star * u + pv_after(dt);
    const double s20 = s_star * d * d + pv_after(2 * dt), s21 = s_star + pv_after(2 * dt);
    const double s22 = s_star * u * u + pv_after(2 * dt);
    out.delta = (f11 - f10) / (s11 - s10);
    const double dlt_hi = (f22 - f21) / (s22 - s21);
    const double dlt_lo = (f21 - f20) / (s21 - s20);
    out.gamma = (dlt_hi - dlt_lo) / (0.5 * (s22 - s20));
    out.theta = (f21 - out.price) / (2.0 * dt);
    if (american) {
        const double euro = run(false, nullptr, nullptr, nullptr, nullptr, nullptr);
        out.early_exercise_premium = std::max(0.0, out.price - euro);
    }
    out.valid = std::isfinite(out.price);
    return out;
}

double baw_price(bool is_call, double S, double K, double T, double r, double q, double sigma) {
    if (!om_finite(S, K, T, r, q, sigma) || !(S > 0) || !(K > 0) || T < 0 || sigma < 0)
        return 0.0;
    const double intrinsic = std::max(is_call ? S - K : K - S, 0.0);
    if (T <= 0 || sigma * std::sqrt(T) < kMinTotalVol)
        return std::max(intrinsic, om_bsm_degenerate(is_call, S, K, std::max(T, 0.0), r, q).price);
    const double b = r - q;
    return is_call ? om_baw_call(S, K, T, r, b, sigma) : om_baw_put(S, K, T, r, b, sigma);
}

Greeks baw_greeks(bool is_call, double S, double K, double T, double r, double q, double sigma) {
    Greeks g;
    if (!om_finite(S, K, T, r, q, sigma) || !(S > 0) || !(K > 0) || T < 0 || sigma < 0)
        return g;
    auto px = [&](double s, double t, double rr, double v) { return baw_price(is_call, s, K, t, rr, q, v); };
    g.price = px(S, T, r, sigma);
    const double hs = S * 1e-3;
    const double up = px(S + hs, T, r, sigma), dn = px(S - hs, T, r, sigma);
    g.delta = (up - dn) / (2 * hs);
    g.gamma = (up - 2 * g.price + dn) / (hs * hs);
    const double hv = std::max(1e-4, sigma * 1e-3);
    const double v_lo = std::max(1e-6, sigma - hv);
    g.vega = (px(S, T, r, sigma + hv) - px(S, T, r, v_lo)) / (sigma + hv - v_lo);
    const double ht = std::min(T * 0.5, 1.0 / 365.0);
    g.theta = ht > 0 ? (px(S, T - ht, r, sigma) - g.price) / ht : 0.0;
    const double hr = 1e-4;
    g.rho = (px(S, T, r + hr, sigma) - px(S, T, r - hr, sigma)) / (2 * hr);
    g.lambda = g.price > 1e-300 ? g.delta * S / g.price : 0.0;
    g.valid = std::isfinite(g.price);
    return g;
}

// ── Implied volatility ───────────────────────────────────────────────────────

const char* solve_status_text(SolveStatus status) {
    switch (status) {
        case SolveStatus::Ok:
            return "solved";
        case SolveStatus::BelowIntrinsic:
            return "the price is below the option's intrinsic value - no volatility reproduces it";
        case SolveStatus::AboveMaximum:
            return "the price is above the most the option can be worth - no volatility reproduces it";
        case SolveStatus::NoSolution:
            return "no volatility in the search range reproduces the price";
        case SolveStatus::InvalidInput:
            return "spot, strike and time to expiry must be positive numbers";
    }
    return "no solution";
}

VolSolve solve_increasing(const std::function<double(double)>& price_of, double target, double lo, double hi,
                          double hi_cap, double tol) {
    VolSolve out;
    if (!std::isfinite(target) || !(hi > lo) || !(lo >= 0))
        return om_status(SolveStatus::InvalidInput);
    auto g = [&](double s) { return price_of(s) - target; };
    double glo = g(lo);
    if (!std::isfinite(glo))
        return om_status(SolveStatus::NoSolution);
    if (glo > 0) {
        // Even the lowest volatility prices above the target.
        return om_status(glo > 1e-12 * std::max(1.0, std::fabs(target)) ? SolveStatus::BelowIntrinsic
                                                                         : SolveStatus::Ok);
    }
    if (glo == 0) {
        out.sigma = lo;
        out.status = SolveStatus::Ok;
        return out;
    }
    double ghi = g(hi);
    while (std::isfinite(ghi) && ghi < 0 && hi < hi_cap) {
        lo = hi;
        glo = ghi;
        hi = std::min(hi * 2.0, hi_cap);
        ghi = g(hi);
    }
    if (!std::isfinite(ghi))
        return om_status(SolveStatus::NoSolution);
    if (ghi < 0)
        return om_status(SolveStatus::AboveMaximum);
    out.sigma = om_brent(g, lo, hi, glo, ghi, tol, &out.iterations);
    out.status = std::isfinite(out.sigma) ? SolveStatus::Ok : SolveStatus::NoSolution;
    return out;
}

VolSolve bsm_implied_vol(double price, bool is_call, double S, double K, double T, double r, double q) {
    // The rational-style solver of OptionPricing (Jäckel's transformations, bracketed Newton).
    const pricing::ImpliedVol iv = pricing::bsm_implied_vol(price, S, K, T, r, q, is_call);
    VolSolve out;
    switch (iv.status) {
        case pricing::IvStatus::Ok:
            out.sigma = iv.sigma;
            out.status = SolveStatus::Ok;
            break;
        case pricing::IvStatus::BelowIntrinsic:
            out.status = SolveStatus::BelowIntrinsic;
            break;
        case pricing::IvStatus::AboveMaximum:
            out.status = SolveStatus::AboveMaximum;
            break;
        case pricing::IvStatus::InvalidInput:
            out.status = SolveStatus::InvalidInput;
            break;
    }
    return out;
}

VolSolve black76_implied_vol(double price, bool is_call, double F, double K, double T, double r) {
    // Black-76 is BSM on S = F with q = r (zero cost of carry).
    return bsm_implied_vol(price, is_call, F, K, T, r, r);
}

VolSolve bachelier_implied_vol(double price, bool is_call, double F, double K, double T, double r) {
    if (!om_finite(price, F, K, T, r, 0.0) || !(T > 0))
        return om_status(SolveStatus::InvalidInput);
    const double df = std::exp(-r * T);
    const double intrinsic = df * std::max(is_call ? F - K : K - F, 0.0);
    if (price < intrinsic - 1e-12 * std::max(1.0, std::fabs(price)))
        return om_status(SolveStatus::BelowIntrinsic);
    if (price <= intrinsic) {
        VolSolve z;
        z.status = SolveStatus::Ok;
        return z;
    }
    // In the money → the out-of-the-money option of the other type by parity (time value
    // only): the solve then never subtracts a large intrinsic value from the model price.
    bool call = is_call;
    double target = price;
    if (intrinsic > 0) {
        target = price - intrinsic;
        call = !call;
    }
    // Initial bracket from the at-the-money value df·σ√T/√(2π).
    const double hi = 2.5066282746310002 * (target / df) / std::sqrt(T) + std::fabs(F - K) + 1e-12;
    return solve_increasing([&](double s) { return bachelier_price(call, F, K, T, r, s); }, target, 0.0, hi, hi * 1e6,
                            1e-14 * std::max(1.0, hi));
}

VolSolve american_implied_vol(double price, AmericanModel model, bool is_call, double S, double K, double T, double r,
                              double q, int steps, const std::vector<CashDividend>& dividends) {
    if (!om_finite(price, S, K, T, r, q) || !(S > 0) || !(K > 0) || !(T > 0))
        return om_status(SolveStatus::InvalidInput);
    const double intrinsic = std::max(is_call ? S - K : K - S, 0.0);
    if (price < intrinsic - 1e-9)
        return om_status(SolveStatus::BelowIntrinsic);
    if (price >= (is_call ? S : K))
        return om_status(SolveStatus::AboveMaximum);
    // BAW has no discrete dividends: with any, the tree is used.
    std::function<double(double)> f;
    if (model == AmericanModel::BaroneAdesiWhaley && dividends.empty())
        f = [&](double s) { return baw_price(is_call, S, K, T, r, q, s); };
    else
        f = [&](double s) { return crr_price(is_call, true, S, K, T, r, q, s, steps, dividends).price; };
    return solve_increasing(f, price, 1e-4, 1.0, 10.0, 1e-7);
}

} // namespace fincept::services::options::models
