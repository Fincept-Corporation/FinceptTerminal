// tests/tst_flow_layout.cpp — the pure geometry of ui::FlowLayout (FlowLayoutGeometry.h, QtCore only).
#include "ui/widgets/FlowLayoutGeometry.h"

#include <QtTest/QtTest>

#include <limits>

using namespace fincept::ui::flow_layout;

namespace {

QVector<Item> items_of(std::initializer_list<QSize> sizes) {
    QVector<Item> v;
    for (const QSize& s : sizes)
        v.append(Item{s});
    return v;
}

Params params(int hs = 6, int vs = 4) {
    Params p;
    p.h_spacing = hs;
    p.v_spacing = vs;
    return p;
}

} // namespace

class TestFlowLayout : public QObject {
    Q_OBJECT
  private slots:
    void empty_has_no_height() {
        const Result r = arrange(QRect(0, 0, 200, 0), {}, params());
        QCOMPARE(r.height, 0);
        QCOMPARE(r.lines, 0);
        QVERIFY(r.rects.isEmpty());
    }

    void one_line_when_it_fits() {
        const Result r = arrange(QRect(0, 0, 200, 0), items_of({{50, 20}, {50, 20}, {50, 20}}), params());
        QCOMPARE(r.lines, 1);
        QCOMPARE(r.height, 20);
        QCOMPARE(r.rects[0], QRect(0, 0, 50, 20));
        QCOMPARE(r.rects[1], QRect(56, 0, 50, 20));
        QCOMPARE(r.rects[2], QRect(112, 0, 50, 20));
    }

    void wraps_when_the_next_item_crosses_the_edge() {
        const Result r = arrange(QRect(0, 0, 100, 0), items_of({{50, 20}, {50, 20}, {50, 20}}), params());
        QCOMPARE(r.lines, 3);
        QCOMPARE(r.rects[1], QRect(0, 24, 50, 20));
        QCOMPARE(r.rects[2], QRect(0, 48, 50, 20));
        QCOMPARE(r.height, 68);
    }

    void exact_fit_stays_on_the_line() {
        // 50 + 6 + 50 = 106: the second item ends on the last column.
        const Result r = arrange(QRect(0, 0, 106, 0), items_of({{50, 20}, {50, 20}, {50, 20}}), params());
        QCOMPARE(r.lines, 2);
        QCOMPARE(r.rects[1], QRect(56, 0, 50, 20));
        QCOMPARE(r.rects[2], QRect(0, 24, 50, 20));
    }

    void line_height_is_the_tallest_item() {
        const Result r = arrange(QRect(0, 0, 100, 0), items_of({{50, 20}, {40, 30}, {50, 10}}), params());
        QCOMPARE(r.rects[2].top(), 34); // 30 + 4
        QCOMPARE(r.height, 44);
    }

    void skipped_items_take_no_space() {
        QVector<Item> v = items_of({{50, 20}, {50, 20}, {50, 20}});
        v[1].skip = true;
        const Result r = arrange(QRect(0, 0, 200, 0), v, params());
        QVERIFY(r.rects[1].isNull());
        QCOMPARE(r.rects[2], QRect(56, 0, 50, 20));
        QCOMPARE(one_line_size(v, 6), QSize(106, 20));
    }

    void only_skipped_items_have_no_height() {
        QVector<Item> v = items_of({{50, 20}});
        v[0].skip = true;
        const Result r = arrange(QRect(0, 0, 200, 0), v, params());
        QCOMPARE(r.height, 0);
        QCOMPARE(r.lines, 0);
    }

    void wider_than_the_area_is_clamped_and_alone() {
        const Result r = arrange(QRect(0, 0, 40, 0), items_of({{100, 20}, {30, 20}}), params());
        QCOMPARE(r.rects[0], QRect(0, 0, 40, 20));
        QCOMPARE(r.rects[1], QRect(0, 24, 30, 20));
    }

    void first_item_never_wraps_even_at_zero_width() {
        const Result r = arrange(QRect(0, 0, 0, 0), items_of({{30, 20}, {30, 20}}), params());
        QCOMPARE(r.rects[0].top(), 0);
        QCOMPARE(r.rects[1].top(), 24);
        QCOMPARE(r.lines, 2);
    }

