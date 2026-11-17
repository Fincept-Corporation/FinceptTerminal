// Unit tests for src/services/options/CreditMath — the CDS maths of the Derivatives tab's CREDIT
// page: hazard-rate bootstrap, legs, risky annuity, upfront <-> spread, CS01 / recovery / JTD.
//
// Every expected number below was recomputed INDEPENDENTLY in Python before it was written here
// (scripts kept in the package scratch folder: credit_ref.py / credit_ref2.py / hull_check.py).
// The Python model is the same model as CreditMath (piecewise-constant hazards, premium in arrears
// on a schedule rolled back from maturity, accrual on default, protection (1 - R) ∫ DF dQ) but it is
// evaluated another way — numerical integration (scipy.integrate.quad) and scipy.optimize.brentq —
// while CreditMath uses closed forms per piece and its own Brent search.
//
// References:
//   [Hull] Hull, J. (2018) Options, Futures and Other Derivatives, 10th ed., ch. 25 "Credit derivatives",
//          CDS valuation example: default probability 2 % a year conditional on survival, R = 40 %,
//          r = 5 % (continuous), 5 years, annual premiums in arrears, defaults mid-year. Published:
//          expected payments 4.0704 s, accrual 0.0426 s, payoff 0.0511, s = 0.0124 (124 bp).
//          Python reproduces those four numbers exactly with Q(t) = 0.98^t (s = 0.012425).
//   [OK]   O'Kane, D. (2008) Modelling Single-name and Multi-name Credit Derivatives, Wiley, ch. 5-6:
//          risky PV01 with a continuously paid premium RPV01 = (1 - e^{-(r+λ)T}) / (r + λ); the
//          credit triangle S ≈ λ (1 - R); upfront = (S - C) x RPV01.

#include "services/options/CreditMath.h"
#include "services/options/RatesMath.h"

#include <QtTest>

#include <cmath>
#include <vector>

using namespace fincept::services::options;
namespace cr = fincept::services::options::credit;

namespace {

bool near(double a, double b, double tol) {
    return std::isfinite(a) && std::isfinite(b) && std::fabs(a - b) <= tol;
}

QByteArray msg(double got, double want) {
    return QStringLiteral("got %1, want %2").arg(got, 0, 'g', 15).arg(want, 0, 'g', 15).toUtf8();
}

const std::vector<double> kTenors = {0.5, 1, 2, 3, 4, 5, 7, 10};
const std::vector<double> kSpreadsBp = {60, 75, 90, 105, 120, 130, 140, 150};

std::vector<double> sample_spreads() {
    std::vector<double> s;
    for (double b : kSpreadsBp)
        s.push_back(b / 1e4);
    return s;
}

cr::Conventions conv(int freq, cr::DayCount dc) {
    cr::Conventions c;
    c.frequency = freq;
    c.day_count = dc;
    c.accrual_on_default = true;
    return c;
}

} // namespace

class TestDerivCredit : public QObject {
    Q_OBJECT

  private slots:
    void schedule_and_hazard_curve() {
        const auto s = cr::premium_schedule(5.0, 4);
        QCOMPARE(int(s.size()), 20);
        QVERIFY(near(s.front(), 0.25, 1e-12));
        QVERIFY(near(s.back(), 5.0, 1e-12));
        // A broken first period: the stub is at the front.
        const auto st = cr::premium_schedule(0.6, 4);
        QCOMPARE(int(st.size()), 3);
        QVERIFY(near(st[0], 0.1, 1e-12) && near(st[1], 0.35, 1e-12) && near(st[2], 0.6, 1e-12));
        QVERIFY(cr::premium_schedule(0.0, 4).empty());
        cr::HazardCurve h;
        h.t = {1, 3};
        h.h = {0.01, 0.03};
        QVERIFY(near(h.survival(0.0), 1.0, 0.0));
        QVERIFY(near(h.cumulative(2.0), 0.01 + 0.03, 1e-15));
        QVERIFY(near(h.cumulative(5.0), 0.01 + 0.06 + 0.06, 1e-15)); // flat beyond the last knot
        QVERIFY(near(h.hazard_at(0.5), 0.01, 0) && near(h.hazard_at(2.0), 0.03, 0) && near(h.hazard_at(9.0), 0.03, 0));
        QVERIFY(near(cr::accrual_factor(cr::DayCount::Act360), 365.0 / 360.0, 1e-15));
    }

