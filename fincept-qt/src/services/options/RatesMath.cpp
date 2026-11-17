#include "services/options/RatesMath.h"

#include "services/options/OptionModels.h"

#include <algorithm>
#include <cmath>

namespace fincept::services::options::rates {

namespace {

double rm_interp(const std::vector<double>& x, const std::vector<double>& y, double x0) {
    const size_t n = std::min(x.size(), y.size());
    if (n == 0)
        return 0.0;
    if (x0 <= x[0] || n == 1)
        return y[0];
    if (x0 >= x[n - 1])
        return y[n - 1];
    const auto it = std::upper_bound(x.begin(), x.begin() + static_cast<long>(n), x0);
    const size_t j = static_cast<size_t>(it - x.begin());
    const double w = (x0 - x[j - 1]) / (x[j] - x[j - 1]);
    return y[j - 1] + w * (y[j] - y[j - 1]);
}

} // namespace

double bey_to_continuous(double y_pct) {
    return 2.0 * std::log1p(y_pct / 200.0);
}

double ZeroCurve::rate_at(double T) const {
    return rm_interp(t, r, T);
}

double ZeroCurve::df(double T) const {
    return std::exp(-rate_at(T) * std::max(T, 0.0));
}

ZeroCurve bootstrap_par_curve(const std::vector<double>& tenors_in, const std::vector<double>& par_in) {
    ZeroCurve out;
    std::vector<std::pair<double, double>> pts;
    for (size_t i = 0; i < tenors_in.size() && i < par_in.size(); ++i)
        if (tenors_in[i] > 0 && std::isfinite(tenors_in[i]) && std::isfinite(par_in[i]))
            pts.emplace_back(tenors_in[i], par_in[i]);
    std::sort(pts.begin(), pts.end());
    if (pts.size() < 2)
        return out;
    std::vector<double> ten, par;
    for (const auto& [t, y] : pts) {
        ten.push_back(t);
        par.push_back(y);
    }
    // Bills: zero-coupon bond-equivalent yields.
    for (size_t i = 0; i < ten.size() && ten[i] <= 1.0 + 1e-9; ++i) {
        out.t.push_back(ten[i]);
        out.r.push_back(bey_to_continuous(par[i]));
    }
    if (ten.back() <= 1.0 + 1e-9)
        return out;
    // Coupon bonds: semi-annual grid from 0.5y to the last tenor, par yield interpolated.
    // Discount factors on the grid up to 1y come from the bill zero rates above.
    const int n = static_cast<int>(std::floor(ten.back() * 2.0 + 1e-9));
    double sum_df = 0;
    for (int k = 1; k <= n; ++k) {
        const double tk = 0.5 * k;
        double dfk;
        if (tk <= 1.0 + 1e-9 && !out.t.empty()) {
            dfk = std::exp(-rm_interp(out.t, out.r, tk) * tk);
        } else {
            const double c = rm_interp(ten, par, tk) / 100.0; // annual coupon rate
            dfk = (1.0 - 0.5 * c * sum_df) / (1.0 + 0.5 * c);
            if (!(dfk > 0))
                break; // a nonsensical curve: keep what was bootstrapped so far
            out.t.push_back(tk);
            out.r.push_back(-std::log(dfk) / tk);
        }
        sum_df += dfk;
    }
    // Keep times ascending and unique.
    std::vector<std::pair<double, double>> z;
    for (size_t i = 0; i < out.t.size(); ++i)
        z.emplace_back(out.t[i], out.r[i]);
    std::sort(z.begin(), z.end());
    z.erase(std::unique(z.begin(), z.end(), [](const auto& a, const auto& b) { return std::fabs(a.first - b.first) < 1e-9; }),
            z.end());
    out.t.clear();
    out.r.clear();
    for (const auto& [t, r] : z) {
        out.t.push_back(t);
        out.r.push_back(r);
    }
    return out;
}

double forward_rate(const ZeroCurve& c, double t1, double t2) {
    if (!(t2 > t1))
        return c.rate_at(t1);
    return (c.rate_at(t2) * t2 - c.rate_at(t1) * t1) / (t2 - t1);
}

ZeroCurve shifted(const ZeroCurve& c, double bp) {
    ZeroCurve s = c;
    for (double& r : s.r)
        r += bp / 10000.0;
    return s;
}

namespace {

/// Payment times of a schedule from `start` to `end` at `freq` per year, rolled
/// backward from the end (a short first period is kept).
std::vector<double> rm_schedule(double start, double end, int freq) {
    std::vector<double> t;
    freq = std::clamp(freq, 1, 12);
    const double step = 1.0 / freq;
    for (double x = end; x > start + 1e-9; x -= step)
        t.push_back(x);
    std::reverse(t.begin(), t.end());
    return t;
}

SwapResult rm_swap(const ZeroCurve& c, double start, double tenor, double k, int freq, double notional, bool payer) {
    SwapResult out;
    const double end = start + tenor;
    const std::vector<double> pay = rm_schedule(start, end, freq);
    double prev = start;
    for (double t : pay) {
        out.annuity += (t - prev) * c.df(t);
        prev = t;
    }
    out.payments = static_cast<int>(pay.size());
    const double flt = c.df(start) - c.df(end);
    out.par_rate = out.annuity > 0 ? flt / out.annuity : 0.0;
    out.fixed_pv = notional * k * out.annuity;
    out.float_pv = notional * flt;
    out.pv = (payer ? 1.0 : -1.0) * (out.float_pv - out.fixed_pv);
    return out;
}

} // namespace

SwapResult swap_on_curve(const ZeroCurve& c, double start, double tenor, double fixed_rate, int freq, double notional,
                         bool payer) {
    SwapResult out = rm_swap(c, start, tenor, fixed_rate, freq, notional, payer);
    if (tenor <= 0 || c.empty())
        return out;
    const double up = rm_swap(shifted(c, 1.0), start, tenor, fixed_rate, freq, notional, payer).pv;
    const double dn = rm_swap(shifted(c, -1.0), start, tenor, fixed_rate, freq, notional, payer).pv;
    out.dv01 = 0.5 * (up - dn);
    return out;
}

double fra_value(const ZeroCurve& c, double t1, double t2, double strike, double notional, bool pay_fixed, double* fwd) {
    if (!(t2 > t1))
        return 0.0;
    const double tau = t2 - t1;
    const double f = (c.df(t1) / c.df(t2) - 1.0) / tau;
    if (fwd)
        *fwd = f;
    return (pay_fixed ? 1.0 : -1.0) * notional * tau * (f - strike) * c.df(t2);
}

namespace {

CapResult rm_cap(const ZeroCurve& c, double start, double tenor, double k, double vol, int freq, double notional,
                 bool cap, bool normal_vol) {
    CapResult out;
    const std::vector<double> pay = rm_schedule(start, start + tenor, freq);
    double prev = start;
    for (double t : pay) {
        const double t_fix = prev;
        const double tau = t - prev;
        prev = t;
        if (t_fix <= 1e-9)
            continue; // already fixed: not an option any more
        const double f = (c.df(t_fix) / c.df(t) - 1.0) / tau;
        // Black-76 (or Bachelier) on the forward, discounted to the payment date.
        const double d = c.df(t);
        double undiscounted;
        if (normal_vol)
            undiscounted = models::bachelier_price(cap, f, k, t_fix, 0.0, vol);
        else
            undiscounted = (f > 0 && k > 0) ? models::black76_price(cap, f, k, t_fix, 0.0, vol)
                                            : std::max(cap ? f - k : k - f, 0.0);
        const double v = notional * tau * d * undiscounted;
        out.caplet_values.push_back(v);
        out.forwards.push_back(f);
        out.value += v;
    }
    return out;
}

} // namespace

CapResult cap_floor(const ZeroCurve& c, double start, double tenor, double strike, double vol, int freq,
                    double notional, bool cap, bool normal_vol) {
    CapResult out = rm_cap(c, start, tenor, strike, vol, freq, notional, cap, normal_vol);
    if (c.empty() || tenor <= 0)
        return out;
    const double up = rm_cap(shifted(c, 1.0), start, tenor, strike, vol, freq, notional, cap, normal_vol).value;
    const double dn = rm_cap(shifted(c, -1.0), start, tenor, strike, vol, freq, notional, cap, normal_vol).value;
    out.dv01 = 0.5 * (up - dn);
    return out;
}

// ── D6 additions ─────────────────────────────────────────────────────────────

double swap_pv(const ZeroCurve& c, double start, double tenor, double fixed_rate, int freq, double notional,
               bool payer) {
    return rm_swap(c, start, tenor, fixed_rate, freq, notional, payer).pv;
}

KeyRateResult key_rate_dv01(const std::vector<double>& tenors, const std::vector<double>& par_pct,
                            const CurvePricer& pv, const std::atomic<bool>* cancel) {
    KeyRateResult out;
    const size_t n = std::min(tenors.size(), par_pct.size());
    if (n < 2 || !pv)
        return out;
    const std::vector<double> ten(tenors.begin(), tenors.begin() + static_cast<long>(n));
    const std::vector<double> base(par_pct.begin(), par_pct.begin() + static_cast<long>(n));
    out.base_pv = pv(bootstrap_par_curve(ten, base));
    constexpr double kBumpPct = 0.01; // 1 bp in percent
    auto bumped_pv = [&](long i, double sign) {
        std::vector<double> p = base;
        if (i < 0) {
            for (double& y : p)
                y += sign * kBumpPct;
        } else {
            p[static_cast<size_t>(i)] += sign * kBumpPct;
        }
        return pv(bootstrap_par_curve(ten, p));
    };
    for (size_t i = 0; i < n; ++i) {
        if (cancel && cancel->load()) {
            out.cancelled = true;
            return out;
        }
        out.tenors.push_back(ten[i]);
        const double d = 0.5 * (bumped_pv(static_cast<long>(i), 1.0) - bumped_pv(static_cast<long>(i), -1.0));
        out.dv01.push_back(d);
        out.total += d;
    }
    out.parallel = 0.5 * (bumped_pv(-1, 1.0) - bumped_pv(-1, -1.0));
    return out;
}

namespace {

/// Premium and annuity of a swaption on curve `c` (no risk figures).
SwaptionResult rm_swaption(const ZeroCurve& c, double expiry, double tenor, double k, double vol, int freq,
                           double notional, bool payer, bool normal_vol) {
    SwaptionResult out;
    const SwapResult s = rm_swap(c, expiry, tenor, k, freq, 1.0, true);
    out.annuity = s.annuity;
    out.forward_rate = s.par_rate;
    out.payments = s.payments;
    if (!(s.annuity > 0) || !(tenor > 0)) {
        out.message = "The swap has no payments: check the expiry and the tenor.";
        return out;
    }
    const double F = s.par_rate;
    const double scale = notional * s.annuity;
    out.intrinsic = scale * std::max(payer ? F - k : k - F, 0.0);
    const double T = std::max(expiry, 0.0);
    if (T <= 0) {
        out.premium = out.intrinsic;
        out.ok = true;
        return out;
    }
    if (normal_vol) {
        const models::Greeks g = models::bachelier_full(payer, F, k, T, 0.0, std::max(vol, 0.0));
        out.premium = scale * g.price;
        out.delta_bp = scale * g.delta * 1e-4;
        out.vega = scale * g.vega * 1e-4; // per 1 bp of normal vol
        out.ok = g.valid || std::isfinite(g.price);
        return out;
    }
    if (!(F > 0) || !(k > 0)) {
        out.message = "Black-76 needs a positive forward swap rate and strike; use the Bachelier (normal) model.";
        return out;
    }
    const models::Greeks g = models::black76_full(payer, F, k, T, 0.0, std::max(vol, 0.0));
    out.premium = scale * g.price;
    out.delta_bp = scale * g.delta * 1e-4;
    out.vega = scale * g.vega * 0.01; // per 1 lognormal vol point
    out.ok = std::isfinite(g.price);
    return out;
}

/// Payment times and amounts (per `face`) of a fixed-coupon bond.
void rm_bond_flows(const BondSpec& b, std::vector<double>* t, std::vector<double>* cf, double* prev) {
    const int f = std::clamp(b.freq, 1, 12);
    *t = rm_schedule(0.0, b.maturity, f);
    cf->assign(t->size(), b.face * b.coupon_pct / 100.0 / f);
    if (!cf->empty())
        cf->back() += b.face;
    *prev = t->empty() ? 0.0 : t->front() - 1.0 / f;
}

double rm_bond_dirty(const ZeroCurve& c, const std::vector<double>& t, const std::vector<double>& cf, double z) {
    double p = 0;
    for (size_t i = 0; i < t.size(); ++i)
        p += cf[i] * c.df(t[i]) * std::exp(-z * t[i]);
    return p;
}

} // namespace

SwaptionResult swaption_on_curve(const ZeroCurve& c, double expiry, double tenor, double strike, double vol, int freq,
                                 double notional, bool payer, bool normal_vol) {
    SwaptionResult out = rm_swaption(c, expiry, tenor, strike, vol, freq, notional, payer, normal_vol);
    if (!out.ok || c.empty())
        return out;
    const SwaptionResult up = rm_swaption(shifted(c, 1.0), expiry, tenor, strike, vol, freq, notional, payer, normal_vol);
    const SwaptionResult dn = rm_swaption(shifted(c, -1.0), expiry, tenor, strike, vol, freq, notional, payer, normal_vol);
    if (up.ok && dn.ok)
        out.dv01 = 0.5 * (up.premium - dn.premium);
    return out;
}

BondCurveResult bond_on_curve(const ZeroCurve& c, const BondSpec& b, double z) {
    BondCurveResult out;
    if (!(b.maturity > 0) || c.empty() || !(b.face > 0))
        return out;
    std::vector<double> t, cf;
    double prev = 0;
    rm_bond_flows(b, &t, &cf, &prev);
    if (t.empty())
        return out;
    out.payments = static_cast<int>(t.size());
    out.dirty = rm_bond_dirty(c, t, cf, z);
    out.accrued = b.face * b.coupon_pct / 100.0 * std::max(0.0, -prev);
    out.clean = out.dirty - out.accrued;
    double d1 = 0, d2 = 0, last = 0;
    for (size_t i = 0; i < t.size(); ++i) {
        const double v = cf[i] * c.df(t[i]) * std::exp(-z * t[i]);
        d1 += t[i] * v;
        d2 += t[i] * t[i] * v;
        out.annuity += (t[i] - last) * c.df(t[i]);
        last = t[i];
    }
    if (out.dirty > 0) {
        out.duration = d1 / out.dirty;
        out.convexity = d2 / out.dirty;
    }
    out.ok = std::isfinite(out.dirty) && out.dirty > 0;
    return out;
}

SpreadSolve z_spread(const ZeroCurve& c, const BondSpec& b, double dirty_price) {
    SpreadSolve out;
    if (!(dirty_price > 0)) {
        out.message = "Enter a positive market price.";
        return out;
    }
    std::vector<double> t, cf;
    double prev = 0;
    rm_bond_flows(b, &t, &cf, &prev);
    if (t.empty() || c.empty()) {
        out.message = "The bond has no cash flows left or no curve is loaded.";
        return out;
    }
    // f(z) = price(z) − target is decreasing in z. Bracket, then bisection with a secant step.
    auto f = [&](double z) { return rm_bond_dirty(c, t, cf, z) - dirty_price; };
    double lo = -0.05, hi = 0.20;
    double flo = f(lo), fhi = f(hi);
    for (int i = 0; i < 40 && flo < 0; ++i) {
        lo = lo * 2.0 - 0.01;
        if (lo < -0.9)
            break;
        flo = f(lo);
    }
    for (int i = 0; i < 40 && fhi > 0; ++i) {
        hi = hi * 2.0 + 0.01;
        if (hi > 20.0)
            break;
        fhi = f(hi);
    }
    if (!(flo >= 0 && fhi <= 0)) {
        out.message = "No spread over the curve reprices this price: check the price, the coupon and the maturity.";
        return out;
    }
    for (int it = 0; it < 200; ++it) {
        out.iterations = it + 1;
        double z = (std::fabs(flo - fhi) > 1e-300) ? lo + flo * (hi - lo) / (flo - fhi) : 0.5 * (lo + hi);
        if (!(z > lo && z < hi) || it % 3 == 2)
            z = 0.5 * (lo + hi); // keep bisection's guarantee
        const double fz = f(z);
        if (std::fabs(fz) < 1e-11 || (hi - lo) < 1e-13) {
            out.spread = z;
            out.ok = true;
            return out;
        }
        if (fz > 0) {
            lo = z;
            flo = fz;
        } else {
            hi = z;
            fhi = fz;
        }
    }
    out.spread = 0.5 * (lo + hi);
    out.ok = true;
    return out;
}

double asset_swap_spread(const ZeroCurve& c, const BondSpec& b, double market_dirty) {
    const BondCurveResult r = bond_on_curve(c, b, 0.0);
    if (!r.ok || !(r.annuity > 0) || !(b.face > 0))
        return 0.0;
    return (r.dirty - market_dirty) / (b.face * r.annuity);
}

CarryResult futures_carry(const CarryInputs& in) {
    CarryResult out;
    if (!(in.spot > 0) || !(in.years > 0)) {
        out.message = "Spot and the time to delivery must be positive.";
        return out;
    }
    for (const CashFlowAt& f : in.income)
        if (f.time > 0 && f.time <= in.years + 1e-12)
            out.pv_income += f.amount * std::exp(-in.rate * f.time);
    const double base = in.spot - out.pv_income;
    if (!(base > 0)) {
        out.message = "The income is worth more than the spot price: check the amounts.";
        return out;
    }
    const double T = in.years;
    out.fair = base * std::exp((in.rate + in.carry_cost - in.income_yield) * T);
    out.ok = std::isfinite(out.fair);
    if (in.market_future > 0) {
        out.has_market = true;
        out.basis = in.market_future - out.fair;
        out.spot_basis = in.market_future - in.spot;
        const double g = std::log(in.market_future / base) / T;
        out.implied_repo = g - in.carry_cost + in.income_yield;
        out.implied_repo_simple = (in.market_future * std::exp((in.income_yield - in.carry_cost) * T) / base - 1.0) / T;
        out.implied_yield = in.rate + in.carry_cost - g;
    }
    return out;
}

} // namespace fincept::services::options::rates
