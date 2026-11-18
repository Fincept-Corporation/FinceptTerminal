#include "services/options/CreditMath.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numeric>
#include <utility>

namespace fincept::services::options::credit {

namespace {

constexpr double kCmNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kCmMaxHazard = 50.0; ///< 5,000 % a year: beyond it a quote is not a credit spread any more

std::string cm_fmt(double v, int decimals) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", decimals, v);
    return buf;
}

/// Brent's root search on [a, b] with f(a), f(b) of opposite signs (Brent 1973, ch. 4).
template <typename F>
double cm_brent(F&& f, double a, double b, double fa, double fb, double tol, int max_iter) {
    double c = b, fc = fb, d = b - a, e = d;
    for (int it = 0; it < max_iter; ++it) {
        if ((fb > 0 && fc > 0) || (fb < 0 && fc < 0)) {
            c = a;
            fc = fa;
            d = b - a;
            e = d;
        }
        if (std::fabs(fc) < std::fabs(fb)) {
            a = b;
            b = c;
            c = a;
            fa = fb;
            fb = fc;
            fc = fa;
        }
        const double tol1 = 2.0 * std::numeric_limits<double>::epsilon() * std::fabs(b) + 0.5 * tol;
        const double xm = 0.5 * (c - b);
        if (std::fabs(xm) <= tol1 || fb == 0.0)
            return b;
        if (std::fabs(e) >= tol1 && std::fabs(fa) > std::fabs(fb)) {
            double p, q;
            const double s = fb / fa;
            if (a == c) {
                p = 2.0 * xm * s;
                q = 1.0 - s;
            } else {
                const double qq = fa / fc, r = fb / fc;
                p = s * (2.0 * xm * qq * (qq - r) - (b - a) * (r - 1.0));
                q = (qq - 1.0) * (r - 1.0) * (s - 1.0);
            }
            if (p > 0)
                q = -q;
            p = std::fabs(p);
            const double min1 = 3.0 * xm * q - std::fabs(tol1 * q);
            const double min2 = std::fabs(e * q);
            if (2.0 * p < std::min(min1, min2)) {
                e = d;
                d = p / q;
            } else {
                d = xm;
                e = d;
            }
        } else {
            d = xm;
            e = d;
        }
        a = b;
        fa = fb;
        b += std::fabs(d) > tol1 ? d : (xm > 0 ? tol1 : -tol1);
        fb = f(b);
    }
    return b;
}

/// ∫_0^d e^{-λu} du and ∫_0^d u e^{-λu} du (series near λd = 0).
void cm_moments(double lambda, double d, double* e1, double* e2) {
    const double x = lambda * d;
    if (std::fabs(x) < 1e-6) {
        *e1 = d * (1.0 - x / 2.0 + x * x / 6.0);
        *e2 = d * d * (0.5 - x / 3.0 + x * x / 8.0);
        return;
    }
    const double ex = std::exp(-x);
    *e1 = (1.0 - ex) / lambda;
    *e2 = (1.0 - ex * (1.0 + x)) / (lambda * lambda);
}

int cm_frequency(int f) {
    return f == 1 || f == 2 || f == 4 || f == 12 ? f : 4;
}

double cm_bp(double s) {
    return s * 1e4;
}

Bootstrap cm_fail(Bootstrap b, ErrorKind kind, int index, std::string message) {
    b.ok = false;
    b.error = kind;
    b.index = index;
    b.message = std::move(message);
    return b;
}

} // namespace

double accrual_factor(DayCount dc) {
    return dc == DayCount::Act360 ? 365.0 / 360.0 : 1.0;
}

rates::ZeroCurve flat_curve(double rate) {
    rates::ZeroCurve z;
    // push_back, not `= {x}`: GCC 12 reports the one-element initializer_list copy as
    // out of bounds (-Warray-bounds false positive, an error in CI).
    z.t.push_back(1.0);
    z.r.push_back(std::isfinite(rate) ? rate : 0.0);
    return z;
}

std::vector<double> premium_schedule(double maturity, int frequency) {
    std::vector<double> out;
    if (!(maturity > 0) || !std::isfinite(maturity))
        return out;
    const double step = 1.0 / cm_frequency(frequency);
    for (int k = 0;; ++k) {
        const double x = maturity - k * step;
        if (x <= 1e-9)
            break;
        out.push_back(x);
    }
    std::reverse(out.begin(), out.end());
    return out;
}

