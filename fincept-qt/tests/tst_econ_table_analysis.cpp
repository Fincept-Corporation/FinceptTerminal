// tst_econ_table_analysis — the pure half of the Economics results view
// (src/screens/economics/panels/EconTableAnalysis.cpp), which EconPanelBase runs on a worker thread:
// column order and the newest-first sort, the stat cards, the chart lines, a sort by any column, the
// CSV text, and the min / max thinning of chart lines to the pixel width (extremes never dropped).
// QtCore only.

#include "screens/economics/panels/EconTableAnalysis.h"

#include <QDate>
#include <QJsonArray>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QTest>
#include <QTimeZone>

#include <algorithm>
#include <cmath>

namespace et = fincept::screens::econ_table;

namespace {

QJsonObject tst_eta_table(const QJsonArray& rows, const QJsonObject& meta = {}, bool keep = false) {
    return QJsonObject{{QStringLiteral("rows"), rows},
                       {QStringLiteral("title"), QStringLiteral("Test series")},
                       {QStringLiteral("order"), QJsonArray()},
                       {QStringLiteral("keep"), keep},
                       {QStringLiteral("meta"), meta}};
}

QJsonObject tst_eta_row(const QString& date, double value) {
    return QJsonObject{{QStringLiteral("date"), date}, {QStringLiteral("value"), value}};
}

} // namespace

class TstEconTableAnalysis : public QObject {
    Q_OBJECT
  private slots:
    void periods() {
        QCOMPARE(et::parse_period(QStringLiteral("2024-03")).date, QDate(2024, 3, 1));
        QCOMPARE(et::parse_period(QStringLiteral("2024-Q2")).date, QDate(2024, 4, 1));
        QCOMPARE(et::parse_period(QStringLiteral("2024M11")).date, QDate(2024, 11, 1));
        QCOMPARE(et::parse_period(QStringLiteral("02 Jan 1975")).date, QDate(1975, 1, 2));
        QCOMPARE(et::parse_period(QStringLiteral("2025年09月份")).date, QDate(2025, 9, 1));
        QVERIFY(!et::parse_period(QStringLiteral("8:30am")).date.isValid());
        QCOMPARE(et::period_text(QJsonValue(2024.0)), QStringLiteral("2024"));
        double v = 0;
        QVERIFY(et::to_number(QJsonValue(QStringLiteral("1,234.5")), &v));
        QCOMPARE(v, 1234.5);
        QVERIFY(et::to_number(QJsonValue(QStringLiteral("4.2%")), &v));
        QCOMPARE(v, 4.2);
    }

    void columns_sort_and_cards() {
        // Monthly series sent oldest-first and out of order: shown newest first, date then value.
        QJsonArray rows;
        rows << tst_eta_row(QStringLiteral("2024-02"), 3.0) << tst_eta_row(QStringLiteral("2024-01"), 2.0)
             << tst_eta_row(QStringLiteral("2024-04"), 5.0) << tst_eta_row(QStringLiteral("2024-03"), 4.0)
             << tst_eta_row(QStringLiteral("2023-04"), 1.0);
        const et::Analysis a = et::analyse(tst_eta_table(rows));
        QCOMPARE(a.columns, (QStringList{QStringLiteral("date"), QStringLiteral("value")}));
        QCOMPARE(a.sort_column, 0);
        QVERIFY(a.sort_descending);
        QCOMPARE(a.order.size(), 5);
        QCOMPARE(a.rows[a.order.first()].toObject().value(QStringLiteral("date")).toString(), QStringLiteral("2024-04"));
        QCOMPARE(a.rows[a.order.last()].toObject().value(QStringLiteral("date")).toString(), QStringLiteral("2023-04"));
        QVERIFY(a.stats.applicable);
        QCOMPARE(a.stats.column, QStringLiteral("value"));
        QCOMPARE(a.stats.latest, 5.0);
        QCOMPARE(a.stats.latest_label, QStringLiteral("2024-04"));
        QVERIFY(a.stats.has_prev);
        QCOMPARE(a.stats.prev, 4.0);
        QVERIFY(a.stats.has_yoy); // 2023-04 is one year before the latest month
        QCOMPARE(a.stats.yoy, 1.0);
        QCOMPARE(a.stats.min, 1.0);
        QCOMPARE(a.stats.max, 5.0);
        QCOMPARE(a.stats.avg, 3.0);
        QCOMPARE(a.stats.gran, et::Gran::Month);
        QCOMPARE(a.lines.size(), 1);
        QCOMPARE(a.lines.first().points.size(), 5);
        QVERIFY(std::is_sorted(a.lines.first().points.cbegin(), a.lines.first().points.cend(),
                               [](const QPointF& l, const QPointF& r) { return l.x() < r.x(); }));
        QVERIFY(a.approx_bytes > 0);
    }

