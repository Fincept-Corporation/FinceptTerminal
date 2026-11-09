#include "services/options/DerivativesPricing.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace fincept::services::options::derivatives {

namespace {

constexpr double kDerivBrentRtol = 4.0 * std::numeric_limits<double>::epsilon(); // scipy's brentq default
constexpr int kDerivMaxScheduleSteps = 4000; // ~330 years of monthly dates: a corrupt date, not an instrument
constexpr double kDerivDaysPerYear = 365.25;

const char* const kDerivFreqError = "Payment frequency must be one of 1, 2, 3, 4, 6 or 12 per year";

bool deriv_valid_freq(int freq) {
    return freq == 1 || freq == 2 || freq == 3 || freq == 4 || freq == 6 || freq == 12;
}

/// `d` moved by `months` calendar months, the day clamped to the target month's length
/// (31-Aug + 6 months = 28/29-Feb). Years outside 1..9999 are rejected.
Result<QDate> deriv_add_months(const QDate& d, int months) {
    const long long idx = static_cast<long long>(d.year()) * 12 + (d.month() - 1) + months;
    long long year = idx / 12;
    long long month0 = idx % 12;
    if (month0 < 0) { // floor division
        month0 += 12;
        --year;
    }
    if (year < 1 || year > 9999)
        return Result<QDate>::err("year " + std::to_string(year) + " is out of range");
    const int y = static_cast<int>(year);
    const int m = static_cast<int>(month0) + 1;
    return Result<QDate>::ok(QDate(y, m, std::min(d.day(), QDate(y, m, 1).daysInMonth())));
}

struct DerivBrent {
    double root = 0;
    bool ok = false;
};

/// scipy.optimize.brentq (scipy/optimize/Zeros/brentq.c), step for step, with its default
/// rtol: same sign test, same interpolation/extrapolation and bisection rules, same
/// termination. A NaN function value or a bracket without a sign change fails, as do
/// `maxiter` iterations without convergence.
template <typename F>
DerivBrent deriv_brentq(F&& f, double xa, double xb, double xtol, int maxiter) {
    DerivBrent out;
    double xpre = xa, xcur = xb;
    double xblk = 0., fblk = 0., spre = 0., scur = 0.;
    double fpre = f(xpre);
    double fcur = f(xcur);
    if (std::isnan(fpre) || std::isnan(fcur))
        return out;
    if (fpre == 0) {
        out.root = xpre;
        out.ok = true;
        return out;
    }
    if (fcur == 0) {
        out.root = xcur;
        out.ok = true;
        return out;
    }
    if (std::signbit(fpre) == std::signbit(fcur))
        return out;
    for (int i = 0; i < maxiter; ++i) {
        if (fpre != 0 && fcur != 0 && (std::signbit(fpre) != std::signbit(fcur))) {
            xblk = xpre;
            fblk = fpre;
            spre = scur = xcur - xpre;
        }
        if (std::fabs(fblk) < std::fabs(fcur)) {
            xpre = xcur;
            xcur = xblk;
            xblk = xpre;
            fpre = fcur;
            fcur = fblk;
            fblk = fpre;
        }
        const double delta = (xtol + kDerivBrentRtol * std::fabs(xcur)) / 2;
        const double sbis = (xblk - xcur) / 2;
        if (fcur == 0 || std::fabs(sbis) < delta) {
            out.root = xcur;
            out.ok = true;
            return out;
        }
        if (std::fabs(spre) > delta && std::fabs(fcur) < std::fabs(fpre)) {
            double stry;
            if (xpre == xblk) {
                stry = -fcur * (xcur - xpre) / (fcur - fpre); // interpolate
            } else {
                const double dpre = (fpre - fcur) / (xpre - xcur); // extrapolate
                const double dblk = (fblk - fcur) / (xblk - xcur);
                stry = -fcur * (fblk * dblk - fpre * dpre) / (dblk * dpre * (fblk - fpre));
            }
            if (2 * std::fabs(stry) < std::min(std::fabs(spre), 3 * std::fabs(sbis) - delta)) {
                spre = scur; // good short step
                scur = stry;
            } else {
                spre = sbis; // bisect
                scur = sbis;
            }
        } else {
            spre = sbis; // bisect
            scur = sbis;
        }
        xpre = xcur;
        fpre = fcur;
        if (std::fabs(scur) > delta)
            xcur += scur;
        else
            xcur += (sbis > 0 ? delta : -delta);
        fcur = f(xcur);
        if (std::isnan(fcur))
            return out;
    }
    return out; // did not converge
}

// ── Bonds ────────────────────────────────────────────────────────────────────

struct DerivCouponSchedule {
    QDate prev;               // last coupon date on or before settlement
    std::vector<QDate> after; // coupon dates strictly after settlement, ascending
    qint64 period_days = 0;   // length of the current coupon period
    double w = 0;             // fraction of the current period still to run
};

/// Coupon dates built BACKWARD from maturity (the market convention). Validates in the
/// order the screen has always reported errors: frequency, yield floor, dates, schedule.
Result<DerivCouponSchedule> deriv_coupon_schedule(const QDate& settle, const QDate& maturity, double ytm_pct,
                                                  int freq) {
    if (!deriv_valid_freq(freq))
        return Result<DerivCouponSchedule>::err(kDerivFreqError);
    if (1.0 + (ytm_pct / 100.0) / freq <= 0)
        return Result<DerivCouponSchedule>::err("Yield is too low to discount cash flows (YTM <= -100% per year)");
    if (settle >= maturity)
        return Result<DerivCouponSchedule>::err("Settlement must be before maturity");

    DerivCouponSchedule s;
    const int step = 12 / freq;
    for (int k = 0;;) {
        Result<QDate> d = deriv_add_months(maturity, -k * step);
        if (d.is_err())
            return Result<DerivCouponSchedule>::err(d.error());
        if (d.value() <= settle) {
            s.prev = d.value();
            break;
        }
        s.after.push_back(d.value());
        ++k;
        if (k > kDerivMaxScheduleSteps)
            return Result<DerivCouponSchedule>::err("Coupon schedule too long - check the dates");
    }
    std::reverse(s.after.begin(), s.after.end());
    s.period_days = s.prev.daysTo(s.after.front());
    if (s.period_days <= 0)
        return Result<DerivCouponSchedule>::err("Invalid coupon schedule");
    s.w = double(settle.daysTo(s.after.front())) / double(s.period_days);
    return Result<DerivCouponSchedule>::ok(std::move(s));
}

/// Cash flows are discounted at (w + k) coupon periods, k = 0..n−1 indexing the remaining
/// coupons, at the per-period yield (YTM / freq).
BondPrice deriv_bond_value(const DerivCouponSchedule& s, const QDate& issue, const QDate& settle,
                           double coupon_rate_pct, double ytm_pct, int freq) {
    const double coupon_dec = coupon_rate_pct / 100.0;
    const double y_per = (ytm_pct / 100.0) / freq;
    const int n = int(s.after.size());
    const double coupon = coupon_dec / freq * 100.0; // per 100 face, per period
    double pv = 0.0;
    double dur_num = 0.0;
    double conv_num = 0.0;
    for (int k = 0; k < n; ++k) {
        const double periods = s.w + k;
        const double t = periods / freq; // years from settlement
        const double cf = coupon + (k == n - 1 ? 100.0 : 0.0);
        const double df = std::pow(1.0 + y_per, -periods);
        pv += cf * df;
        dur_num += t * cf * df;
        conv_num += t * (t + 1.0 / freq) * cf * df;
    }
    // Accrued runs from the last coupon (or the issue date, for a first short coupon) to
    // settlement, actual/actual within the period.
    const QDate accrual_start = std::max(s.prev, issue);
    const double accrued = coupon * (double(std::max<qint64>(accrual_start.daysTo(settle), 0)) / double(s.period_days));

    BondPrice b;
    b.dirty_price = pv;
    b.accrued_interest = accrued;
    b.clean_price = pv - accrued;
    b.macaulay_duration = dur_num / pv;
    b.modified_duration = b.macaulay_duration / (1.0 + y_per);
    b.convexity = conv_num / (pv * std::pow(1.0 + y_per, 2.0));
    b.remaining_coupons = n;
    b.next_coupon_date = s.after.front();
    return b;
}

} // namespace

