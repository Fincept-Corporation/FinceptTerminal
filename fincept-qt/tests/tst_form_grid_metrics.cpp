// tests/tst_form_grid_metrics.cpp — the arithmetic of a compact input form beside its results
// (src/ui/widgets/FormGridMetrics.h, header-only, std only): field / span / form widths and the
// responsive breakpoint the Derivatives pricer uses.
#include "ui/widgets/FormGridMetrics.h"

#include <QtTest/QtTest>

using namespace fincept::ui::form_grid;

// Compile-time use: the functions are constexpr.
static_assert(field_px(8, 15, 38) == 158);
static_assert(span_px(158, 3, 8) == 490);
static_assert(side_by_side(1000, 500, 400, 12));

class TestFormGridMetrics : public QObject {
    Q_OBJECT
  private slots:
    void field_width_scales_with_the_font() {
        // A 15-character field at the default input font (~7 px per digit) and at a large one.
        QCOMPARE(field_px(7, 15, 38), 143);
        QCOMPARE(field_px(13, 15, 38), 233);
        // At least one character; negative inputs are treated as zero.
        QCOMPARE(field_px(7, 0, 38), 45);
        QCOMPARE(field_px(-3, 10, 38), 38);
        QCOMPARE(field_px(7, 10, -5), 70);
    }

    void spans_include_the_gaps() {
        QCOMPARE(span_px(140, 1, 8), 140);
        QCOMPARE(span_px(140, 2, 8), 288);
        QCOMPARE(span_px(140, 3, 8), 436);
        QCOMPARE(span_px(140, 0, 8), 140); // a span is at least one column
        QCOMPARE(span_px(140, 3, -2), 420);
    }

    void form_adds_padding_on_both_sides() {
        QCOMPARE(form_px(3, 140, 8, 12), 460);
        QCOMPARE(form_px(1, 140, 8, 0), 140);
        QCOMPARE(form_px(2, 140, 8, -4), 288);
    }

    void columns_for_fit_and_bounds() {
        // Exactly three fit: 3 * 100 + 2 * 10 = 320.
        QCOMPARE(columns_for(320, 100, 10, 0), 3);
        QCOMPARE(columns_for(319, 100, 10, 0), 2);
        // Capped by the maximum.
        QCOMPARE(columns_for(2000, 100, 10, 4), 4);
        // Never fewer than one, even when nothing fits or the area is empty.
        QCOMPARE(columns_for(50, 100, 10, 4), 1);
        QCOMPARE(columns_for(0, 100, 10, 4), 1);
        QCOMPARE(columns_for(-20, 100, 10, 0), 1);
        // Degenerate item width: the maximum (or one without a maximum).
        QCOMPARE(columns_for(500, 0, 10, 3), 3);
        QCOMPARE(columns_for(500, 0, 10, 0), 1);
    }

    void side_by_side_breakpoint() {
        // Lead 480 + gap 12 + results minimum 432 = 924.
        QVERIFY(side_by_side(924, 480, 432, 12));
        QVERIFY(!side_by_side(923, 480, 432, 12));
        // A 1280 px window at the default font fits; a large font's wider column does not.
        QVERIFY(side_by_side(1280 - 24, 480, 432, 12));
        QVERIFY(!side_by_side(1280 - 24, 750, 780, 12));
        // Negative parts count as zero.
        QVERIFY(side_by_side(100, -50, 100, -10));
    }
};

QTEST_GUILESS_MAIN(TestFormGridMetrics)
#include "tst_form_grid_metrics.moc"