    // [Hull] worked example.
    void hull_cds_example() {
        const cr::Conventions c = conv(1, cr::DayCount::Act365F);
        const rates::ZeroCurve z = cr::flat_curve(0.05);
        // Hull's 2 % a year is a probability conditional on survival: Q(t) = 0.98^t, i.e. λ = −ln 0.98.
        const double lambda = -std::log(0.98);
        const cr::Legs l = cr::cds_legs(cr::flat_hazard(lambda), z, 0.40, 5.0, c);
        // Python (exact integral, same model): protection 0.051113602296, rpv01 4.112543889345,
        // accrual on default 0.042096332670, par spread 124.2870682272 bp.
        QVERIFY2(near(l.protection, 0.051113602296, 1e-11), msg(l.protection, 0.051113602296));
        QVERIFY2(near(l.rpv01, 4.112543889345, 1e-10), msg(l.rpv01, 4.112543889345));
        QVERIFY2(near(l.accrual_on_default, 0.042096332670, 1e-11), msg(l.accrual_on_default, 0.04209633267));
        QVERIFY2(near(l.par_spread() * 1e4, 124.2870682272, 1e-6), msg(l.par_spread() * 1e4, 124.2870682272));
        // Hull's published 0.0124 (0.012425 unrounded) with defaults at mid-year: within a few hundredths of a bp.
        QVERIFY2(near(l.par_spread() * 1e4, 124.25, 0.1), msg(l.par_spread() * 1e4, 124.25));
        // Hull's components: payments 4.0704 s + accrual 0.0426 s = 4.1130 s; payoff 0.0511.
        QVERIFY(near(l.rpv01, 4.1130, 0.002));
        QVERIFY(near(l.protection, 0.0511, 0.0002));
        // The continuous 2 % hazard (Python: 123.0401192092 bp) and the credit triangle h (1 − R) = 120 bp.
        const cr::Legs c2 = cr::cds_legs(cr::flat_hazard(0.02), z, 0.40, 5.0, c);
        QVERIFY2(near(c2.par_spread() * 1e4, 123.0401192092, 1e-6), msg(c2.par_spread() * 1e4, 123.0401192092));
        QVERIFY(near(c2.par_spread() * 1e4, 120.0, 4.0));
    }

    // [OK] continuous-premium risky annuity and the credit triangle.
    void okane_risky_annuity() {
        const rates::ZeroCurve z = cr::flat_curve(0.05);
        const double cont = (1.0 - std::exp(-0.07 * 5.0)) / 0.07; // 4.218741575447
        const cr::Legs m = cr::cds_legs(cr::flat_hazard(0.02), z, 0.40, 5.0, conv(12, cr::DayCount::Act365F));
        // Python: monthly 4.209961075398 (ratio 0.99791869), par 120.25027785 bp.
        QVERIFY2(near(m.rpv01, 4.209961075398, 1e-10), msg(m.rpv01, 4.209961075398));
        QVERIFY(std::fabs(m.rpv01 / cont - 1.0) < 0.0025);
        QVERIFY2(near(m.par_spread() * 1e4, 120.25027785, 1e-6), msg(m.par_spread() * 1e4, 120.25027785));
        const cr::Legs q = cr::cds_legs(cr::flat_hazard(0.02), z, 0.40, 5.0, conv(4, cr::DayCount::Act365F));
        // Python: quarterly 4.192451344351, par 120.75250193 bp.
        QVERIFY2(near(q.rpv01, 4.192451344351, 1e-10), msg(q.rpv01, 4.192451344351));
        QVERIFY2(near(q.par_spread() * 1e4, 120.75250193, 1e-6), msg(q.par_spread() * 1e4, 120.75250193));
        // Without accrual on default the annuity is smaller.
        cr::Conventions no_aod = conv(4, cr::DayCount::Act365F);
        no_aod.accrual_on_default = false;
        QVERIFY(cr::cds_legs(cr::flat_hazard(0.02), z, 0.40, 5.0, no_aod).rpv01 < q.rpv01);
    }