    void interleaved_dates_have_no_cards() {
        // Two series interleaved by date: no LATEST / CHANGE over unrelated numbers, a line per series.
        QJsonArray rows;
        for (int m = 1; m <= 4; ++m) {
            const QString d = QStringLiteral("2024-%1").arg(m, 2, 10, QLatin1Char('0'));
            rows << QJsonObject{{QStringLiteral("date"), d}, {QStringLiteral("series"), QStringLiteral("A")},
                                {QStringLiteral("value"), double(m)}}
                 << QJsonObject{{QStringLiteral("date"), d}, {QStringLiteral("series"), QStringLiteral("B")},
                                {QStringLiteral("value"), double(10 * m)}};
        }
        const et::Analysis a = et::analyse(tst_eta_table(rows));
        QVERIFY(!a.stats.applicable);
        QCOMPARE(a.lines.size(), 2);
    }

    void projections_stay_out_of_the_cards() {
        QJsonArray rows;
        rows << tst_eta_row(QStringLiteral("2023"), 1.0) << tst_eta_row(QStringLiteral("2024"), 2.0)
             << tst_eta_row(QStringLiteral("2025"), 3.0) << tst_eta_row(QStringLiteral("2026"), 9.0);
        const QJsonObject meta{{QStringLiteral("stats_cutoff"), QStringLiteral("2024-12-31")}};
        const et::Analysis a = et::analyse(tst_eta_table(rows, meta));
        QVERIFY(a.stats.applicable);
        QCOMPARE(a.stats.latest, 2.0);
        QCOMPARE(a.stats.excluded, 2);
        QCOMPARE(a.stats.max, 2.0);
    }

    void keep_row_order_and_bare_values() {
        QJsonArray rows;
        rows << QJsonValue(3.0) << QJsonValue(QStringLiteral("x"));
        const et::Analysis a = et::analyse(tst_eta_table(rows, {}, true));
        QCOMPARE(a.columns, QStringList{QStringLiteral("value")});
        QCOMPARE(a.rows.size(), 2);
        QCOMPARE(a.sort_column, -1);
        QCOMPARE(a.order, (QVector<int>{0, 1}));
    }

    void sort_any_column_blanks_last() {
        QJsonArray rows;
        rows << QJsonObject{{QStringLiteral("k"), 3.0}} << QJsonObject{{QStringLiteral("k"), QString()}}
             << QJsonObject{{QStringLiteral("k"), QStringLiteral("10")}} << QJsonObject{}
             << QJsonObject{{QStringLiteral("k"), -1.0}};
        const QVector<int> asc = et::sorted_order(rows, QStringLiteral("k"), true);
        QCOMPARE(asc.mid(0, 3), (QVector<int>{4, 0, 2}));
        const QVector<int> desc = et::sorted_order(rows, QStringLiteral("k"), false);
        QCOMPARE(desc.mid(0, 3), (QVector<int>{2, 0, 4}));
        // blanks last in both directions, in their original order (stable)
        QCOMPARE(asc.mid(3), (QVector<int>{1, 3}));
        QCOMPARE(desc.mid(3), (QVector<int>{1, 3}));
    }