// ── Rounding ─────────────────────────────────────────────────────────────────

double round_half_even(double x, int ndigits) {
    static constexpr double kPow10[] = {1e0, 1e1, 1e2,  1e3,  1e4,  1e5,  1e6,  1e7,
                                        1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15};
    if (!std::isfinite(x) || x == 0 || ndigits < 0 || ndigits > 15)
        return x;
    const double ax = std::fabs(x);
    const double p10 = kPow10[ndigits];
    const double p = ax * p10;
    if (p >= 9007199254740992.0) // 2^53: doubles are coarser than the decimal grid — x is its own nearest
        return x;
    // ax·10^n = p + e exactly (an FMA residual is exact); round that exact value to an
    // integer k, ties to even, then take the double nearest k·10^−n.
    const double e = std::fma(ax, p10, -p);
    double k = std::floor(p);
    if (p >= 4503599627370496.0) {
        // 2^52 ≤ p < 2^53: p is an integer and |e| ≤ ½, so only an exact half moves it.
        if (std::fmod(k, 2.0) != 0 && (e == 0.5 || e == -0.5))
            k += e > 0 ? 1.0 : -1.0;
    } else {
        // p − k and (p − k) − ½ are exact here, so r carries the exact sign (zero on a tie).
        const double r = (p - k - 0.5) + e;
        if (r > 0 || (r == 0 && std::fmod(k, 2.0) != 0))
            k += 1.0;
    }
    const double out = k / p10;
    return std::signbit(x) ? -out : out;
}