double HazardCurve::hazard_at(double T) const {
    if (empty())
        return 0.0;
    for (size_t k = 0; k < t.size() && k < h.size(); ++k)
        if (T <= t[k])
            return h[k];
    return h[std::min(t.size(), h.size()) - 1];
}

double HazardCurve::cumulative(double T) const {
    if (empty() || !(T > 0))
        return 0.0;
    double sum = 0, prev = 0;
    const size_t n = std::min(t.size(), h.size());
    for (size_t k = 0; k < n; ++k) {
        if (T <= t[k])
            return sum + h[k] * (T - prev);
        sum += h[k] * (t[k] - prev);
        prev = t[k];
    }
    return sum + h[n - 1] * (T - prev);
}

double HazardCurve::survival(double T) const {
    return std::exp(-cumulative(T));
}

HazardCurve flat_hazard(double h) {
    HazardCurve c;
    c.t.push_back(1.0); // not `= {x}`: see flat_curve()
    c.h.push_back(h);
    return c;
}

Legs cds_legs(const HazardCurve& hz, const rates::ZeroCurve& z, double recovery, double maturity,
              const Conventions& cv) {
    Legs out;
    if (!(maturity > 0) || hz.empty())
        return out;
    const double alpha = accrual_factor(cv.day_count);
    const bool flat_rates = z.t.size() <= 1;
    const double max_step = cv.max_step > 1e-4 ? cv.max_step : 1.0 / 24.0;
    const std::vector<double> pay = premium_schedule(maturity, cv.frequency);

    double prot_unit = 0, prem = 0, aod = 0;
    double a = 0.0, df_a = z.df(0.0), q_a = 1.0;
    double period_start = 0.0;
    std::vector<double> cuts;
    for (double e : pay) {
        // Breakpoints inside the period: hazard knots and (non-flat curve) curve knots.
        cuts.clear();
        for (double k : hz.t)
            if (k > period_start + 1e-12 && k < e - 1e-12)
                cuts.push_back(k);
        if (!flat_rates)
            for (double k : z.t)
                if (k > period_start + 1e-12 && k < e - 1e-12)
                    cuts.push_back(k);
        cuts.push_back(e);
        std::sort(cuts.begin(), cuts.end());
        for (double b_end : cuts) {
            if (b_end <= a + 1e-14)
                continue;
            // Split into pieces of at most max_step when the forward rate is not flat.
            const int pieces = flat_rates ? 1 : std::max(1, static_cast<int>(std::ceil((b_end - a) / max_step - 1e-9)));
            const double seg_a = a, seg_len = b_end - a;
            const double h = hz.hazard_at(0.5 * (seg_a + b_end));
            for (int p = 1; p <= pieces; ++p) {
                const double b = p == pieces ? b_end : seg_a + seg_len * p / pieces;
                const double d = b - a;
                const double df_b = z.df(b);
                const double f = (df_a > 0 && df_b > 0) ? std::log(df_a / df_b) / d : 0.0;
                double e1 = 0, e2 = 0;
                cm_moments(h + f, d, &e1, &e2);
                const double c = df_a * q_a;
                prot_unit += h * c * e1;
                if (cv.accrual_on_default)
                    aod += alpha * h * c * ((a - period_start) * e1 + e2);
                q_a *= std::exp(-h * d);
                df_a = df_b;
                a = b;
            }
        }
        prem += alpha * (e - period_start) * df_a * q_a; // df_a, q_a are at e now
        period_start = e;
    }
    out.protection = (1.0 - recovery) * prot_unit;
    out.accrual_on_default = aod;
    out.rpv01 = prem + aod;
    return out;
}

