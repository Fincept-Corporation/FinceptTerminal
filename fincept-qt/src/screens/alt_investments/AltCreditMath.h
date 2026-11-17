#pragma once
// AltCreditMath.h — private credit analytics for one loan (A10), Qt Core only so every formula is
// unit-tested (tests/tst_alt_credit_math.cpp) against textbook values.
//
// The loan: face amount bought at `price_pct` of par (less an upfront fee / OID received), fixed or
// floating coupon (floating = max(base, floor) + spread, the base held flat at today's level — no
// forward curve is assumed), paid `frequency` times a year, straight-line amortisation as a % of the
// original face per year, the rest repaid at maturity (or at `call_years` for the takeout yield).
//
// Results: the cash-flow schedule; yield (bond-equivalent: periodic IRR x frequency, plus the
// effective annual rate); spread over the base / a reference yield; Macaulay and modified duration
// at that yield (and, for a floating loan, the rate duration to the next reset); expected loss
// = PD x LGD a year, cumulative default probability 1 - (1 - PD)^T, and the loss-adjusted yield
// (yield - expected loss, the usual first-order adjustment); a rate-shock table (fixed: repriced at
// the shocked yield; floating: the change in a year's interest income).

#include <QString>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens::alt_credit {

struct LoanInputs {
    double face = 10'000'000;
    double price_pct = 98.0;       ///< % of par paid
    double upfront_fee_pct = 1.0;  ///< % of face received at close (OID / arrangement fee)
    bool floating = true;
    double coupon_pct = 9.0;       ///< fixed coupon, % a year
    double base_pct = 4.3;         ///< floating reference rate today, % a year
    double spread_bps = 550;       ///< floating spread
    double floor_pct = 1.0;        ///< floating floor on the base
    double maturity_years = 5;
    int frequency = 4;             ///< payments a year
    double amort_pct = 1.0;        ///< % of original face repaid a year (straight line)
    double call_years = 0;         ///< > 0: also the yield if repaid at par then (takeout convention)
    double pd_pct = 2.0;           ///< annual probability of default
    double lgd_pct = 40.0;         ///< loss given default
    double reference_yield_pct = std::numeric_limits<double>::quiet_NaN(); ///< for a fixed loan's spread
};

struct Period {
    int index = 0;           ///< 1..n
    double years = 0;        ///< time of the payment
    double interest = 0;
    double principal = 0;
    double balance_after = 0;
};

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// Annual coupon rate (%) actually paid.
inline double coupon_rate_pct(const LoanInputs& in) {
    if (!in.floating)
        return in.coupon_pct;
    return std::max(in.base_pct, in.floor_pct) + in.spread_bps / 100.0;
}

/// Payment schedule until `years` (the maturity, or a call date when the loan is repaid early).
inline QVector<Period> schedule(const LoanInputs& in, double years, double coupon_pct) {
    QVector<Period> out;
    const int f = std::max(1, in.frequency);
    const int n = std::max(1, int(std::lround(years * f)));
    const double per_period_amort = in.face * std::max(0.0, in.amort_pct) / 100.0 / f;
    double bal = in.face;
    for (int i = 1; i <= n; ++i) {
        Period p;
        p.index = i;
        p.years = double(i) / f;
        p.interest = bal * coupon_pct / 100.0 / f;
        p.principal = (i == n) ? bal : std::min(bal, per_period_amort);
        bal -= p.principal;
        p.balance_after = bal;
        out << p;
    }
    return out;
}

/// Present value of `sched` at periodic rate `r`.
inline double pv(const QVector<Period>& sched, double r) {
    double v = 0;
    for (const Period& p : sched)
        v += (p.interest + p.principal) / std::pow(1.0 + r, p.index);
    return v;
}

/// Periodic IRR: the rate at which the schedule's PV equals `cost`, by bisection on [-0.99, 10]
/// (PV falls as the rate rises). NaN when no rate in the range fits.
inline double periodic_irr(const QVector<Period>& sched, double cost) {
    if (sched.isEmpty() || !(cost > 0))
        return kNaN;
    double lo = -0.99, hi = 10.0;
    double flo = pv(sched, lo) - cost, fhi = pv(sched, hi) - cost;
    if (flo * fhi > 0)
        return kNaN;
    for (int i = 0; i < 300; ++i) {
        const double mid = 0.5 * (lo + hi);
        const double fm = pv(sched, mid) - cost;
        if (std::fabs(fm) < 1e-10 * std::max(1.0, cost) || (hi - lo) < 1e-14)
            return mid;
        if ((fm > 0) == (flo > 0)) {
            lo = mid;
            flo = fm;
        } else {
            hi = mid;
        }
    }
    return 0.5 * (lo + hi);
}

