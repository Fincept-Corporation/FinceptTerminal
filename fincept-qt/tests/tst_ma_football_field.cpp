// tst_ma_football_field — the M&A football field's ranges (src/screens/ma_analytics/MaFootballFieldMath.h).
// Reference values are hand-computed in each test.
#include "screens/ma_analytics/MaFootballFieldMath.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QtTest/QtTest>

using namespace fincept::screens::ma_ff;

class TstMaFootballField : public QObject {
    Q_OBJECT
  private slots:
    void bridge_round_trip() {
        // EV 1.6bn, debt 1.0bn, cash 0.5bn, 100m shares -> (1.6 - 1.0 + 0.5) bn / 100m = 11.00 per share.
        Bridge b{100e6, 1e9, 0.5e9};
        QCOMPARE(ev_to_per_share(1.6e9, b), 11.0);
        QCOMPARE(per_share_to_ev(11.0, b), 1.6e9);
        Bridge none;
        QVERIFY(!std::isfinite(ev_to_per_share(1.6e9, none)));
    }

    void crossing_interpolates() {
        // IRR falls from 25% at 7.0x to 20% at 8.0x: 22.5% is crossed half-way, at 7.5x.
        const QVector<double> xs{6, 7, 8, 9, 10};
        const QVector<double> ys{30, 25, 20, 16, 12};
        QCOMPARE(crossing(xs, ys, 22.5), 7.5);
        QCOMPARE(crossing(xs, ys, 25.0), 7.0);
        QVERIFY(!std::isfinite(crossing(xs, ys, 40.0)));
    }

    void lbo_band_at_base_exit() {
        // Rows = entry 6..10x, columns = exit 5..9x; base exit = column 2 (7x) holds 30/25/20/16/12.
        QJsonArray irr;
        const double col2[] = {30, 25, 20, 16, 12};
        for (double v : col2)
            irr.append(QJsonArray{v + 10, v + 5, v, v - 5, v - 10});
        QJsonObject p{{"entry_multiple_range", QJsonArray{6, 7, 8, 9, 10}},
                      {"exit_multiple_range", QJsonArray{5, 6, 7, 8, 9}},
                      {"irr_pct_matrix", irr}};
        // 20-25% IRR -> entry 7.0x-8.0x of a 200m EBITDA = 1.4bn-1.6bn.
        const Range r = lbo_range(p, 200e6, 20, 25);
        QVERIFY(r.valid());
        QCOMPARE(r.low, 1.4e9);
        QCOMPARE(r.high, 1.6e9);
        // A 35-40% band is outside the grid: no range, a note says why.
        const Range out = lbo_range(p, 200e6, 35, 40);
        QVERIFY(!out.valid());
        QVERIFY(out.note.contains(QStringLiteral("outside")));
    }

    void dcf_grid_range_and_base() {
        QJsonObject sens{{"price_per_share", QJsonArray{QJsonArray{60, 64}, QJsonArray{50, QJsonValue()}}},
                         {"enterprise_value", QJsonArray{QJsonArray{6e9, 6.4e9}, QJsonArray{5e9, 5.2e9}}},
                         {"base_row", 1},
                         {"base_col", 0},
                         {"wacc_values", QJsonArray{9.0, 10.0}},
                         {"terminal_growth_values", QJsonArray{2.0, 2.5}}};
        QJsonObject p{{"sensitivity", sens}};
        const Range ps = dcf_range(p, Basis::PerShare);
        QCOMPARE(ps.low, 50.0); // the empty cell is skipped
        QCOMPARE(ps.high, 64.0);
        QCOMPARE(ps.mid, 50.0);
        QVERIFY(!ps.basis_ev);
        const Range ev = dcf_range(p, Basis::EnterpriseValue);
        QCOMPARE(ev.low, 5e9);
        QCOMPARE(ev.high, 6.4e9);
        QVERIFY(ev.basis_ev);
    }

    void multiples_skip_missing() {
        QJsonObject v{{"ev_ebitda_q1", 1.8e9}, {"ev_ebitda_q3", 2.4e9}, {"ev_ebitda_median", 2.1e9},
                      {"ev_revenue_q1", 0}, {"ev_revenue_q3", 0}};
        QJsonObject p{{"target_valuation", QJsonObject{{"valuations", v}}}};
        const QVector<Range> rows = multiple_ranges(p, QStringLiteral("comps"), QStringLiteral("Trading comps"));
        QCOMPARE(rows.size(), 1); // EV/Revenue came back as 0: not a range
        QCOMPARE(rows.first().low, 1.8e9);
        QCOMPARE(rows.first().mid, 2.1e9);
    }

    void convert_and_axis() {
        Range r;
        r.low = 1.4e9;
        r.high = 1.6e9;
        r.basis_ev = true;
        Bridge b{100e6, 1e9, 0.5e9};
        QVERIFY(to_basis(&r, Basis::PerShare, b));
        QCOMPARE(r.low, 9.0);  // (1.4 - 1.0 + 0.5) bn / 100m
        QCOMPARE(r.high, 11.0);
        Range s;
        s.low = 1.0;
        s.high = 2.0;
        s.basis_ev = true;
        QVERIFY(!to_basis(&s, Basis::PerShare, Bridge{}));
        double lo = 0, hi = 0;
        QVERIFY(axis({r}, {12.0}, 0.1, &lo, &hi));
        QCOMPARE(lo, 9.0 - 0.3);  // span 9..12 = 3, padded 10%
        QCOMPARE(hi, 12.0 + 0.3);
    }
};

QTEST_APPLESS_MAIN(TstMaFootballField)
#include "tst_ma_football_field.moc"