Bootstrap bootstrap(const std::vector<double>& tenors_in, const std::vector<double>& spreads_in, double recovery,
                    const rates::ZeroCurve& z, const Conventions& cv) {
    Bootstrap out;
    const size_t n = std::min(tenors_in.size(), spreads_in.size());
    if (n == 0)
        return cm_fail(out, ErrorKind::NoQuotes, -1, "Enter at least one tenor and spread.");
    if (!(recovery >= 0.0 && recovery < 0.99))
        return cm_fail(out, ErrorKind::BadRecovery, -1, "Recovery must be between 0% and 99%.");
    std::vector<size_t> order(n);
    std::iota(order.begin(), order.end(), size_t{0});
    for (size_t i = 0; i < n; ++i) {
        if (!(tenors_in[i] > 0) || !std::isfinite(tenors_in[i]) || tenors_in[i] > 100)
            return cm_fail(out, ErrorKind::BadTenor, static_cast<int>(i),
                           "Every tenor must be a positive number of years (100 at most).");
        if (!(spreads_in[i] > 0) || !std::isfinite(spreads_in[i]))
            return cm_fail(out, ErrorKind::BadSpread, static_cast<int>(i), "Every spread must be positive.");
    }
    std::stable_sort(order.begin(), order.end(), [&](size_t x, size_t y) { return tenors_in[x] < tenors_in[y]; });
    for (size_t i = 0; i < n; ++i) {
        out.tenors.push_back(tenors_in[order[i]]);
        out.spreads.push_back(spreads_in[order[i]]);
        if (i > 0 && std::fabs(out.tenors[i] - out.tenors[i - 1]) < 1e-9)
            return cm_fail(out, ErrorKind::DuplicateTenor, static_cast<int>(i),
                           "The tenor " + cm_fmt(out.tenors[i], 2) + "y is listed twice.");
    }

    HazardCurve& c = out.curve;
    for (size_t k = 0; k < n; ++k) {
        const double T = out.tenors[k], S = out.spreads[k];
        c.t.push_back(T);
        c.h.push_back(0.0);
        auto g = [&](double h) {
            c.h.back() = h;
            const Legs l = cds_legs(c, z, recovery, T, cv);
            return l.protection - S * l.rpv01;
        };
        const double g0 = g(0.0);
        double h = 0.0;
        if (g0 > 1e-15) {
            return cm_fail(out, ErrorKind::NegativeHazard, static_cast<int>(k),
                           "The " + cm_fmt(T, 2) + "y spread (" + cm_fmt(cm_bp(S), 2) +
                               " bp) is too low after the shorter tenors: fitting it would need a negative "
                               "default intensity.");
        }
        if (g0 < 0) {
            double hi = std::max(1e-4, 2.0 * S / std::max(1.0 - recovery, 0.01));
            double ghi = g(hi);
            while (!(ghi > 0) && hi < kCmMaxHazard) {
                hi = std::min(hi * 2.0, kCmMaxHazard);
                ghi = g(hi);
            }
            if (!(ghi > 0))
                return cm_fail(out, ErrorKind::NoSolution, static_cast<int>(k),
                               "No default intensity reproduces the " + cm_fmt(T, 2) + "y spread.");
            h = cm_brent(g, 0.0, hi, g0, ghi, 1e-16, 300);
        }
        c.h.back() = h;
    }
    for (size_t k = 0; k < n; ++k) {
        const Legs l = cds_legs(c, z, recovery, out.tenors[k], cv);
        out.repriced.push_back(l.par_spread());
        out.rpv01.push_back(l.rpv01);
    }
    out.ok = true;
    return out;
}

Bootstrap flat_from_spread(double spread, double maturity, double recovery, const rates::ZeroCurve& z,
                           const Conventions& cv) {
    if (!(maturity > 0) || !std::isfinite(maturity))
        return cm_fail(Bootstrap{}, ErrorKind::BadMaturity, -1, "The maturity must be a positive number of years.");
    return bootstrap({maturity}, {spread}, recovery, z, cv);
}

Conversion upfront_from_spread(double spread, double coupon, double maturity, double recovery,
                               const rates::ZeroCurve& z, const Conventions& cv) {
    Conversion out;
    const Bootstrap b = flat_from_spread(spread, maturity, recovery, z, cv);
    if (!b.ok) {
        out.error = b.error;
        out.message = b.message;
        return out;
    }
    const Legs l = cds_legs(b.curve, z, recovery, maturity, cv);
    out.ok = true;
    out.spread = spread;
    out.rpv01 = l.rpv01;
    out.hazard = b.curve.h.front();
    out.upfront = l.protection - coupon * l.rpv01; // = (S - C) x RPV01
    return out;
}

