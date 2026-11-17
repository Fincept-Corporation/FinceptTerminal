// tst_alt_credit_math — private credit analytics (src/screens/alt_investments/AltCreditMath.h)
// against textbook values:
//  * a par bond yields its coupon; 6% 5-year annual par bond: Macaulay duration 4.4651 years,
//    modified 4.2124 (sum t*CF/1.06^t / 100);
//  * a 10-year 8% annual-coupon bond priced at 87.7108 yields 10% (8 x annuity(10%, 10) +
//    100 / 1.1^10 = 49.1565 + 38.5543);
//  * semi-annual 5% par: bond-equivalent yield 5%, effective (1.025^2 - 1) = 5.0625%;
//  * repricing the 6% par bond at 7%: 95.8998 (change -4.1002);
//  * expected loss PD 2% x LGD 40% = 0.80% a year; 5-year cumulative PD 1 - 0.98^5 = 9.6079%;
//  * straight-line amortisation 10%/yr over 3 years: principal 10, 10, 80, WAL 2.7 years;
//  * floating coupon = max(base, floor) + spread, and a rate shock that hits the floor.

#include "screens/alt_investments/AltCreditMath.h"

#include <QtTest>

#include <cmath>

using namespace fincept::screens::alt_credit;

namespace {
LoanInputs fixed_bond(double coupon, double price, double years, int freq) {
    LoanInputs in;
    in.face = 100;
    in.price_pct = price;
    in.upfront_fee_pct = 0;
    in.floating = false;
    in.coupon_pct = coupon;
    in.maturity_years = years;
    in.frequency = freq;
    in.amort_pct = 0;
    in.pd_pct = 2.0;
    in.lgd_pct = 40.0;
    return in;
}
bool near(double a, double b, double tol) {
    return std::fabs(a - b) <= tol;
}
} // namespace

class TstAltCreditMath : public QObject {
    Q_OBJECT
  private slots:
    void par_bond_yields_its_coupon() {
        const LoanResult r = analyse(fixed_bond(6.0, 100.0, 5, 1));
        QVERIFY(r.problem.isEmpty());
        QVERIFY(near(r.yield_pct, 6.0, 1e-7));
        QVERIFY(near(r.macaulay_years, 4.46511, 1e-4));
        QVERIFY(near(r.modified_duration, 4.21237, 1e-4));
        QVERIFY(near(r.rate_duration, r.modified_duration, 1e-12));
    }

    void discount_bond_textbook_yield() {
        const LoanResult r = analyse(fixed_bond(8.0, 87.7108, 10, 1));
        QVERIFY(near(r.yield_pct, 10.0, 1e-3));
    }

    void semiannual_effective_rate() {
        const LoanResult r = analyse(fixed_bond(5.0, 100.0, 2, 2));
        QVERIFY(near(r.yield_pct, 5.0, 1e-7));
        QVERIFY(near(r.effective_yield_pct, 5.0625, 1e-6));
    }

    void fixed_rate_shock_reprices() {
        const LoanResult r = analyse(fixed_bond(6.0, 100.0, 5, 1));
        QCOMPARE(r.shocks.size(), 4);
        const RateShock up100 = r.shocks.at(2);
        QCOMPARE(up100.bps, 100);
        QVERIFY(near(up100.value_change, -4.1002, 1e-3));
        QVERIFY(std::isnan(up100.income_change));
    }

    void expected_loss() {
        const LoanResult r = analyse(fixed_bond(6.0, 100.0, 5, 1));
        QVERIFY(near(r.expected_loss_pct, 0.8, 1e-12));
        QVERIFY(near(r.cumulative_pd_pct, 9.60792, 1e-4));
        QVERIFY(near(r.loss_adjusted_yield_pct, 5.2, 1e-6));
    }

    void amortisation_and_wal() {
        LoanInputs in = fixed_bond(6.0, 100.0, 3, 1);
        in.amort_pct = 10.0;
        const LoanResult r = analyse(in);
        QCOMPARE(r.periods.size(), 3);
        QVERIFY(near(r.periods.at(0).principal, 10.0, 1e-12));
        QVERIFY(near(r.periods.at(1).principal, 10.0, 1e-12));
        QVERIFY(near(r.periods.at(2).principal, 80.0, 1e-12));
        QVERIFY(near(r.periods.at(1).interest, 90.0 * 0.06, 1e-12)); // on the balance after year 1
        QVERIFY(near(r.weighted_average_life, 2.7, 1e-12));
        QVERIFY(near(r.yield_pct, 6.0, 1e-7)); // still par
    }

    void floating_coupon_and_floor() {
        LoanInputs in;
        in.face = 1'000'000;
        in.price_pct = 100;
        in.upfront_fee_pct = 0;
        in.floating = true;
        in.base_pct = 4.3;
        in.floor_pct = 1.0;
        in.spread_bps = 550;
        QVERIFY(near(coupon_rate_pct(in), 9.8, 1e-12));
        in.base_pct = 0.5; // under the floor
        QVERIFY(near(coupon_rate_pct(in), 6.5, 1e-12));

        in.base_pct = 1.5;
        const LoanResult r = analyse(in);
        QVERIFY(near(r.yield_pct, 7.0, 1e-6));   // par: yield = coupon 1.5 + 5.5
        QVERIFY(near(r.spread_bps, 550, 1e-4));  // over the base
        QVERIFY(near(r.rate_duration, 0.25, 1e-12)); // quarterly reset
        // -200 bp: base 1.5 -> -0.5, floored at 1.0: income falls by 0.5% of face only.
        QCOMPARE(r.shocks.at(0).bps, -200);
        QVERIFY(near(r.shocks.at(0).income_change, -5'000, 1e-6));
        QVERIFY(near(r.shocks.at(3).income_change, 20'000, 1e-6)); // +200 bp
    }

    void discount_and_fee_raise_the_yield() {
        LoanInputs in = fixed_bond(8.0, 98.0, 5, 4);
        in.upfront_fee_pct = 1.0;
        in.call_years = 3;
        const LoanResult r = analyse(in);
        QVERIFY(r.yield_pct > 8.0);
        QVERIFY(r.yield_to_call_pct > r.yield_pct); // the discount is earned over a shorter life
        QVERIFY(near(r.cost, 97.0, 1e-12));
    }

    void bad_inputs() {
        LoanInputs in = fixed_bond(6.0, 0.0, 5, 1);
        QVERIFY(!analyse(in).problem.isEmpty());
        in = fixed_bond(6.0, 100.0, 0.0, 1);
        QVERIFY(!analyse(in).problem.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstAltCreditMath)
#include "tst_alt_credit_math.moc"