// ── Bonds ────────────────────────────────────────────────────────────────────

Result<BondPrice> bond_price_from_ytm(const QDate& issue, const QDate& settlement, const QDate& maturity,
                                      double coupon_rate_pct, double ytm_pct, int freq) {
    Result<DerivCouponSchedule> s = deriv_coupon_schedule(settlement, maturity, ytm_pct, freq);
    if (s.is_err())
        return Result<BondPrice>::err(s.error());
    const BondPrice b = deriv_bond_value(s.value(), issue, settlement, coupon_rate_pct, ytm_pct, freq);
    // Every discount factor underflowed (a zero-coupon bond centuries out at a high yield):
    // the price is 0 and duration / convexity are 0/0.
    if (b.dirty_price == 0)
        return Result<BondPrice>::err("Bond price underflows to zero at this yield - check the maturity date");
    return Result<BondPrice>::ok(b);
}

Result<double> bond_ytm_from_clean_price(const QDate& issue, const QDate& settlement, const QDate& maturity,
                                         double coupon_rate_pct, double clean_price, int freq) {
    // Bad dates / frequency are reported as such, not as "could not converge".
    Result<DerivCouponSchedule> s = deriv_coupon_schedule(settlement, maturity, 5.0, freq);
    if (s.is_err())
        return Result<double>::err(s.error());
    const DerivCouponSchedule& sched = s.value();
    // A trial yield at which the whole price underflows ends the search: an underflowed
    // price of exactly 0 would otherwise pass for a root.
    bool underflow = false;
    const DerivBrent ytm = deriv_brentq(
        [&](double y) {
            const BondPrice b = deriv_bond_value(sched, issue, settlement, coupon_rate_pct, y, freq);
            if (b.dirty_price == 0) {
                underflow = true;
                return std::numeric_limits<double>::quiet_NaN();
            }
            return b.clean_price - clean_price;
        },
        -5.0, 100.0, 1e-10, 200);
    if (underflow)
        return Result<double>::err("Bond price underflows to zero within the -5% to 100% yield search - check the "
                                   "maturity date");
    if (!ytm.ok)
        return Result<double>::err("Could not converge on YTM - the clean price is outside what any yield between -5% "
                                   "and 100% can produce for this bond");
    return Result<double>::ok(ytm.root);
}