Conversion spread_from_upfront(double upfront, double coupon, double maturity, double recovery,
                               const rates::ZeroCurve& z, const Conventions& cv) {
    Conversion out;
    if (!(maturity > 0) || !std::isfinite(maturity)) {
        out.error = ErrorKind::BadMaturity;
        out.message = "The maturity must be a positive number of years.";
        return out;
    }
    if (!(recovery >= 0.0 && recovery < 0.99)) {
        out.error = ErrorKind::BadRecovery;
        out.message = "Recovery must be between 0% and 99%.";
        return out;
    }
    if (!std::isfinite(upfront) || !std::isfinite(coupon)) {
        out.error = ErrorKind::UpfrontRange;
        out.message = "Enter a finite upfront and coupon.";
        return out;
    }
    auto f = [&](double s) {
        const Conversion c = upfront_from_spread(s, coupon, maturity, recovery, z, cv);
        return c.ok ? c.upfront - upfront : kCmNaN;
    };
    double lo = 1e-7;
    const double flo = f(lo);
    if (!(flo < 0)) {
        out.error = ErrorKind::UpfrontRange;
        out.message = "An upfront this low at this coupon would need a negative spread.";
        return out;
    }
    double hi = std::max(0.01, coupon * 2.0 + 0.01);
    double fhi = f(hi);
    while (std::isfinite(fhi) && !(fhi > 0) && hi < 5.0) {
        lo = hi;
        hi = std::min(hi * 2.0, 5.0);
        fhi = f(hi);
    }
    if (!(fhi > 0)) {
        out.error = ErrorKind::UpfrontRange;
        out.message = "No spread gives an upfront this high (it cannot exceed 1 - recovery).";
        return out;
    }
    const double flo2 = f(lo);
    const double s = cm_brent(f, lo, hi, flo2, fhi, 1e-14, 300);
    out = upfront_from_spread(s, coupon, maturity, recovery, z, cv);
    return out;
}

Risk position_risk(const std::vector<double>& tenors, const std::vector<double>& spreads, double recovery,
                   const rates::ZeroCurve& z, const Conventions& cv, const Position& pos,
                   const std::function<bool()>& cancelled) {
    Risk out;
    auto stop = [&]() { return cancelled && cancelled(); };
    if (!(pos.maturity > 0) || !std::isfinite(pos.maturity) || pos.maturity > 100) {
        out.error = ErrorKind::BadMaturity;
        out.message = "The maturity must be a positive number of years.";
        return out;
    }
    out.fit = bootstrap(tenors, spreads, recovery, z, cv);
    if (!out.fit.ok) {
        out.error = out.fit.error;
        out.index = out.fit.index;
        out.message = out.fit.message;
        return out;
    }
    const double N = pos.notional;
    const double side = pos.buy_protection ? 1.0 : -1.0;
    auto pv_on = [&](const HazardCurve& c, const rates::ZeroCurve& zz, double rr) {
        const Legs l = cds_legs(c, zz, rr, pos.maturity, cv);
        return side * N * (l.protection - pos.coupon * l.rpv01);
    };
    const Legs l = cds_legs(out.fit.curve, z, recovery, pos.maturity, cv);
    out.par_spread = l.par_spread();
    out.rpv01 = l.rpv01;
    out.protection_leg = N * l.protection;
    out.premium_leg = N * pos.coupon * l.rpv01;
    out.upfront = l.protection - pos.coupon * l.rpv01;
    out.upfront_amount = N * out.upfront;
    out.pv = side * out.upfront_amount;
    out.survival = out.fit.curve.survival(pos.maturity);
    out.default_probability = 1.0 - out.survival;
    out.jtd = side * N * (1.0 - recovery) - out.pv;

    // Re-bootstrapped bumps: the curve is refitted so the quotes still price to par.
    auto bumped_pv = [&](const std::vector<double>& s, double rr, const rates::ZeroCurve& zz) {
        const Bootstrap b = bootstrap(tenors, s, rr, zz, cv);
        return b.ok ? pv_on(b.curve, zz, rr) : kCmNaN;
    };
    auto cancelled_result = [&out]() {
        out.ok = false;
        out.error = ErrorKind::Cancelled;
        out.message = "Cancelled.";
        return out;
    };
    std::vector<double> up = spreads;
    for (double& s : up)
        s += 1e-4;
    if (stop())
        return cancelled_result();
    out.cs01 = bumped_pv(up, recovery, z) - out.pv;
    if (stop())
        return cancelled_result();
    out.recovery01 = recovery + 0.01 < 0.99 ? bumped_pv(spreads, recovery + 0.01, z) - out.pv : kCmNaN;
    if (stop())
        return cancelled_result();
    out.ir_dv01 = bumped_pv(spreads, recovery, rates::shifted(z, 1.0)) - out.pv;
    // Per-quote buckets, in the caller's order of the quotes.
    const size_t n = std::min(tenors.size(), spreads.size());
    out.bucket_cs01.assign(n, kCmNaN);
    for (size_t i = 0; i < n; ++i) {
        if (stop())
            return cancelled_result();
        std::vector<double> b = spreads;
        b[i] += 1e-4;
        out.bucket_cs01[i] = bumped_pv(b, recovery, z) - out.pv;
    }
    out.ok = true;
    return out;
}

} // namespace fincept::services::options::credit
