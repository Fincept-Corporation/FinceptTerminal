#pragma once
// ChartRanges.h — pure range rules of the painted charts (QtCore only, header-inline,
// unit-tested by tests/tst_chart_ranges.cpp):
//   * Extent       — running min / max over finite values (auto-range: FanChart fan + lines),
//   * clamp_view   — CandleChart's view window clamp (zoom / pan / reset / set_visible_range),
//   * view_bars / band_bars — a [from, to] time range → the bars it covers, by the
//     "bar containing" rule trade markers use (the last bar opening at or before a time),
//   * window_for   — the view window showing a span, centred when shorter than the zoom floor.

#include <QVector>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::ui::chart_ranges {

/// Running min / max over finite values. valid() is false until one finite value was taken.
struct Extent {
    double lo = std::numeric_limits<double>::infinity();
    double hi = -std::numeric_limits<double>::infinity();

    bool valid() const { return lo <= hi; }
    void take(double v) {
        if (std::isfinite(v)) {
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
    }
    /// The first `count` values (all of them when count < 0 or larger than the vector).
    void take(const QVector<double>& values, qsizetype count = -1) {
        const qsizetype n = count < 0 ? values.size() : std::min(count, values.size());
        for (qsizetype i = 0; i < n; ++i)
            take(values[i]);
    }
};

/// Inclusive bar-index span; empty when first > last.
struct IndexSpan {
    int first = 0;
    int last = -1;
    bool empty() const { return first > last; }
    int count() const { return empty() ? 0 : last - first + 1; }
};

/// Index of the bar containing time `t` among `n` bars whose open times `time_at(i)` ascend:
/// the last bar opening at or before `t`; -1 when `t` is before the first bar (or n <= 0).
template <class TimeAt>
int bar_containing(int n, double t, TimeAt time_at) {
    int lo = 0;
    int hi = n; // first index whose time is > t
    while (lo < hi) {
        const int mid = lo + (hi - lo) / 2;
        if (time_at(mid) <= t)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo - 1;
}

/// Bars to SHOW for the time range [from, to] (swapped when from > to). Each end maps to the bar
/// containing it; a range before the first bar clamps to bar 0, one after the last bar to the last
/// bar. Empty only when n <= 0 or an end is not finite.
template <class TimeAt>
IndexSpan view_bars(int n, double from, double to, TimeAt time_at) {
    if (n <= 0 || !std::isfinite(from) || !std::isfinite(to))
        return {};
    if (from > to)
        std::swap(from, to);
    const int first = std::clamp(bar_containing(n, from, time_at), 0, n - 1);
    const int last = std::clamp(bar_containing(n, to, time_at), 0, n - 1);
    return {first, last};
}

/// Bars a BAND over [from, to] (swapped when from > to) shades: like view_bars, except that a band
/// ending before the first bar shades nothing (empty). A band starting before the first bar starts
/// at bar 0; one starting after the last bar's open shades the last bar.
template <class TimeAt>
IndexSpan band_bars(int n, double from, double to, TimeAt time_at) {
    if (n <= 0 || !std::isfinite(from) || !std::isfinite(to))
        return {};
    if (from > to)
        std::swap(from, to);
    const int last = bar_containing(n, to, time_at);
    if (last < 0)
        return {};
    const int first = std::max(0, bar_containing(n, from, time_at));
    return {first, last};
}

/// A view window: fractional bar index at the left edge + slots across.
struct ViewWindow {
    double start = 0.0;
    double count = 0.0;
};

/// The window showing `span` exactly; a span shorter than `min_slots` is centred in a window of
/// `min_slots` (so a one-bar trade sits in the middle, not at the edge).
inline ViewWindow window_for(const IndexSpan& span, double min_slots) {
    if (span.empty())
        return {};
    double count = static_cast<double>(span.last - span.first + 1);
    double start = static_cast<double>(span.first);
    if (count < min_slots) {
        start -= (min_slots - count) / 2.0;
        count = min_slots;
    }
    return {start, count};
}

/// CandleChart's view clamp over `n` bars. `count` is kept within [min_slots, n + right_margin]
/// (min_slots wins for tiny series). Not following: `start` keeps at least min(5, n) bars on screen,
/// and a window reaching the live edge (start + count >= n + right_margin - 0.5) switches to
/// following. Following: `start` is returned unchanged (the chart derives it from the live edge).
struct ViewState {
    double start = 0.0;
    double count = 0.0;
    bool follow = true;
};
inline ViewState clamp_view(double start, double count, bool follow, double n, double min_slots,
                            double right_margin) {
    ViewState v{start, count, follow};
    const double max_count = std::max(min_slots, n + right_margin);
    v.count = std::clamp(count, std::min(min_slots, max_count), max_count);
    if (!v.follow) {
        const double visible_min = std::min(5.0, std::max(1.0, n));
        const double hi = std::max(0.0, n - visible_min);
        const double lo = std::min(hi, -v.count + visible_min);
        v.start = std::clamp(start, lo, hi);
        if (v.start + v.count >= n + right_margin - 0.5)
            v.follow = true;
    }
    return v;
}

} // namespace fincept::ui::chart_ranges
