// tst_ts_chart_panes — the pure pane rules behind ui::TimeSeriesChart (src/ui/charts/TsChartPanes.h):
// vertical slots of the shown panes (ratio split, remainder to the last shown pane, gaps only between
// shown panes, exactly the original rule when every pane is shown), which pane may be hidden or removed
// (never the last shown one), where a series naming a removed / unknown pane goes, and the line-width
// clamp. QtCore only.

#include "ui/charts/TsChartPanes.h"

#include <QtTest>

#include <cmath>
#include <limits>

using namespace fincept::ui::ts_panes;

namespace {

PaneState pane(double ratio, bool visible = true, bool removed = false) {
    PaneState p;
    p.ratio = ratio;
    p.visible = visible;
    p.removed = removed;
    return p;
}

/// The chart's layout rule before panes could be hidden (TsChartPaint.cpp, 2026-11-16), verbatim.
QVector<QPair<int, int>> original_rule(const QVector<double>& ratios, int top, int bottom, int gap) {
    const int np = static_cast<int>(ratios.size());
    const int avail = std::max(np, bottom - top - (np - 1) * gap);
    double total_ratio = 0.0;
    for (double r : ratios)
        total_ratio += r;
    QVector<int> heights(np);
    int used_h = 0;
    for (int i = 0; i < np; ++i) {
        heights[i] = (i == np - 1) ? avail - used_h : static_cast<int>(avail * ratios[i] / std::max(1e-9, total_ratio));
        used_h += heights[i];
    }
    QVector<QPair<int, int>> out;
    int y = top;
    for (int i = 0; i < np; ++i) {
        out.append({y, heights[i]});
        y += heights[i] + gap;
    }
    return out;
}

} // namespace

class TstTsChartPanes : public QObject {
    Q_OBJECT
  private slots:
    void all_shown_matches_original_rule_data() {
        QTest::addColumn<QVector<double>>("ratios");
        QTest::addColumn<int>("column");
        QTest::addColumn<int>("gap");
        QTest::newRow("main only") << QVector<double>{1.0} << 400 << 6;
        QTest::newRow("main + 0.35") << QVector<double>{1.0, 0.35} << 400 << 6;
        QTest::newRow("main + 2 panes") << QVector<double>{1.0, 0.35, 0.25} << 517 << 7;
        QTest::newRow("base 24 gap") << QVector<double>{1.0, 0.5, 0.5, 0.2} << 300 << 13;
        QTest::newRow("tiny column") << QVector<double>{1.0, 0.35, 0.35} << 5 << 6;
    }
    void all_shown_matches_original_rule() {
        QFETCH(QVector<double>, ratios);
        QFETCH(int, column);
        QFETCH(int, gap);
        QVector<PaneState> panes;
        for (double r : ratios)
            panes.append(pane(r));
        const int top = 31;
        const auto expected = original_rule(ratios, top, top + column, gap);
        const auto slots_out = layout_slots(panes, column, gap);
        QCOMPARE(slots_out.size(), ratios.size());
        for (int i = 0; i < ratios.size(); ++i) {
            QVERIFY(slots_out[i].shown);
            QCOMPARE(top + slots_out[i].top, expected[i].first);
            QCOMPARE(slots_out[i].height, expected[i].second);
        }
    }

    void hidden_pane_takes_no_room() {
        // main 1.0, volume 0.35 (hidden), drawdown 0.25 — column 400, gap 6.
        // Shown: main + drawdown -> avail = 400 - 6 = 394; total ratio 1.25;
        // main = int(394 * 1.0 / 1.25) = 315; drawdown = 394 - 315 = 79 at 315 + 6 = 321.
        const QVector<PaneState> panes{pane(1.0), pane(0.35, false), pane(0.25)};
        const auto s = layout_slots(panes, 400, 6);
        QVERIFY(s[0].shown);
        QCOMPARE(s[0].top, 0);
        QCOMPARE(s[0].height, 315);
        QVERIFY(!s[1].shown);
        QCOMPARE(s[1].height, 0);
        QVERIFY(s[2].shown);
        QCOMPARE(s[2].top, 321);
        QCOMPARE(s[2].height, 79);
        QCOMPARE(s[2].top + s[2].height, 400); // fills the column exactly
    }

