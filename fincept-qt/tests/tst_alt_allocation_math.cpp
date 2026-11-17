// tst_alt_allocation_math — the ALLOCATION & LIQUIDITY arithmetic
// (src/screens/alt_investments/AltAllocationMath.h). A worked book, by hand:
//   listed equity 600 (daily), listed bonds 200 (daily), hedge fund 100 (quarterly, alternative),
//   PE fund NAV 100 (locked, alternative, unfunded 80).
//   total 1000; alternatives 200 (20%); ladder 800 / 800 / 900 / 900 / 1000;
//   liquid within 90 days 900 -> unfunded cover 900 / 80 = 11.25x.
// Stress: listed -20% -> equity 480, bonds 160; calls 50% of 80 = 40 paid pro rata from the 640
// listed -> 600 listed; PE NAV 140; total 600 + 100 + 140 = 840; alternatives 240 = 28.571%;
// liquid within 90 days 600 + 100 = 700; unfunded 40 -> cover 17.5x.

#include "screens/alt_investments/AltAllocationMath.h"

#include <QtTest>

#include <cmath>

using namespace fincept::screens::alt_alloc;

namespace {
QVector<Position> book() {
    return {
        {QStringLiteral("Equities"), QStringLiteral("equity"), 600, Daily, 0, false},
        {QStringLiteral("Bonds"), QStringLiteral("bond"), 200, Daily, 0, false},
        {QStringLiteral("Hedge fund"), QStringLiteral("hedge_fund"), 100, Quarterly, 0, true},
        {QStringLiteral("PE fund"), QStringLiteral("private_equity"), 100, Locked, 80, true},
    };
}
bool near(double a, double b) {
    return std::fabs(a - b) < 1e-9;
}
} // namespace

class TstAltAllocationMath : public QObject {
    Q_OBJECT
  private slots:
    void totals_and_weights() {
        const auto b = book();
        QVERIFY(near(total_value(b), 1000));
        QVERIFY(near(alternative_value(b), 200));
        QVERIFY(near(total_unfunded(b), 80));
        const auto cls = by_class(b);
        QVERIFY(near(cls.value(QStringLiteral("equity")), 600));
        QVERIFY(near(cls.value(QStringLiteral("private_equity")), 100));
    }

    void ladder_and_cover() {
        const auto b = book();
        const QVector<double> l = ladder(b);
        QCOMPARE(l.size(), kTierCount);
        QVERIFY(near(l[Daily], 800));
        QVERIFY(near(l[Monthly], 800));
        QVERIFY(near(l[Quarterly], 900));
        QVERIFY(near(l[Annual], 900));
        QVERIFY(near(l[Locked], 1000));
        QVERIFY(near(liquid_within(b, 92), 900));
        QVERIFY(near(unfunded_cover(b), 11.25));
    }

    void denominator_effect_stress() {
        const StressResult r = stress(book(), -20.0, 50.0);
        QVERIFY(near(r.total, 840));
        QVERIFY(near(r.alternatives, 240));
        QVERIFY(std::fabs(r.alt_weight_pct - 28.571428571) < 1e-6);
        QVERIFY(near(r.liquid_90d, 700));
        QVERIFY(near(r.unfunded, 40));
        QVERIFY(near(r.cover, 17.5));
    }

    void no_unfunded_has_no_cover() {
        QVector<Position> b = book();
        b[3].unfunded = 0;
        QVERIFY(std::isnan(unfunded_cover(b)));
    }

    void tiers_from_keys() {
        QCOMPARE(tier_from_key(QStringLiteral("daily")), int(Daily));
        QCOMPARE(tier_from_key(QStringLiteral("quarterly")), int(Quarterly));
        QCOMPARE(tier_from_key(QStringLiteral("anything else")), int(Locked));
        QVERIFY(tier_days(Locked) > tier_days(Annual));
    }
};

QTEST_GUILESS_MAIN(TstAltAllocationMath)
#include "tst_alt_allocation_math.moc"