// ── Equity options ───────────────────────────────────────────────────────────

Result<OptionValue> equity_option(double spot, double strike, double years, double rate_pct, double vol_pct,
                                  double div_yield_pct, bool is_call) {
    const double r = rate_pct / 100;
    const double sigma = vol_pct / 100;
    const double q = div_yield_pct / 100;
    if (spot <= 0 || strike <= 0)
        return Result<OptionValue>::err("Spot and strike must be positive");
    if (years <= 0)
        return Result<OptionValue>::err("Time to expiry must be positive");
    if (sigma <= 0)
        return Result<OptionValue>::err("Volatility must be positive");
    OptionValue v;
    v.price = is_call ? pricing::bsm_call(spot, strike, years, r, sigma, q)
                      : pricing::bsm_put(spot, strike, years, r, sigma, q);
    v.greeks = pricing::bsm_greeks(is_call, spot, strike, years, r, sigma, q);
    return Result<OptionValue>::ok(v);
}

Result<double> equity_implied_vol(double spot, double strike, double years, double rate_pct, double market_price,
                                  double div_yield_pct, bool is_call) {
    const double r = rate_pct / 100;
    const double q = div_yield_pct / 100;
    if (spot <= 0 || strike <= 0)
        return Result<double>::err("Spot and strike must be positive");
    if (years <= 0)
        return Result<double>::err("Time to expiry must be positive");
    if (market_price <= 0)
        return Result<double>::err("Market price must be positive");

    // No-arbitrage bounds on the discounted price.
    const double fwd_s = spot * std::exp(-q * years);
    const double disc_k = strike * std::exp(-r * years);
    const double lower = is_call ? std::max(fwd_s - disc_k, 0.0) : std::max(disc_k - fwd_s, 0.0);
    const double upper = is_call ? fwd_s : disc_k;
    const auto fmt4 = [](double v) { return QString::number(round_half_even(v, 4), 'f', 4).toStdString(); };
    if (market_price < lower - 1e-9)
        return Result<double>::err("Market price " + fmt4(market_price) +
                                   " is below the option's no-arbitrage minimum (intrinsic value " + fmt4(lower) +
                                   ") - no implied volatility exists");
    if (market_price > upper + 1e-9)
        return Result<double>::err("Market price " + fmt4(market_price) +
                                   " is above the option's no-arbitrage maximum (" + fmt4(upper) +
                                   ") - no implied volatility exists");

    const DerivBrent iv = deriv_brentq(
        [&](double sigma) {
            return (is_call ? pricing::bsm_call(spot, strike, years, r, sigma, q)
                            : pricing::bsm_put(spot, strike, years, r, sigma, q)) -
                   market_price;
        },
        0.001, 10.0, 1e-8, 200);
    if (!iv.ok || iv.root < 0)
        return Result<double>::err("Could not converge on implied volatility - it lies outside the solver's 0.1% to "
                                   "1000% range");
    return Result<double>::ok(iv.root);
}

// ── FX options ───────────────────────────────────────────────────────────────

Result<FxOptionValue> fx_option(double spot, double strike, double years, double domestic_rate_pct,
                                double foreign_rate_pct, double vol_pct, bool is_call, double notional) {
    const double r_d = domestic_rate_pct / 100;
    const double r_f = foreign_rate_pct / 100;
    const double sigma = vol_pct / 100;
    if (spot <= 0 || strike <= 0)
        return Result<FxOptionValue>::err("Spot and strike must be positive");
    if (years <= 0)
        return Result<FxOptionValue>::err("Time to expiry must be positive");
    if (sigma <= 0)
        return Result<FxOptionValue>::err("Volatility must be positive");
    if (notional <= 0)
        return Result<FxOptionValue>::err("Notional must be positive");
    // Garman-Kohlhagen = BSM with the foreign rate in the dividend-yield slot.
    FxOptionValue v;
    v.premium = (is_call ? pricing::bsm_call(spot, strike, years, r_d, sigma, r_f)
                         : pricing::bsm_put(spot, strike, years, r_d, sigma, r_f)) *
                notional;
    v.premium_per_unit = v.premium / notional;
    v.premium_pct_foreign = v.premium_per_unit / spot * 100.0;
    v.greeks = pricing::bsm_greeks(is_call, spot, strike, years, r_d, sigma, r_f);
    return Result<FxOptionValue>::ok(v);
}