    void bootstrap_sample_curve() {
        const cr::Conventions c = conv(4, cr::DayCount::Act360);
        const std::vector<double> spreads = sample_spreads();
        const cr::Bootstrap b = cr::bootstrap(kTenors, spreads, 0.40, cr::flat_curve(0.04), c);
        QVERIFY2(b.ok, b.message.c_str());
        // Python (quad + brentq): hazards per segment, survival and risky annuity at each tenor.
        const double h[] = {0.010088300244, 0.015199716841, 0.017799382023, 0.0231669428,
                            0.028758711765, 0.029791779067, 0.028869533717, 0.030783306923};
        const double q[] = {0.99496855024, 0.987435591094, 0.970015342533, 0.947801361121,
                            0.920932030494, 0.893900485111, 0.843749278732, 0.769318814321};
        const double a[] = {0.498151090047, 0.983364151529, 1.913326087722, 2.788732868088,
                            3.608284785597, 4.372988672598, 5.752462855764, 7.491290616719};
        for (int i = 0; i < 8; ++i) {
            QVERIFY2(near(b.curve.h[i], h[i], 1e-10), msg(b.curve.h[i], h[i]));
            QVERIFY2(near(b.curve.survival(kTenors[i]), q[i], 1e-10), msg(b.curve.survival(kTenors[i]), q[i]));
            QVERIFY2(near(b.rpv01[i], a[i], 1e-9), msg(b.rpv01[i], a[i]));
            // Every quote reprices to its par spread (< 1e-8 bp).
            QVERIFY2(std::fabs(b.repriced[i] - spreads[i]) * 1e4 < 1e-8, msg(b.repriced[i], spreads[i]));
            QVERIFY(b.curve.h[i] > 0);
            if (i > 0)
                QVERIFY(b.curve.survival(kTenors[i]) < b.curve.survival(kTenors[i - 1]));
        }
        // Survival strictly decreasing on a fine grid too.
        double prev = 1.0;
        for (int k = 1; k <= 240; ++k) {
            const double s = b.curve.survival(k / 20.0);
            QVERIFY(s < prev);
            prev = s;
        }
        // Order of the input does not matter.
        std::vector<double> rt(kTenors.rbegin(), kTenors.rend()), rs(spreads.rbegin(), spreads.rend());
        const cr::Bootstrap r = cr::bootstrap(rt, rs, 0.40, cr::flat_curve(0.04), c);
        QVERIFY(r.ok && near(r.curve.h[3], b.curve.h[3], 1e-15));
    }

    void bootstrap_on_zero_curve() {
        rates::ZeroCurve z;
        z.t = {0.5, 1, 2, 5, 10};
        z.r = {0.040, 0.041, 0.042, 0.043, 0.044};
        const std::vector<double> spreads = sample_spreads();
        const cr::Bootstrap b = cr::bootstrap(kTenors, spreads, 0.40, z, conv(4, cr::DayCount::Act360));
        QVERIFY2(b.ok, b.message.c_str());
        // Python (exact integral of the linearly interpolated zero curve). CreditMath takes the forward
        // flat on pieces of ≤ 1/24 year, so agreement is to ~1e-8 rather than machine precision.
        const double h[] = {0.010088300244, 0.015197376311, 0.017798847913, 0.023182071636,
                            0.028800601883, 0.029847920056, 0.028928762091, 0.030892718714};
        for (int i = 0; i < 8; ++i) {
            QVERIFY2(near(b.curve.h[i], h[i], 2e-7), msg(b.curve.h[i], h[i]));
            QVERIFY2(std::fabs(b.repriced[i] - spreads[i]) * 1e4 < 1e-8, msg(b.repriced[i], spreads[i]));
        }
    }

