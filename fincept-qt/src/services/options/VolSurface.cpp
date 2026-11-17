#include "services/options/VolSurface.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>

namespace fincept::services::options::vol {

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

struct VsData {
    const std::vector<double>* k = nullptr;
    std::vector<double> w;   // total variance
    std::vector<double> wt;  // weights (normalised to sum 1)
};

struct VsInner {
    double a = 0, d = 0, c = 0; // w = a + d·y + c·√(y²+1), y = (k − m)/s
    double sse = std::numeric_limits<double>::infinity();
};

/// Solve the 3×3 symmetric system M·x = v by Gaussian elimination with partial pivoting.
bool vs_solve3(std::array<std::array<double, 3>, 3> M, std::array<double, 3> v, std::array<double, 3>& x) {
    for (int col = 0; col < 3; ++col) {
        int piv = col;
        for (int r = col + 1; r < 3; ++r)
            if (std::fabs(M[r][col]) > std::fabs(M[piv][col]))
                piv = r;
        if (std::fabs(M[piv][col]) < 1e-300)
            return false;
        std::swap(M[piv], M[col]);
        std::swap(v[piv], v[col]);
        for (int r = col + 1; r < 3; ++r) {
            const double f = M[r][col] / M[col][col];
            for (int c = col; c < 3; ++c)
                M[r][c] -= f * M[col][c];
            v[r] -= f * v[col];
        }
    }
    for (int r = 2; r >= 0; --r) {
        double s = v[r];
        for (int c = r + 1; c < 3; ++c)
            s -= M[r][c] * x[c];
        x[r] = s / M[r][r];
    }
    return std::isfinite(x[0]) && std::isfinite(x[1]) && std::isfinite(x[2]);
}

double vs_sse(const VsData& data, double m, double s, double a, double d, double c) {
    double sse = 0;
    const auto& k = *data.k;
    for (size_t i = 0; i < k.size(); ++i) {
        const double y = (k[i] - m) / s;
        const double e = a + d * y + c * std::sqrt(y * y + 1.0) - data.w[i];
        sse += data.wt[i] * e * e;
    }
    return sse;
}

/// Weighted least squares in (a, d, c) for fixed (m, s), then projected onto the
/// admissible set: 0 ≤ c ≤ 2s, |d| ≤ min(c, 2s − c) (Roger Lee's wing bound
/// b(1 + |ρ|) ≤ 2) and a ≥ −√(c² − d²) (non-negative minimum variance).
VsInner vs_inner(const VsData& data, double m, double s) {
    VsInner out;
    if (!(s > 0) || !std::isfinite(m))
        return out;
    std::array<std::array<double, 3>, 3> M{};
    std::array<double, 3> v{};
    const auto& k = *data.k;
    for (size_t i = 0; i < k.size(); ++i) {
        const double y = (k[i] - m) / s;
        const double z = std::sqrt(y * y + 1.0);
        const double f[3] = {1.0, y, z};
        const double wi = data.wt[i];
        for (int r = 0; r < 3; ++r) {
            v[r] += wi * f[r] * data.w[i];
            for (int c = 0; c < 3; ++c)
                M[r][c] += wi * f[r] * f[c];
        }
    }
    std::array<double, 3> x{};
    if (!vs_solve3(M, v, x))
        return out;
    double a = x[0], d = x[1], c = x[2];
    const double c_max = 2.0 * s;
    bool projected = false;
    if (c < 0 || c > c_max) {
        c = std::clamp(c, 0.0, c_max);
        projected = true;
    }
    const double d_max = std::min(c, c_max - c);
    if (std::fabs(d) > d_max) {
        d = std::clamp(d, -d_max, d_max);
        projected = true;
    }
    if (projected) {
        // Best level for the projected slope / curvature.
        double num = 0;
        for (size_t i = 0; i < k.size(); ++i) {
            const double y = (k[i] - m) / s;
            num += data.wt[i] * (data.w[i] - d * y - c * std::sqrt(y * y + 1.0));
        }
        a = num; // weights sum to 1
    }
    a = std::max(a, -std::sqrt(std::max(c * c - d * d, 0.0)));
    out.a = a;
    out.d = d;
    out.c = c;
    out.sse = vs_sse(data, m, s, a, d, c);
    return out;
}

/// Nelder-Mead over (m, ln s) of the inner least-squares error.
std::pair<double, double> vs_nelder_mead(const VsData& data, double m0, double s0, double k_lo, double k_hi,
                                         double* best_sse) {
    auto f = [&](const std::array<double, 2>& p) {
        const double m = p[0];
        const double s = std::exp(p[1]);
        if (m < k_lo - 1.0 || m > k_hi + 1.0 || s < 1e-4 || s > 5.0)
            return std::numeric_limits<double>::infinity();
        return vs_inner(data, m, s).sse;
    };
    std::array<std::array<double, 2>, 3> x = {{{m0, std::log(s0)}, {m0 + 0.05, std::log(s0)}, {m0, std::log(s0) + 0.5}}};
    std::array<double, 3> fx = {f(x[0]), f(x[1]), f(x[2])};
    for (int it = 0; it < 400; ++it) {
        // order
        std::array<int, 3> idx = {0, 1, 2};
        std::sort(idx.begin(), idx.end(), [&](int i, int j) { return fx[i] < fx[j]; });
        const auto xb = x[idx[0]], xm = x[idx[1]], xw = x[idx[2]];
        const double fb = fx[idx[0]], fm = fx[idx[1]], fw = fx[idx[2]];
        if (std::isfinite(fw) && std::fabs(fw - fb) <= 1e-14 * (std::fabs(fb) + 1e-20) &&
            std::fabs(xw[0] - xb[0]) + std::fabs(xw[1] - xb[1]) < 1e-9)
            break;
        const std::array<double, 2> c = {(xb[0] + xm[0]) / 2, (xb[1] + xm[1]) / 2};
        auto at = [&](double t) { return std::array<double, 2>{c[0] + t * (xw[0] - c[0]), c[1] + t * (xw[1] - c[1])}; };
        const auto xr = at(-1.0);
        const double fr = f(xr);
        std::array<std::array<double, 2>, 3> nx = {xb, xm, xw};
        std::array<double, 3> nf = {fb, fm, fw};
        if (fr < fb) {
            const auto xe = at(-2.0);
            const double fe = f(xe);
            if (fe < fr) {
                nx[2] = xe;
                nf[2] = fe;
            } else {
                nx[2] = xr;
                nf[2] = fr;
            }
        } else if (fr < fm) {
            nx[2] = xr;
            nf[2] = fr;
        } else {
            const auto xc = fr < fw ? at(-0.5) : at(0.5);
            const double fc = f(xc);
            if (fc < std::min(fr, fw)) {
                nx[2] = xc;
                nf[2] = fc;
            } else { // shrink towards the best
                for (int j = 1; j < 3; ++j) {
                    nx[j] = {xb[0] + 0.5 * (nx[j][0] - xb[0]), xb[1] + 0.5 * (nx[j][1] - xb[1])};
                    nf[j] = f(nx[j]);
                }
            }
        }
        x = nx;
        fx = nf;
    }
    int best = 0;
    for (int j = 1; j < 3; ++j)
        if (fx[j] < fx[best])
            best = j;
    *best_sse = fx[best];
    return {x[best][0], std::exp(x[best][1])};
}

} // namespace