    void removed_and_hidden_main_pane_reflow() {
        // Pane 0 removed, pane 1 hidden: pane 2 alone fills the column from the top.
        const QVector<PaneState> panes{pane(1.0, false, true), pane(0.35, false), pane(0.25)};
        const auto s = layout_slots(panes, 250, 6);
        QVERIFY(!s[0].shown);
        QVERIFY(!s[1].shown);
        QVERIFY(s[2].shown);
        QCOMPARE(s[2].top, 0);
        QCOMPARE(s[2].height, 250);
        QCOMPARE(first_shown(panes), 2);
        QCOMPARE(first_alive(panes), 1);
    }

    void shown_heights_and_gaps_fill_column() {
        const QVector<PaneState> panes{pane(1.0), pane(0.3), pane(0.7, false), pane(0.2), pane(0.5, true, true)};
        for (int column : {40, 99, 401, 1033}) {
            const auto s = layout_slots(panes, column, 5);
            int bottom = 0;
            int prev_end = -5;
            for (int i = 0; i < s.size(); ++i) {
                if (!s[i].shown)
                    continue;
                QCOMPARE(s[i].top, prev_end + 5); // one gap between neighbouring shown panes
                QVERIFY(s[i].height >= 0);
                prev_end = s[i].top + s[i].height;
                bottom = prev_end;
            }
            QCOMPARE(bottom, column);
        }
    }

    void no_shown_pane_gives_empty_slots() {
        const QVector<PaneState> panes{pane(1.0, false), pane(0.35, false, true)};
        const auto s = layout_slots(panes, 300, 6);
        QCOMPARE(s.size(), 2);
        QVERIFY(!s[0].shown && !s[1].shown);
        QCOMPARE(first_shown(panes), -1);
    }

    void hide_rules() {
        QVector<PaneState> panes{pane(1.0), pane(0.35)};
        QVERIFY(can_hide(panes, 1));
        QVERIFY(can_hide(panes, 0)); // the main pane may be hidden while another is shown
        panes[1].visible = false;
        QVERIFY(!can_hide(panes, 0)); // last shown pane: refused
        QVERIFY(can_hide(panes, 1));  // already hidden: no-op, allowed
        QVERIFY(!can_hide(panes, 2)); // no such pane
        QVERIFY(!can_hide(panes, -1));
        panes[1].removed = true;
        QVERIFY(!can_hide(panes, 1)); // removed
    }

    void remove_rules() {
        QVector<PaneState> panes{pane(1.0)};
        QVERIFY(!can_remove(panes, 0)); // the only pane
        panes.append(pane(0.35));
        QVERIFY(can_remove(panes, 1));
        QVERIFY(can_remove(panes, 0));
        panes[1].visible = false;
        QVERIFY(!can_remove(panes, 0)); // the last SHOWN pane while the other is hidden
        QVERIFY(can_remove(panes, 1));  // a hidden pane may go while another is shown
        panes[1].removed = true;
        QVERIFY(!can_remove(panes, 1)); // already removed
        QVERIFY(!can_remove(panes, 0)); // now the last pane
        QVERIFY(!can_remove(panes, 7));
    }

    void resolve_pane_rules() {
        QVector<PaneState> panes{pane(1.0), pane(0.35), pane(0.25, false)};
        QCOMPARE(resolve_pane(panes, 1), 1);
        QCOMPARE(resolve_pane(panes, 2), 2);  // hidden pane: still a valid target
        QCOMPARE(resolve_pane(panes, 9), 0);  // out of range: the main pane (original rule)
        QCOMPARE(resolve_pane(panes, -1), 0);
        panes[0].removed = true;
        QCOMPARE(resolve_pane(panes, 0), 1);  // removed: first live pane
        QCOMPARE(resolve_pane(panes, 9), 1);
        QVERIFY(!is_alive(panes, 0));
        QVERIFY(is_alive(panes, 2));
        QVERIFY(!is_shown(panes, 2));
        QCOMPARE(shown_count(panes), 1);
    }

    void line_width_clamp() {
        QCOMPARE(line_width_px(1.5).value(), 1.5);
        QCOMPARE(line_width_px(0.1).value(), kMinLineWidth);
        QCOMPARE(line_width_px(40.0).value(), kMaxLineWidth);
        QVERIFY(!line_width_px(0.0).has_value());
        QVERIFY(!line_width_px(-2.0).has_value());
        QVERIFY(!line_width_px(std::numeric_limits<double>::quiet_NaN()).has_value());
        QVERIFY(!line_width_px(std::numeric_limits<double>::infinity()).has_value());
    }
};

QTEST_GUILESS_MAIN(TstTsChartPanes)
#include "tst_ts_chart_panes.moc"