// ── Interest-rate swaps ──────────────────────────────────────────────────────

Result<SwapValue> swap_value(const QDate& effective, const QDate& maturity, double fixed_rate_pct, int freq,
                             double notional, double discount_rate_pct) {
    const double fixed_rate_dec = fixed_rate_pct / 100.0;
    const double disc_rate_dec = discount_rate_pct / 100.0;
    if (!deriv_valid_freq(freq))
        return Result<SwapValue>::err(kDerivFreqError);
    const double T = double(effective.daysTo(maturity)) / kDerivDaysPerYear;
    if (T <= 0)
        return Result<SwapValue>::err("Maturity must be after effective date");

    // Payment dates roll forward from the effective date; the last one is always maturity
    // (a short final period is kept, not dropped).
    const int step = 12 / freq;
    std::vector<QDate> dates;
    for (int k = 1;;) {
        Result<QDate> d = deriv_add_months(effective, k * step);
        if (d.is_err())
            return Result<SwapValue>::err(d.error());
        if (d.value() >= maturity)
            break;
        dates.push_back(d.value());
        ++k;
        if (k > kDerivMaxScheduleSteps)
            return Result<SwapValue>::err("Payment schedule too long - check the dates");
    }
    dates.push_back(maturity);
    // A negative rate over centuries: e^(−r·t) overflows and every PV with it.
    if (!std::isfinite(std::exp(-disc_rate_dec * T)))
        return Result<SwapValue>::err("Discount factors overflow at this rate and tenor - check the dates");

    double fixed_pv = 0.0;
    double annuity = 0.0;
    QDate prev = effective;
    for (const QDate& d : dates) {
        const double dt = double(prev.daysTo(d)) / kDerivDaysPerYear;
        const double t_i = double(effective.daysTo(d)) / kDerivDaysPerYear;
        const double df = std::exp(-disc_rate_dec * t_i);
        fixed_pv += fixed_rate_dec * dt * notional * df;
        annuity += dt * df;
        prev = d;
    }
    // Floating leg at par on the effective date: notional·(1 − DF(T)).
    const double float_pv = notional * (1 - std::exp(-disc_rate_dec * T));

    SwapValue v;
    v.value = float_pv - fixed_pv;
    v.fixed_leg_pv = fixed_pv;
    v.floating_leg_pv = float_pv;
    v.par_rate_pct = (annuity > 0 ? (1 - std::exp(-disc_rate_dec * T)) / annuity : 0) * 100;
    v.tenor_years = T;
    v.payments = int(dates.size());
    if (!std::isfinite(v.value) || !std::isfinite(v.fixed_leg_pv) || !std::isfinite(v.par_rate_pct))
        return Result<SwapValue>::err("Discount factors overflow at this rate and tenor - check the dates");
    return Result<SwapValue>::ok(v);
}

// ── Credit default swaps ─────────────────────────────────────────────────────

