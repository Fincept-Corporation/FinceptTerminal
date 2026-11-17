// tst_chart_ranges — the pure range rules of the painted charts (src/ui/charts/ChartRanges.h):
// the auto-range extent (FanChart: fan + extra lines), the "bar containing a time" lookup, the
// bars a visible range / a band covers (CandleChart::set_visible_range / add_band), the centred
// window for short spans and CandleChart's view clamp (zoom floor, live edge, keep-on-screen).
// Expected values are worked by hand in the comments.

#include "ui/charts/ChartRanges.h"

#include <QtTest>

#include <cmath>
#include <limits>

using namespace fincept::ui::chart_ranges;

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// Bars opening at 100, 200, 300, 400, 500 (ms).
const QVector<double> kTimes = {100, 200, 300, 400, 500};
auto time_at() {
    return [](int i) { return kTimes[i]; };
}
constexpr int kN = 5;

} // namespace

class TstChartRanges : public QObject {
    Q_OBJECT
  private slots:
    // ── Extent ──────────────────────────────────────────────────────────────
    void extent_starts_invalid() {
        Extent e;
        QVERIFY(!e.valid());
        e.take(kNaN);
        e.take(kInf);
        QVERIFY(!e.valid()); // non-finite values never count
    }

    void extent_takes_finite_values() {
        Extent e;
        e.take(QVector<double>{3, kNaN, -2, 7});
        QVERIFY(e.valid());
        QCOMPARE(e.lo, -2.0);
        QCOMPARE(e.hi, 7.0);
    }

    void extent_limits_to_count() {
        // A line longer than x: only the first x.size() values are drawn, so only they count.
        Extent e;
        e.take(QVector<double>{1, 2, 100}, 2);
        QCOMPARE(e.lo, 1.0);
        QCOMPARE(e.hi, 2.0);
        Extent all;
        all.take(QVector<double>{1, 2, 100}, 99); // count beyond the vector = all of it
        QCOMPARE(all.hi, 100.0);
        Extent neg;
        neg.take(QVector<double>{5, 6}, -1); // negative = all
        QCOMPARE(neg.hi, 6.0);
    }

    void extent_line_widens_fan_range() {
        // Fan 90..110, realised path dips to 80 and peaks at 120: the range must include both.
        Extent e;
        e.take(QVector<double>{90, 95, 100});
        e.take(QVector<double>{100, 105, 110});
        e.take(QVector<double>{100, 80, 120});
        QCOMPARE(e.lo, 80.0);
        QCOMPARE(e.hi, 120.0);
    }

    // ── bar_containing ──────────────────────────────────────────────────────
    void bar_containing_rules() {
        QCOMPARE(bar_containing(kN, 99, time_at()), -1);  // before the first bar
        QCOMPARE(bar_containing(kN, 100, time_at()), 0);  // exactly on an open
        QCOMPARE(bar_containing(kN, 250, time_at()), 1);  // inside bar 1 (200..300)
        QCOMPARE(bar_containing(kN, 500, time_at()), 4);
        QCOMPARE(bar_containing(kN, 9999, time_at()), 4); // after the last open: the last bar
        QCOMPARE(bar_containing(0, 250, time_at()), -1);  // no bars
    }

    // ── view_bars (set_visible_range) ───────────────────────────────────────
    void view_bars_inside() {
        const IndexSpan s = view_bars(kN, 210, 420, time_at()); // bars 1 (200) .. 3 (400)
        QCOMPARE(s.first, 1);
        QCOMPARE(s.last, 3);
        QCOMPARE(s.count(), 3);
    }

    void view_bars_swaps_reversed_ends() {
        const IndexSpan s = view_bars(kN, 420, 210, time_at());
        QCOMPARE(s.first, 1);
        QCOMPARE(s.last, 3);
    }

    void view_bars_clamps_outside_bars() { // (a slot name ending in "_data" would be a QtTest data function)
        IndexSpan s = view_bars(kN, 0, 50, time_at()); // wholly before: the first bar
        QCOMPARE(s.first, 0);
        QCOMPARE(s.last, 0);
        s = view_bars(kN, 600, 900, time_at()); // wholly after: the last bar
        QCOMPARE(s.first, 4);
        QCOMPARE(s.last, 4);
        s = view_bars(kN, -1e18, 1e18, time_at()); // everything
        QCOMPARE(s.first, 0);
        QCOMPARE(s.last, 4);
    }

    void view_bars_inside_one_bar() {
        // Both ends inside bar 2 (300..400): one bar, never an inverted span.
        const IndexSpan s = view_bars(kN, 310, 390, time_at());
        QCOMPARE(s.first, 2);
        QCOMPARE(s.last, 2);
    }

    void view_bars_rejects_bad_input() {
        QVERIFY(view_bars(kN, kNaN, 300, time_at()).empty());
        QVERIFY(view_bars(kN, 100, kInf, time_at()).empty());
        QVERIFY(view_bars(0, 100, 300, time_at()).empty());
    }

