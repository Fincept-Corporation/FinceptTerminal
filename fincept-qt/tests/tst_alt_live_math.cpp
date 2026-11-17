// tst_alt_live_math — the Alternative Investments LIVE DATA prefill arithmetic
// (src/screens/alt_investments/AltLiveMath.h): Treasury par yield at a bond's maturity by linear
// interpolation (flat beyond the curve ends, gaps skipped) and the futures contract nearest to
// "n months ahead". Expected values worked by hand in the comments.

#include "screens/alt_investments/AltLiveMath.h"

#include <QtTest>

#include <cmath>
#include <limits>

using namespace fincept::screens;

class TstAltLiveMath : public QObject {
    Q_OBJECT
  private slots:
    void interpolates_between_tenors() {
        // 2y 4.00 %, 5y 4.30 %: 3y = 4.00 + (1/3) * 0.30 = 4.10 %
        const QVector<double> t{0.25, 2.0, 5.0, 10.0};
        const QVector<double> y{4.50, 4.00, 4.30, 4.60};
        QVERIFY(std::fabs(alt_live::interpolate(t, y, 3.0) - 4.10) < 1e-12);
        QCOMPARE(alt_live::interpolate(t, y, 5.0), 4.30);   // on a tenor
        QCOMPARE(alt_live::interpolate(t, y, 0.1), 4.50);   // flat before the first
        QCOMPARE(alt_live::interpolate(t, y, 30.0), 4.60);  // flat after the last
        // 7.5y = halfway between 5y and 10y: 4.45 %
        QVERIFY(std::fabs(alt_live::interpolate(t, y, 7.5) - 4.45) < 1e-12);
    }

    void skips_missing_points() {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const QVector<double> t{1.0, 2.0, 3.0};
        const QVector<double> y{4.0, nan, 5.0}; // the 2y point is missing: 2y = 4.5 from 1y..3y
        QCOMPARE(alt_live::interpolate(t, y, 2.0), 4.5);
        QVERIFY(std::isnan(alt_live::interpolate({}, {}, 2.0)));
        QVERIFY(std::isnan(alt_live::interpolate(t, {nan, nan, nan}, 2.0)));
    }

    void nearest_contract_month() {
        const QVector<QDate> m{QDate(2026, 12, 1), QDate(2027, 1, 1), QDate(2027, 2, 1), QDate(2027, 3, 1),
                               QDate(2027, 5, 1), QDate(2027, 11, 1)};
        const QVector<double> p{60.1, 60.4, 60.8, 61.0, 61.5, 62.0};
        // 17 Nov 2026 + 3 months = 17 Feb 2027: Feb (16 days) beats Mar (12 days)? 1 Mar is 12 days away -> Mar.
        QCOMPARE(alt_live::nearest_contract(m, p, QDate(2027, 2, 17)), 3);
        // + 6 months = 17 May 2027 -> May (16 days)
        QCOMPARE(alt_live::nearest_contract(m, p, QDate(2027, 5, 17)), 4);
        // + 12 months = 17 Nov 2027 -> Nov 2027
        QCOMPARE(alt_live::nearest_contract(m, p, QDate(2027, 11, 17)), 5);
        // A contract without a price is skipped.
        QVector<double> gaps = p;
        gaps[3] = std::numeric_limits<double>::quiet_NaN();
        QCOMPARE(alt_live::nearest_contract(m, gaps, QDate(2027, 2, 17)), 2);
        // Tie: 1 Jan and 1 Mar are both 30 days from 30 Jan? (1 Jan -> 29 days, 1 Mar -> 30): Jan.
        QCOMPARE(alt_live::nearest_contract({QDate(2027, 1, 1), QDate(2027, 3, 1)}, {1.0, 2.0}, QDate(2027, 1, 30)), 0);
        // An exact tie goes to the later month: 1 Jan and 1 Mar around 30/31 Jan... use 15-day symmetric case.
        QCOMPARE(alt_live::nearest_contract({QDate(2027, 1, 1), QDate(2027, 1, 31)}, {1.0, 2.0}, QDate(2027, 1, 16)), 1);
        QCOMPARE(alt_live::nearest_contract({}, {}, QDate(2027, 1, 1)), -1);
    }
};

QTEST_GUILESS_MAIN(TstAltLiveMath)
#include "tst_alt_live_math.moc"