double svi_w(const SviParams& p, double k) {
    const double x = k - p.m;
    return p.a + p.b * (p.rho * x + std::sqrt(x * x + p.sigma * p.sigma));
}

double svi_w1(const SviParams& p, double k) {
    const double x = k - p.m;
    return p.b * (p.rho + x / std::sqrt(x * x + p.sigma * p.sigma));
}

double svi_w2(const SviParams& p, double k) {
    const double x = k - p.m;
    const double r = x * x + p.sigma * p.sigma;
    return p.b * p.sigma * p.sigma / (r * std::sqrt(r));
}

double svi_vol(const SviParams& p, double k, double T) {
    const double w = svi_w(p, k);
    return (w > 0 && T > 0) ? std::sqrt(w / T) : 0.0;
}

SviFit fit_svi(const std::vector<double>& k_in, const std::vector<double>& iv_in, const std::vector<double>& wt_in,
               double T) {
    SviFit out;
    if (!(T > 0) || !std::isfinite(T)) {
        out.message = "the expiry has no time left";
        return out;
    }
    std::vector<double> k;
    VsData data;
    double wsum = 0;
    for (size_t i = 0; i < k_in.size() && i < iv_in.size(); ++i) {
        const double w = i < wt_in.size() ? wt_in[i] : 1.0;
        if (!std::isfinite(k_in[i]) || !std::isfinite(iv_in[i]) || !(iv_in[i] > 0) || !(w > 0) || !std::isfinite(w))
            continue;
        k.push_back(k_in[i]);
        data.w.push_back(iv_in[i] * iv_in[i] * T);
        data.wt.push_back(w);
        wsum += w;
    }
    out.points = static_cast<int>(k.size());
    if (k.size() < 5) {
        out.message = "fewer than 5 usable quotes";
        return out;
    }
    const auto [kmin_it, kmax_it] = std::minmax_element(k.begin(), k.end());
    const double k_lo = *kmin_it, k_hi = *kmax_it;
    if (k_hi - k_lo < 0.01) {
        out.message = "the quotes cover too narrow a strike range";
        return out;
    }
    for (double& w : data.wt)
        w /= wsum;
    data.k = &k;

    // Starts: the vertex near the lowest variance, and the money.
    const size_t i_min = static_cast<size_t>(std::min_element(data.w.begin(), data.w.end()) - data.w.begin());
    double best_sse = std::numeric_limits<double>::infinity();
    double best_m = 0, best_s = 0.1;
    for (double m0 : {k[i_min], 0.0}) {
        for (double s0 : {0.05, 0.25}) {
            double sse = 0;
            const auto [m, s] = vs_nelder_mead(data, m0, s0, k_lo, k_hi, &sse);
            if (sse < best_sse) {
                best_sse = sse;
                best_m = m;
                best_s = s;
            }
        }
    }
    if (!std::isfinite(best_sse)) {
        out.message = "the fit did not converge";
        return out;
    }
    const VsInner in = vs_inner(data, best_m, best_s);
    SviParams p;
    p.m = best_m;
    p.sigma = best_s;
    p.b = in.c / best_s;
    p.rho = in.c > 0 ? std::clamp(in.d / in.c, -0.999999, 0.999999) : 0.0;
    p.a = in.a;
    out.params = p;

    double se = 0, max_err = 0;
    for (size_t i = 0; i < k.size(); ++i) {
        const double fit = svi_vol(p, k[i], T);
        const double e = fit - std::sqrt(data.w[i] / T);
        se += e * e;
        max_err = std::max(max_err, std::fabs(e));
    }
    out.rmse_vol = std::sqrt(se / static_cast<double>(k.size()));
    out.max_err_vol = max_err;
    out.ok = std::isfinite(out.rmse_vol) && p.b >= 0;
    if (!out.ok)
        out.message = "the fit did not converge";
    return out;
}

