// tst_sensitivity_grid — the pure rules behind ui::SensitivityTable (src/ui/widgets/SensitivityGrid.h):
// colour-scale position (diverging around the base cell / an explicit midpoint, sequential, the
// "smaller is better" direction), missing cells, the exported header / rows / TSV text and the
// centred axis builder. Expected values are worked by hand in the comments.

#include "ui/widgets/SensitivityGrid.h"

#include <QtTest>

#include <cmath>
#include <limits>

using namespace fincept::ui;

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

SensitivityGrid make_grid() {
    SensitivityGrid g;
    g.row_title = QStringLiteral("WACC");
    g.col_title = QStringLiteral("Growth");
    g.row_labels = {QStringLiteral("8%"), QStringLiteral("9%"), QStringLiteral("10%")};
    g.col_labels = {QStringLiteral("2%"), QStringLiteral("3%"), QStringLiteral("4%")};
    SensitivityMetric m;
    m.name = QStringLiteral("Value");
    m.values = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    g.metrics << m;
    return g;
}

} // namespace

class TstSensitivityGrid : public QObject {
    Q_OBJECT
  private slots:
    void diverging_centres_on_base_cell() {
        SensitivityGrid g = make_grid();
        g.base_row = 1;
        g.base_col = 1; // value 5: centre; data 1..9 -> span max(|9-5|, |5-1|) = 4
        QCOMPARE(sensitivity_detail::resolved_midpoint(g, 0), 5.0);
        QCOMPARE(sensitivity_detail::tone(g, 0, 9.0), 1.0);
        QCOMPARE(sensitivity_detail::tone(g, 0, 1.0), -1.0);
        QCOMPARE(sensitivity_detail::tone(g, 0, 7.0), 0.5);   // (7 - 5) / 4
        QCOMPARE(sensitivity_detail::tone(g, 0, 5.0), 0.0);
    }

    void diverging_without_base_uses_data_midpoint() {
        SensitivityGrid g = make_grid(); // no base: mid = (1 + 9) / 2 = 5
        QCOMPARE(sensitivity_detail::resolved_midpoint(g, 0), 5.0);
        QCOMPARE(sensitivity_detail::tone(g, 0, 3.0), -0.5);
    }

    void explicit_midpoint_wins() {
        SensitivityGrid g = make_grid();
        g.base_row = 1;
        g.base_col = 1;
        g.metrics[0].midpoint = 0.0; // accretion %: zero is the dividing line; span = max(9, 0) = 9
        QCOMPARE(sensitivity_detail::resolved_midpoint(g, 0), 0.0);
        QCOMPARE(sensitivity_detail::tone(g, 0, 9.0), 1.0);
        QVERIFY(std::fabs(sensitivity_detail::tone(g, 0, 1.0) - 1.0 / 9.0) < 1e-12);
    }

    void lower_is_better_flips_the_direction() {
        SensitivityGrid g = make_grid();
        g.base_row = 1;
        g.base_col = 1;
        g.metrics[0].higher_is_better = false;
        QCOMPARE(sensitivity_detail::tone(g, 0, 9.0), -1.0);
        QCOMPARE(sensitivity_detail::tone(g, 0, 1.0), 1.0);
    }

    void sequential_scale() {
        SensitivityGrid g = make_grid();
        g.metrics[0].diverging = false; // 0 at the minimum, 1 at the maximum
        QCOMPARE(sensitivity_detail::tone(g, 0, 1.0), 0.0);
        QCOMPARE(sensitivity_detail::tone(g, 0, 9.0), 1.0);
        QCOMPARE(sensitivity_detail::tone(g, 0, 5.0), 0.5);
        g.metrics[0].higher_is_better = false;
        QCOMPARE(sensitivity_detail::tone(g, 0, 1.0), 1.0);
    }

    void missing_cells() {
        SensitivityGrid g = make_grid();
        g.metrics[0].values[0][2] = kNaN;
        g.metrics[0].values[2].resize(2); // a short row: its last cell is missing too
        QVERIFY(std::isnan(sensitivity_detail::tone(g, 0, kNaN)));
        QCOMPARE(sensitivity_detail::cell_text(g, 0, 0, 2, {}), QStringLiteral("-"));
        QCOMPARE(sensitivity_detail::cell_text(g, 0, 2, 2, {}), QStringLiteral("-"));
        double lo = 0, hi = 0;
        QVERIFY(sensitivity_detail::finite_range(g.metrics[0], &lo, &hi));
        QCOMPARE(lo, 1.0);
        QCOMPARE(hi, 8.0);
        SensitivityMetric empty;
        empty.values = {{kNaN}};
        QVERIFY(!sensitivity_detail::finite_range(empty, &lo, &hi));
    }

    void export_text() {
        SensitivityGrid g = make_grid();
        const auto fmt = [](double v) { return QString::number(v, 'f', 1); };
        QCOMPARE(sensitivity_detail::headers(g),
                 (QStringList{QStringLiteral("WACC \\ Growth"), QStringLiteral("2%"), QStringLiteral("3%"),
                              QStringLiteral("4%")}));
        const auto rows = sensitivity_detail::rows(g, 0, fmt);
        QCOMPARE(rows.size(), 3);
        QCOMPARE(rows.at(1), (QStringList{QStringLiteral("9%"), QStringLiteral("4.0"), QStringLiteral("5.0"),
                                          QStringLiteral("6.0")}));
        QCOMPARE(sensitivity_detail::to_tsv(g, 0, fmt),
                 QStringLiteral("WACC \\ Growth\t2%\t3%\t4%\n8%\t1.0\t2.0\t3.0\n9%\t4.0\t5.0\t6.0\n10%\t7.0\t8.0\t9.0"));
        // A metric's own formatter wins over the fallback; several metrics name the block.
        SensitivityMetric second;
        second.name = QStringLiteral("MOIC");
        second.values = {{1, 1, 1}, {2, 2, 2}, {3, 3, 3}};
        second.format = [](double v) { return QString::number(v, 'f', 2) + QLatin1Char('x'); };
        g.metrics << second;
        QVERIFY(sensitivity_detail::to_tsv(g, 1, fmt).startsWith(QStringLiteral("MOIC\nWACC \\ Growth")));
        QCOMPARE(sensitivity_detail::cell_text(g, 1, 2, 0, fmt), QStringLiteral("3.00x"));
    }

    void centred_axis() {
        const QVector<double> a = sensitivity_detail::centred_axis(0.09, 0.005, 5);
        QCOMPARE(a.size(), 5);
        QVERIFY(std::fabs(a.first() - 0.08) < 1e-12);
        QVERIFY(std::fabs(a.at(2) - 0.09) < 1e-12);
        QVERIFY(std::fabs(a.last() - 0.10) < 1e-12);
        QVERIFY(sensitivity_detail::centred_axis(1.0, 1.0, 0).isEmpty());
    }

    void base_cell_bounds() {
        SensitivityGrid g = make_grid();
        g.base_row = 3; // outside: no base
        g.base_col = 0;
        QVERIFY(!g.has_base());
        QCOMPARE(sensitivity_detail::resolved_midpoint(g, 0), 5.0);
        QVERIFY(!g.is_empty());
        QVERIFY(SensitivityGrid{}.is_empty());
    }
};

QTEST_GUILESS_MAIN(TstSensitivityGrid)
#include "tst_sensitivity_grid.moc"