    void csv_text() {
        QJsonArray rows;
        rows << QJsonObject{{QStringLiteral("a"), QStringLiteral("x,y")}, {QStringLiteral("b"), 1.5}}
             << QJsonObject{{QStringLiteral("a"), QStringLiteral("say \"hi\"")}, {QStringLiteral("b"), 2.0}};
        const QString csv = et::rows_text(rows, {QStringLiteral("a"), QStringLiteral("b")}, {1, 0}, true);
        QCOMPARE(csv, QStringLiteral("a,b\n\"say \"\"hi\"\"\",2\n\"x,y\",1.5\n"));
        const QString tsv = et::rows_text(rows, {QStringLiteral("a")}, {}, false);
        QCOMPARE(tsv, QStringLiteral("a\nx,y\nsay \"hi\"\n"));
    }

    void thinning_keeps_every_extreme() {
        // 50,000 daily points with spikes: thinned to ~2 points per pixel column, the global and every
        // spike's extreme stay, first and last stay, x stays ascending.
        QVector<QPointF> pts;
        pts.reserve(50000);
        const double day = 86400000.0;
        QRandomGenerator rng(7);
        for (int i = 0; i < 50000; ++i)
            pts.append(QPointF(i * day, std::sin(i / 300.0) + rng.generateDouble() * 0.01));
        pts[12345].setY(50.0);  // spike up
        pts[33333].setY(-40.0); // spike down
        const int px = 1000;
        const QVector<QPointF> out = et::decimate(pts, pts.first().x(), pts.last().x(), px);
        QVERIFY(out.size() <= 2 * px + 4);
        QVERIFY(out.size() > px);
        QCOMPARE(out.first(), pts.first());
        QCOMPARE(out.last(), pts.last());
        QVERIFY(out.contains(pts[12345]));
        QVERIFY(out.contains(pts[33333]));
        QVERIFY(std::is_sorted(out.cbegin(), out.cend(), [](const QPointF& l, const QPointF& r) { return l.x() < r.x(); }));
        // Every bucket of 50 consecutive points keeps its own min and max.
        const double width = (pts.last().x() - pts.first().x()) / px;
        for (int b = 0; b < px; b += 97) {
            double lo = 1e300, hi = -1e300;
            for (const QPointF& p : pts) {
                if (static_cast<int>((p.x() - pts.first().x()) / width) == b) {
                    lo = std::min(lo, p.y());
                    hi = std::max(hi, p.y());
                }
            }
            bool has_lo = false, has_hi = false;
            for (const QPointF& p : out) {
                has_lo = has_lo || p.y() == lo;
                has_hi = has_hi || p.y() == hi;
            }
            QVERIFY2(has_lo && has_hi, qPrintable(QStringLiteral("bucket %1").arg(b)));
        }
        // Zoomed into a tenth: that window at full pixel resolution, the rest coarse; extent unchanged.
        const double from = pts[20000].x(), to = pts[25000].x();
        const QVector<QPointF> zoomed = et::decimate(pts, from, to, px);
        int inside = 0;
        for (const QPointF& p : zoomed)
            inside += (p.x() >= from && p.x() <= to) ? 1 : 0;
        QVERIFY(inside > px);
        QCOMPARE(zoomed.first(), pts.first());
        QCOMPARE(zoomed.last(), pts.last());
        QVERIFY(zoomed.contains(pts[12345]));
        QVERIFY(zoomed.contains(pts[33333]));
        // A short series is never touched.
        const QVector<QPointF> small = pts.mid(0, 2 * px);
        QCOMPARE(et::decimate(small, small.first().x(), small.last().x(), px), small);
    }
};

QTEST_GUILESS_MAIN(TstEconTableAnalysis)
#include "tst_econ_table_analysis.moc"