    void area_offset_and_margins() {
        const Result r = arrange(QRect(10, 5, 100, 0), items_of({{50, 20}, {50, 20}}), params());
        QCOMPARE(r.rects[0], QRect(10, 5, 50, 20));
        QCOMPARE(r.rects[1], QRect(10, 29, 50, 20));
        QCOMPARE(r.height, 44); // from area.top()
    }

    void centered_lines() {
        Params p = params();
        p.centered = true;
        const Result r = arrange(QRect(0, 0, 200, 0), items_of({{50, 20}, {50, 20}}), p);
        // used width 106 -> (200 - 106) / 2 = 47
        QCOMPARE(r.rects[0].left(), 47);
        QCOMPARE(r.rects[1].left(), 103);
    }

    void right_to_left_mirrors_each_line() {
        Params p = params();
        p.rtl = true;
        const Result r = arrange(QRect(0, 0, 200, 0), items_of({{50, 20}, {50, 20}}), p);
        QCOMPARE(r.rects[0], QRect(150, 0, 50, 20)); // first item at the right edge
        QCOMPARE(r.rects[1], QRect(94, 0, 50, 20));
        // wrapping: the second line starts at the right too
        const Result w = arrange(QRect(0, 0, 100, 0), items_of({{50, 20}, {50, 20}}), p);
        QCOMPARE(w.rects[0], QRect(50, 0, 50, 20));
        QCOMPARE(w.rects[1], QRect(50, 24, 50, 20));
    }

    void right_to_left_with_an_offset_area() {
        Params p = params();
        p.rtl = true;
        const Result r = arrange(QRect(20, 0, 100, 0), items_of({{30, 20}}), p);
        QCOMPARE(r.rects[0], QRect(90, 0, 30, 20)); // ends on the area's last column (119)
    }

    void vertical_alignment_inside_a_line() {
        QVector<Item> v = items_of({{20, 10}, {20, 10}, {20, 30}});
        v[0].align = Qt::AlignVCenter;
        v[1].align = Qt::AlignBottom;
        const Result r = arrange(QRect(0, 0, 200, 0), v, params());
        QCOMPARE(r.rects[0].top(), 10);
        QCOMPARE(r.rects[1].top(), 20);
        QCOMPARE(r.rects[2].top(), 0);
        // horizontal alignment flags are ignored
        v[0].align = Qt::AlignRight;
        QCOMPARE(arrange(QRect(0, 0, 200, 0), v, params()).rects[0], QRect(0, 0, 20, 10));
    }

    void negative_spacing_counts_as_zero() {
        const Result r = arrange(QRect(0, 0, 200, 0), items_of({{50, 20}, {50, 20}}), params(-5, -5));
        QCOMPARE(r.rects[1].left(), 50);
    }

    void one_line_size_sums_widths_and_spacing() {
        QCOMPARE(one_line_size(items_of({{50, 20}, {30, 25}, {10, 5}}), 6), QSize(102, 25));
        QCOMPARE(one_line_size({}, 6), QSize(0, 0));
    }

    void default_spacing_follows_the_font() {
        QCOMPARE(default_h_spacing(7), 6);  // 14 px UI font
        QCOMPARE(default_v_spacing(17), 4);
        QCOMPARE(default_h_spacing(4), 4);  // 9 px floor
        QCOMPARE(default_v_spacing(11), 3);
        QVERIFY(default_h_spacing(13) > default_h_spacing(7)); // 24 px grows
        QVERIFY(default_v_spacing(30) > default_v_spacing(17));
    }

    void height_for_width_is_monotonic() {
        const QVector<Item> v = items_of({{40, 18}, {70, 18}, {25, 18}, {90, 18}, {60, 18}, {30, 18}});
        int prev = std::numeric_limits<int>::max();
        for (int w = 20; w <= 400; w += 7) {
            const int h = arrange(QRect(0, 0, w, 0), v, params()).height;
            QVERIFY2(h <= prev, qPrintable(QStringLiteral("width %1").arg(w)));
            prev = h;
        }
        QCOMPARE(prev, 18); // everything on one line at 400
    }
};

QTEST_APPLESS_MAIN(TestFlowLayout)
#include "tst_flow_layout.moc"