    // ── band_bars (add_band) ────────────────────────────────────────────────
    void band_bars_inside_and_reversed() {
        IndexSpan s = band_bars(kN, 250, 450, time_at()); // bar 1 .. bar 3
        QCOMPARE(s.first, 1);
        QCOMPARE(s.last, 3);
        s = band_bars(kN, 450, 250, time_at());
        QCOMPARE(s.first, 1);
        QCOMPARE(s.last, 3);
    }

    void band_bars_before_data_is_empty() {
        QVERIFY(band_bars(kN, 0, 99, time_at()).empty()); // ends before the first bar: nothing shaded
    }

    void band_bars_clamps_partial_overlap() {
        IndexSpan s = band_bars(kN, 0, 210, time_at()); // starts before the data: from bar 0
        QCOMPARE(s.first, 0);
        QCOMPARE(s.last, 1);
        s = band_bars(kN, 450, 9999, time_at()); // runs past the data: up to the last bar
        QCOMPARE(s.first, 3);
        QCOMPARE(s.last, 4);
        s = band_bars(kN, 600, 700, time_at()); // after the last open: the last bar (as markers)
        QCOMPARE(s.first, 4);
        QCOMPARE(s.last, 4);
    }

    void band_bars_rejects_bad_input() {
        QVERIFY(band_bars(kN, kNaN, 300, time_at()).empty());
        QVERIFY(band_bars(0, 100, 300, time_at()).empty());
    }

    // ── window_for ──────────────────────────────────────────────────────────
    void window_for_long_span_is_exact() {
        const ViewWindow w = window_for({20, 59}, 10); // 40 bars
        QCOMPARE(w.start, 20.0);
        QCOMPARE(w.count, 40.0);
    }

    void window_for_short_span_is_centred() {
        // One bar at index 50 in a 10-slot floor: start 50 - 9/2 = 45.5, bar centre 50.5 = window centre.
        const ViewWindow w = window_for({50, 50}, 10);
        QCOMPARE(w.count, 10.0);
        QCOMPARE(w.start, 45.5);
        QCOMPARE(w.start + w.count / 2.0, 50.5);
    }

    void window_for_empty_span() {
        const ViewWindow w = window_for({}, 10);
        QCOMPARE(w.count, 0.0);
    }

    // ── clamp_view (CandleChart::set_view) ──────────────────────────────────
    void clamp_view_count_bounds() {
        // 200 bars, floor 10, margin 3: count within [10, 203].
        ViewState v = clamp_view(0, 2, false, 200, 10, 3);
        QCOMPARE(v.count, 10.0);
        v = clamp_view(0, 1000, true, 200, 10, 3);
        QCOMPARE(v.count, 203.0);
        // Tiny series (4 bars): max(10, 4 + 3) = 10 — the floor wins.
        v = clamp_view(0, 50, true, 4, 10, 3);
        QCOMPARE(v.count, 10.0);
    }

    void clamp_view_keeps_bars_on_screen() {
        // 200 bars, 40 across: start within [-40 + 5, 200 - 5] = [-35, 195].
        ViewState v = clamp_view(-100, 40, false, 200, 10, 3);
        QCOMPARE(v.start, -35.0);
        QVERIFY(!v.follow);
        v = clamp_view(50, 40, false, 200, 10, 3);
        QCOMPARE(v.start, 50.0);
        QVERIFY(!v.follow);
    }

    void clamp_view_reaching_live_edge_follows() {
        // start + count >= 200 + 3 - 0.5 = 202.5 -> follow again.
        ViewState v = clamp_view(163, 40, false, 200, 10, 3);
        QVERIFY(v.follow);
        v = clamp_view(160, 40, false, 200, 10, 3); // 200 < 202.5: a window ending on the last bar stays put
        QVERIFY(!v.follow);
        QCOMPARE(v.start, 160.0);
    }

    void clamp_view_following_keeps_start() {
        const ViewState v = clamp_view(12.5, 40, true, 200, 10, 3);
        QVERIFY(v.follow);
        QCOMPARE(v.start, 12.5); // unchanged: the chart derives it from the live edge
    }

    void visible_range_end_to_end() {
        // 300 bars opening every 60 s; a trade from bar 120 to bar 126 (+ nothing): 7 bars < 10 ->
        // centred window of 10 from 118.5; clamp keeps it (inside [-5, 295]) and does not follow.
        QVector<double> t(300);
        for (int i = 0; i < 300; ++i)
            t[i] = 1.7e12 + i * 60000.0;
        const auto at = [&t](int i) { return t[i]; };
        const IndexSpan s = view_bars(300, t[120] + 1000, t[126] + 30000, at);
        QCOMPARE(s.first, 120);
        QCOMPARE(s.last, 126);
        const ViewWindow w = window_for(s, 10);
        QCOMPARE(w.start, 118.5);
        QCOMPARE(w.count, 10.0);
        const ViewState v = clamp_view(w.start, w.count, false, 300, 10, 3);
        QCOMPARE(v.start, 118.5);
        QCOMPARE(v.count, 10.0);
        QVERIFY(!v.follow);
    }
};

QTEST_APPLESS_MAIN(TstChartRanges)
#include "tst_chart_ranges.moc"