double svi_density_g(const SviParams& p, double k) {
    const double w = svi_w(p, k);
    if (!(w > 0))
        return -1.0; // zero / negative variance is itself an arbitrage
    const double w1 = svi_w1(p, k);
    const double w2 = svi_w2(p, k);
    const double t = 1.0 - k * w1 / (2.0 * w);
    return t * t - (w1 * w1 / 4.0) * (1.0 / w + 0.25) + w2 / 2.0;
}

ArbitrageCheck butterfly_check(const SviParams& p, double k_lo, double k_hi, int n) {
    ArbitrageCheck out;
    n = std::max(n, 2);
    for (int i = 0; i < n; ++i) {
        const double k = k_lo + (k_hi - k_lo) * i / (n - 1);
        const double g = svi_density_g(p, k);
        if (g < -1e-9) {
            ++out.violations;
            if (g < out.worst) {
                out.worst = g;
                out.worst_k = k;
            }
        }
    }
    out.ok = out.violations == 0;
    return out;
}

ArbitrageCheck calendar_check(const SviParams& near, const SviParams& far, double k_lo, double k_hi, int n) {
    ArbitrageCheck out;
    n = std::max(n, 2);
    for (int i = 0; i < n; ++i) {
        const double k = k_lo + (k_hi - k_lo) * i / (n - 1);
        const double diff = svi_w(far, k) - svi_w(near, k);
        if (diff < -1e-9) {
            ++out.violations;
            if (diff < out.worst) {
                out.worst = diff;
                out.worst_k = k;
            }
        }
    }
    out.ok = out.violations == 0;
    return out;
}