    // [OK] credit triangle: a flat spread curve gives a flat hazard close to S / (1 − R).
    void credit_triangle() {
        const std::vector<double> flat(kTenors.size(), 0.01);
        const cr::Bootstrap q = cr::bootstrap(kTenors, flat, 0.40, cr::flat_curve(0.04), conv(4, cr::DayCount::Act360));
        QVERIFY(q.ok);
        for (double h : q.curve.h) // Python: 0.016813857417 (ACT/360 pays 365/360 more premium)
            QVERIFY2(near(h, 0.016813857417, 1e-10), msg(h, 0.016813857417));
        const cr::Bootstrap m = cr::bootstrap(kTenors, flat, 0.40, cr::flat_curve(0.04), conv(12, cr::DayCount::Act365F));
        QVERIFY(m.ok);
        for (double h : m.curve.h) { // Python: 0.01663891074 vs 0.01 / 0.6 = 0.0166667
            QVERIFY2(near(h, 0.01663891074, 1e-10), msg(h, 0.01663891074));
            QVERIFY(std::fabs(h / (0.01 / 0.6) - 1.0) < 0.002);
        }
    }

    // [OK] upfront = (S − C) x RPV01 on the flat curve of the quoted spread, and its inverse.
    void upfront_conversion() {
        const rates::ZeroCurve z = cr::flat_curve(0.04);
        const cr::Conventions c = conv(4, cr::DayCount::Act360);
        const cr::Conversion u = cr::upfront_from_spread(0.03, 0.01, 5.0, 0.40, z, c);
        QVERIFY2(u.ok, u.message.c_str());
        // Python: flat hazard 0.050441927402, rpv01 4.057791671751, upfront 0.081155833435.
        QVERIFY2(near(u.hazard, 0.050441927402, 1e-10), msg(u.hazard, 0.050441927402));
        QVERIFY2(near(u.rpv01, 4.057791671751, 1e-9), msg(u.rpv01, 4.057791671751));
        QVERIFY2(near(u.upfront, 0.081155833435, 1e-10), msg(u.upfront, 0.081155833435));
        QVERIFY(near(u.upfront, (0.03 - 0.01) * u.rpv01, 1e-12));
        // A 500 bp coupon on a 300 bp name: the buyer RECEIVES the same amount.
        const cr::Conversion u5 = cr::upfront_from_spread(0.03, 0.05, 5.0, 0.40, z, c);
        QVERIFY(u5.ok && near(u5.upfront, -0.081155833435, 1e-10));
        // Coupon = spread: no upfront.
        QVERIFY(near(cr::upfront_from_spread(0.0125, 0.0125, 5.0, 0.40, z, c).upfront, 0.0, 1e-14));
        // Inverse.
        const cr::Conversion s = cr::spread_from_upfront(0.081155833435, 0.01, 5.0, 0.40, z, c);
        QVERIFY2(s.ok, s.message.c_str());
        QVERIFY2(near(s.spread, 0.03, 1e-11), msg(s.spread, 0.03));
        const cr::Conversion s5 = cr::spread_from_upfront(-0.081155833435, 0.05, 5.0, 0.40, z, c);
        QVERIFY(s5.ok && near(s5.spread, 0.03, 1e-11));
        // Out of range: more than 1 − R up front, or a negative spread needed.
        QCOMPARE(int(cr::spread_from_upfront(0.70, 0.01, 5.0, 0.40, z, c).error), int(cr::ErrorKind::UpfrontRange));
        QCOMPARE(int(cr::spread_from_upfront(-0.30, 0.01, 5.0, 0.40, z, c).error), int(cr::ErrorKind::UpfrontRange));
    }