Result<CdsValue> cds_value(const QDate& valuation, const QDate& maturity, double recovery_pct, double notional,
                           double spread_bps, double risk_free_pct, double coupon_bps) {
    const double rec_rate = recovery_pct / 100.0;
    const double spread = spread_bps / 10000.0;
    const double coupon = coupon_bps / 10000.0;
    const double T = double(valuation.daysTo(maturity)) / kDerivDaysPerYear;
    if (T <= 0)
        return Result<CdsValue>::err("Maturity must be after valuation date");
    const double lgd = 1 - rec_rate;
    if (lgd <= 0)
        return Result<CdsValue>::err("LGD must be positive (recovery < 100%)");
    if (spread < 0 || coupon < 0)
        return Result<CdsValue>::err("Spread and coupon must not be negative");
    const double r = risk_free_pct / 100.0;

    // Quarterly premium dates strictly after valuation, built backward from maturity.
    std::vector<QDate> pay_dates;
    for (int k = 0;;) {
        Result<QDate> d = deriv_add_months(maturity, -3 * k);
        if (d.is_err())
            return Result<CdsValue>::err(d.error());
        if (d.value() <= valuation)
            break;
        pay_dates.push_back(d.value());
        ++k;
        if (k > kDerivMaxScheduleSteps)
            return Result<CdsValue>::err("Premium schedule too long - check the dates");
    }
    std::reverse(pay_dates.begin(), pay_dates.end());
    struct Period {
        double t_prev;
        double t_i;
    };
    std::vector<Period> periods;
    periods.reserve(pay_dates.size());
    QDate prev = valuation;
    for (const QDate& d : pay_dates) {
        periods.push_back(
            {double(valuation.daysTo(prev)) / kDerivDaysPerYear, double(valuation.daysTo(d)) / kDerivDaysPerYear});
        prev = d;
    }

    // PV of 1 per year of premium: scheduled payments on survival + premium accrued to the
    // default time (paid at default, mid-period).
    const auto risky_annuity = [&](double h) {
        double a = 0.0;
        for (const Period& p : periods) {
            const double accrual = p.t_i - p.t_prev;
            const double s_prev = std::exp(-h * p.t_prev);
            const double s_i = std::exp(-h * p.t_i);
            a += accrual * s_i * std::exp(-r * p.t_i);
            a += 0.5 * accrual * (s_prev - s_i) * std::exp(-r * 0.5 * (p.t_prev + p.t_i));
        }
        return a;
    };
    // LGD-weighted PV of 1 of protection, closed form for flat h and r.
    const auto protection_per_unit = [&](double h) {
        const double hr = h + r;
        if (std::fabs(hr) < 1e-12)
            return lgd * h * T;
        return lgd * h / hr * (1.0 - std::exp(-hr * T));
    };

    // Calibrate the flat hazard rate to the market (par) spread, seeded by the credit
    // triangle h = s / (1 − R).
    double hazard_rate = spread / lgd;
    if (spread > 0) {
        const double hi = std::max(1.0, 10.0 * hazard_rate);
        const DerivBrent h = deriv_brentq([&](double x) { return protection_per_unit(x) - spread * risky_annuity(x); },
                                          1e-12, hi, 1e-14, 200);
        if (!h.ok)
            return Result<CdsValue>::err("Could not calibrate a hazard rate to this spread and recovery");
        hazard_rate = h.root;
    }

    const double annuity = risky_annuity(hazard_rate);
    // Every figure below is per unit of notional at heart; a zero notional leaves the par
    // spread as 0/0.
    if (notional <= 0)
        return Result<CdsValue>::err("Notional must be positive");
    CdsValue v;
    v.premium_leg_pv = coupon * notional * annuity;
    v.protection_leg_pv = protection_per_unit(hazard_rate) * notional;
    v.upfront = v.protection_leg_pv - v.premium_leg_pv;
    v.upfront_points = v.upfront / notional * 100.0;
    v.breakeven_spread_bps = annuity > 0 ? v.protection_leg_pv / (notional * annuity) * 10000 : 0;
    v.risky_pv01 = annuity * notional * 1e-4;
    v.hazard_rate_pct = hazard_rate * 100;
    v.survival_pct = std::exp(-hazard_rate * T) * 100;
    v.tenor_years = T;
    v.premium_payments = int(pay_dates.size());
    return Result<CdsValue>::ok(v);
}

} // namespace fincept::services::options::derivatives