double interp_linear(const std::vector<double>& x, const std::vector<double>& y, double x0, bool extrapolate_flat) {
    const size_t n = std::min(x.size(), y.size());
    if (n == 0 || !std::isfinite(x0))
        return kNaN;
    if (n == 1)
        return extrapolate_flat ? y[0] : (x0 == x[0] ? y[0] : kNaN);
    if (x0 <= x[0])
        return (x0 == x[0] || extrapolate_flat) ? y[0] : kNaN;
    if (x0 >= x[n - 1])
        return (x0 == x[n - 1] || extrapolate_flat) ? y[n - 1] : kNaN;
    const auto it = std::upper_bound(x.begin(), x.begin() + static_cast<long>(n), x0);
    const size_t j = static_cast<size_t>(it - x.begin());
    const size_t i = j - 1;
    const double t = (x0 - x[i]) / (x[j] - x[i]);
    return y[i] + t * (y[j] - y[i]);
}

SmileStats smile_stats(const std::vector<SmileQuote>& otm, double spot, double T) {
    SmileStats out;
    std::vector<SmileQuote> q;
    for (const SmileQuote& s : otm)
        if (std::isfinite(s.k) && std::isfinite(s.iv) && s.iv > 0)
            q.push_back(s);
    if (q.empty())
        return out;
    std::sort(q.begin(), q.end(), [](const SmileQuote& a, const SmileQuote& b) { return a.k < b.k; });
    std::vector<double> ks, ivs;
    for (const SmileQuote& s : q) {
        if (!ks.empty() && s.k == ks.back())
            continue;
        ks.push_back(s.k);
        ivs.push_back(s.iv);
    }
    out.atm_iv = interp_linear(ks, ivs, 0.0, true);

    auto at_delta = [&q](bool calls, double target) {
        std::vector<std::pair<double, double>> pts; // (delta, iv)
        for (const SmileQuote& s : q)
            if (s.is_call == calls && std::isfinite(s.fwd_delta))
                pts.emplace_back(s.fwd_delta, s.iv);
        std::sort(pts.begin(), pts.end());
        std::vector<double> dx, dy;
        for (const auto& [d, v] : pts) {
            if (!dx.empty() && d == dx.back())
                continue;
            dx.push_back(d);
            dy.push_back(v);
        }
        return interp_linear(dx, dy, target, false);
    };
    const double c25 = at_delta(true, 0.25);
    const double p25 = at_delta(false, -0.25);
    if (std::isfinite(c25) && std::isfinite(p25)) {
        out.iv_25c = c25;
        out.iv_25p = p25;
        out.rr25 = c25 - p25;
        out.bf25 = 0.5 * (c25 + p25) - out.atm_iv;
        out.has_25d = true;
    }
    if (std::isfinite(out.atm_iv) && T > 0 && spot > 0)
        out.move_1sd = spot * out.atm_iv * std::sqrt(T);
    if (!std::isfinite(out.atm_iv))
        out.atm_iv = 0;
    return out;
}

double forward_vol(double atm_iv1, double T1, double atm_iv2, double T2) {
    if (!(T2 > T1) || !(T1 >= 0) || !(atm_iv1 >= 0) || !(atm_iv2 >= 0))
        return -1.0;
    const double var = (atm_iv2 * atm_iv2 * T2 - atm_iv1 * atm_iv1 * T1) / (T2 - T1);
    return var >= 0 ? std::sqrt(var) : -1.0;
}

} // namespace fincept::services::options::vol