    void position_risk_sample() {
        const rates::ZeroCurve z = cr::flat_curve(0.04);
        const cr::Conventions c = conv(4, cr::DayCount::Act360);
        cr::Position p;
        p.notional = 10e6;
        p.coupon = 0.01;
        p.maturity = 5.0;
        p.buy_protection = true;
        const cr::Risk r = cr::position_risk(kTenors, sample_spreads(), 0.40, z, c, p);
        QVERIFY2(r.ok, r.message.c_str());
        // Python (credit_ref2.py): buyer PV 131189.660178, upfront 0.013118966018, par 130 bp,
        // survival 0.893900485111, JTD 5868810.339822, CS01 4318.707983, recovery01 −98.146169,
        // bucket 5y 4361.684101, bucket 7y 0.
        QVERIFY2(near(r.pv, 131189.660178, 1e-3), msg(r.pv, 131189.660178));
        QVERIFY2(near(r.upfront, 0.013118966018, 1e-11), msg(r.upfront, 0.013118966018));
        QVERIFY(near(r.par_spread * 1e4, 130.0, 1e-8));
        QVERIFY(near(r.survival, 0.893900485111, 1e-10));
        QVERIFY(near(r.default_probability, 1.0 - 0.893900485111, 1e-10));
        QVERIFY2(near(r.jtd, 5868810.339822, 1e-3), msg(r.jtd, 5868810.339822));
        QVERIFY2(near(r.cs01, 4318.707983, 1e-3), msg(r.cs01, 4318.707983));
        QVERIFY2(near(r.recovery01, -98.146169, 1e-3), msg(r.recovery01, -98.146169));
        QCOMPARE(int(r.bucket_cs01.size()), 8);
        QVERIFY2(near(r.bucket_cs01[5], 4361.684101, 1e-3), msg(r.bucket_cs01[5], 4361.684101));
        QVERIFY(near(r.bucket_cs01[6], 0.0, 1e-6) && near(r.bucket_cs01[7], 0.0, 1e-6));
        QVERIFY(near(r.protection_leg - r.premium_leg, r.pv, 1e-6));
        // Signs: the protection buyer gains when spreads widen and on default.
        QVERIFY(r.cs01 > 0 && r.jtd > 0);
        QVERIFY(std::isfinite(r.ir_dv01));
        // The seller holds the mirror image.
        p.buy_protection = false;
        const cr::Risk s = cr::position_risk(kTenors, sample_spreads(), 0.40, z, c, p);
        QVERIFY(s.ok);
        QVERIFY(near(s.pv, -r.pv, 1e-6) && near(s.cs01, -r.cs01, 1e-6) && near(s.jtd, -r.jtd, 1e-6));
        QVERIFY(s.cs01 < 0 && s.jtd < 0);
        // Cancelled between re-bootstraps.
        const cr::Risk x = cr::position_risk(kTenors, sample_spreads(), 0.40, z, c, p, [] { return true; });
        QVERIFY(!x.ok);
        QCOMPARE(int(x.error), int(cr::ErrorKind::Cancelled));
    }

    void plain_errors() {
        const rates::ZeroCurve z = cr::flat_curve(0.04);
        // Inverted so steeply that the 5y quote needs a negative intensity after the 1y one.
        const cr::Bootstrap inv = cr::bootstrap({1.0, 5.0}, {0.03, 0.005}, 0.40, z);
        QVERIFY(!inv.ok);
        QCOMPARE(int(inv.error), int(cr::ErrorKind::NegativeHazard));
        QCOMPARE(inv.index, 1);
        QVERIFY(!inv.message.empty());
        // A mildly inverted curve is fine.
        QVERIFY(cr::bootstrap({1.0, 5.0}, {0.03, 0.025}, 0.40, z).ok);
        QCOMPARE(int(cr::bootstrap({}, {}, 0.4, z).error), int(cr::ErrorKind::NoQuotes));
        QCOMPARE(int(cr::bootstrap({1.0, 1.0}, {0.01, 0.02}, 0.4, z).error), int(cr::ErrorKind::DuplicateTenor));
        QCOMPARE(int(cr::bootstrap({1.0, -2.0}, {0.01, 0.02}, 0.4, z).error), int(cr::ErrorKind::BadTenor));
        QCOMPARE(int(cr::bootstrap({1.0, 2.0}, {0.01, 0.0}, 0.4, z).error), int(cr::ErrorKind::BadSpread));
        QCOMPARE(int(cr::bootstrap({1.0}, {0.01}, 1.0, z).error), int(cr::ErrorKind::BadRecovery));
        cr::Position p;
        p.maturity = 0.0;
        QCOMPARE(int(cr::position_risk({1.0}, {0.01}, 0.4, z, {}, p).error), int(cr::ErrorKind::BadMaturity));
        // Discounting on an empty curve (zero rates) still prices.
        QVERIFY(cr::bootstrap({1.0}, {0.01}, 0.4, rates::ZeroCurve{}).ok);
    }
};

QTEST_GUILESS_MAIN(TestDerivCredit)
#include "tst_deriv_credit.moc"
