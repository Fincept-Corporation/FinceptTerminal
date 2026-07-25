#include "ui/charts/TimeAxisNavigator.h"

#include <algorithm>

namespace fincept::ui {

void TimeAxisNavigator::set_extent(long long first, long long last, long long slot) {
    first_ = first;
    last_ = std::max(first, last);
    slot_ = slot > 0 ? slot : 1;
    if (active_)
        clamp();
}

void TimeAxisNavigator::seed(long long min_v, long long max_v) {
    if (active_ || max_v <= min_v)
        return;
    min_ = min_v;
    max_ = max_v;
}

bool TimeAxisNavigator::zoom(double factor, long long anchor) {
    if (max_ <= min_ || factor <= 0.0)
        return false;

    const long long span = max_ - min_;
    const long long min_span = MIN_SLOTS * slot_;
    // Zooming out stops one screen-width past the data on each side — beyond
    // that the candles would shrink into an empty plot.
    const long long max_span = (last_ - first_) + 2LL * EDGE_SLOTS * slot_;

    long long new_span = static_cast<long long>(static_cast<double>(span) * factor);
    new_span = std::clamp(new_span, min_span, std::max(min_span, max_span));

    // Keep `anchor` under the cursor: preserve its relative position in the window.
    double rel = static_cast<double>(anchor - min_) / static_cast<double>(span);
    rel = std::clamp(rel, 0.0, 1.0);

    min_ = anchor - static_cast<long long>(rel * static_cast<double>(new_span));
    max_ = min_ + new_span;
    active_ = true;
    clamp();
    return true;
}

bool TimeAxisNavigator::pan(long long delta) {
    if (max_ <= min_ || delta == 0)
        return false;
    min_ += delta;
    max_ += delta;
    active_ = true;
    clamp();
    return true;
}

void TimeAxisNavigator::clamp() {
    const long long span = max_ - min_;
    const long long lo = first_ - EDGE_SLOTS * slot_;
    const long long hi = last_ + EDGE_SLOTS * slot_;

    if (span >= hi - lo) {
        // Window is wider than everything we allow scrolling over — centre it
        // on the data instead of letting it drift.
        const long long mid = first_ + (last_ - first_) / 2;
        min_ = mid - span / 2;
        max_ = min_ + span;
        return;
    }
    if (min_ < lo) {
        min_ = lo;
        max_ = lo + span;
    } else if (max_ > hi) {
        max_ = hi;
        min_ = hi - span;
    }
}

} // namespace fincept::ui