/// Macaulay duration in years at periodic yield `r`.
inline double macaulay_years(const QVector<Period>& sched, double r) {
    const double price = pv(sched, r);
    if (!(price > 0))
        return kNaN;
    double t = 0;
    for (const Period& p : sched)
        t += p.years * (p.interest + p.principal) / std::pow(1.0 + r, p.index);
    return t / price;
}

struct RateShock {
    int bps = 0;
    double value_change = kNaN;      ///< fixed: price change (currency) at the shocked yield
    double value_change_pct = kNaN;  ///< fixed: % of the current value
    double income_change = kNaN;     ///< floating: change in one year's interest income (currency)
};

struct LoanResult {
    QVector<Period> periods;
    double coupon_pct = kNaN;
    double cost = kNaN;              ///< cash paid: price - upfront fee
    double yield_pct = kNaN;         ///< bond-equivalent (periodic x frequency)
    double effective_yield_pct = kNaN;
    double yield_to_call_pct = kNaN; ///< repaid at par at call_years (when set and before maturity)
    double spread_bps = kNaN;        ///< over the base (floating) or the reference yield (fixed)
    double macaulay_years = kNaN;
    double modified_duration = kNaN;
    double rate_duration = kNaN;     ///< floating: years to the next reset; fixed: = modified duration
    double expected_loss_pct = kNaN; ///< PD x LGD, a year
    double cumulative_pd_pct = kNaN; ///< over the life
    double loss_adjusted_yield_pct = kNaN;
    double weighted_average_life = kNaN;
    QVector<RateShock> shocks;
    QString problem;                 ///< why nothing was computed
};

inline LoanResult analyse(const LoanInputs& in) {
    LoanResult r;
    if (!(in.face > 0) || !(in.price_pct > 0) || !(in.maturity_years > 0) || in.frequency < 1) {
        r.problem = QStringLiteral("Enter a positive face amount, price and maturity.");
        return r;
    }
    const int f = in.frequency;
    r.coupon_pct = coupon_rate_pct(in);
    r.periods = schedule(in, in.maturity_years, r.coupon_pct);
    r.cost = in.face * (in.price_pct - in.upfront_fee_pct) / 100.0;
    const double y = periodic_irr(r.periods, r.cost);
    if (!std::isfinite(y)) {
        r.problem = QStringLiteral("No yield fits these cash flows and price.");
        return r;
    }
    r.yield_pct = y * f * 100.0;
    r.effective_yield_pct = (std::pow(1.0 + y, f) - 1.0) * 100.0;
    if (in.call_years > 0 && in.call_years < in.maturity_years) {
        const double yc = periodic_irr(schedule(in, in.call_years, r.coupon_pct), r.cost);
        if (std::isfinite(yc))
            r.yield_to_call_pct = yc * f * 100.0;
    }
    if (in.floating)
        r.spread_bps = (r.yield_pct - std::max(in.base_pct, in.floor_pct)) * 100.0;
    else if (std::isfinite(in.reference_yield_pct))
        r.spread_bps = (r.yield_pct - in.reference_yield_pct) * 100.0;
    r.macaulay_years = macaulay_years(r.periods, y);
    r.modified_duration = r.macaulay_years / (1.0 + y);
    r.rate_duration = in.floating ? 1.0 / f : r.modified_duration;
    double wal = 0, total_principal = 0;
    for (const Period& p : r.periods) {
        wal += p.years * p.principal;
        total_principal += p.principal;
    }
    r.weighted_average_life = total_principal > 0 ? wal / total_principal : kNaN;
    const double pd = std::clamp(in.pd_pct, 0.0, 100.0) / 100.0;
    const double lgd = std::clamp(in.lgd_pct, 0.0, 100.0) / 100.0;
    r.expected_loss_pct = pd * lgd * 100.0;
    r.cumulative_pd_pct = (1.0 - std::pow(1.0 - pd, in.maturity_years)) * 100.0;
    r.loss_adjusted_yield_pct = r.yield_pct - r.expected_loss_pct;

    const double value_now = pv(r.periods, y);
    for (int bps : {-200, -100, 100, 200}) {
        RateShock s;
        s.bps = bps;
        if (in.floating) {
            // A year of interest on the current balance, the floor respected.
            const double base_new = std::max(in.base_pct + bps / 100.0, in.floor_pct);
            const double c_new = base_new + in.spread_bps / 100.0;
            s.income_change = in.face * (c_new - r.coupon_pct) / 100.0;
        } else {
            const double shocked = pv(r.periods, y + bps / 10000.0 / f);
            s.value_change = shocked - value_now;
            s.value_change_pct = value_now > 0 ? (shocked / value_now - 1.0) * 100.0 : kNaN;
        }
        r.shocks << s;
    }
    return r;
}

} // namespace fincept::screens::alt_credit
