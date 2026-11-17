#include "services/markets/MarketPerformance.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace fincept::services {

namespace {

// Unity builds batch this file with others: every helper carries the mperf_ prefix.

struct MperfBar {
    QDate date;
    double close = 0;
    double high = 0;
    double low = 0;
};

// A daily bar is stamped at the session's local midnight. Shifting by 12 h
// before taking the UTC calendar day lands inside the session's own date for
// every UTC offset in (-12 h, +12 h] — Tokyo's 15:00Z-the-day-before included.
QDate mperf_session_date(qint64 ts) {
    constexpr qint64 kDay = 86'400;
    const qint64 shifted = ts + kDay / 2;
    qint64 days = shifted / kDay;
    if (shifted % kDay < 0)
        --days; // floor division for pre-1970 stamps
    return QDate(1970, 1, 1).addDays(days);
}

// Close of the last bar dated on/before `anchor`. When the series starts after
// the anchor, its first bar if that is within `tolerance_days`; else NaN.
double mperf_base_close(const QVector<MperfBar>& bars, const QDate& anchor, int tolerance_days) {
    auto it = std::upper_bound(bars.cbegin(), bars.cend(), anchor,
                               [](const QDate& d, const MperfBar& b) { return d < b.date; });
    if (it != bars.cbegin())
        return std::prev(it)->close;
    if (!bars.isEmpty() && tolerance_days > 0 && bars.first().date <= anchor.addDays(tolerance_days))
        return bars.first().close;
    return kMarketDataAbsent;
}

double mperf_pct(double last, double base) {
    if (!std::isfinite(last) || !std::isfinite(base) || base <= 0)
        return kMarketDataAbsent;
    return (last / base - 1.0) * 100.0;
}

} // namespace

PerformanceStats compute_performance(const QVector<HistoryPoint>& daily, double live_price, int periods_per_year) {
    PerformanceStats s;

    QVector<MperfBar> bars;
    bars.reserve(daily.size());
    for (const HistoryPoint& p : daily) {
        if (p.timestamp <= 0 || !std::isfinite(p.close) || p.close <= 0)
            continue;
        MperfBar b;
        b.date = mperf_session_date(p.timestamp);
        b.close = p.close;
        b.high = (std::isfinite(p.high) && p.high > 0) ? p.high : p.close;
        b.low = (std::isfinite(p.low) && p.low > 0) ? p.low : p.close;
        bars.append(b);
    }
    if (bars.isEmpty())
        return s;
    // Sorted by date; for duplicate dates (a re-stamped partial bar) the later
    // one in the input wins.
    std::stable_sort(bars.begin(), bars.end(), [](const MperfBar& a, const MperfBar& b) { return a.date < b.date; });
    QVector<MperfBar> uniq;
    uniq.reserve(bars.size());
    for (const MperfBar& b : bars) {
        if (!uniq.isEmpty() && uniq.last().date == b.date)
            uniq.last() = b;
        else
            uniq.append(b);
    }
    bars = std::move(uniq);

    s.as_of = bars.last().date;
    s.last = (std::isfinite(live_price) && live_price > 0) ? live_price : bars.last().close;

    constexpr int kBoundaryToleranceDays = 4;
    s.change_1w_pct = mperf_pct(s.last, mperf_base_close(bars, s.as_of.addDays(-7), 0));
    s.change_1m_pct = mperf_pct(s.last, mperf_base_close(bars, s.as_of.addMonths(-1), 0));
    s.change_3m_pct = mperf_pct(s.last, mperf_base_close(bars, s.as_of.addMonths(-3), 0));
    s.change_6m_pct = mperf_pct(s.last, mperf_base_close(bars, s.as_of.addMonths(-6), 0));
    s.change_ytd_pct =
        mperf_pct(s.last, mperf_base_close(bars, QDate(s.as_of.year() - 1, 12, 31), kBoundaryToleranceDays));
    s.change_1y_pct = mperf_pct(s.last, mperf_base_close(bars, s.as_of.addYears(-1), kBoundaryToleranceDays));

    // 52-week range: bars dated after the one-year anchor, plus the live price.
    const QDate year_ago = s.as_of.addYears(-1);
    double hi = -1;
    double lo = -1;
    for (const MperfBar& b : bars) {
        if (b.date <= year_ago)
            continue;
        hi = (hi < 0) ? b.high : std::max(hi, b.high);
        lo = (lo < 0) ? b.low : std::min(lo, b.low);
    }
    if (std::isfinite(live_price) && live_price > 0 && hi > 0) {
        hi = std::max(hi, live_price);
        lo = std::min(lo, live_price);
    }
    if (hi > 0 && lo > 0) {
        s.high_52w = hi;
        s.low_52w = lo;
        s.from_52w_high_pct = mperf_pct(s.last, hi);
        s.from_52w_low_pct = mperf_pct(s.last, lo);
    }

    // 20-day realised volatility from the last 21 closes.
    constexpr int kVolReturns = 20;
    if (bars.size() > kVolReturns && periods_per_year > 0) {
        double rets[kVolReturns];
        double mean = 0;
        const int first = static_cast<int>(bars.size()) - (kVolReturns + 1);
        for (int i = 0; i < kVolReturns; ++i) {
            rets[i] = std::log(bars.at(first + i + 1).close / bars.at(first + i).close);
            mean += rets[i];
        }
        mean /= kVolReturns;
        double ss = 0;
        for (double r : rets)
            ss += (r - mean) * (r - mean);
        const double sd = std::sqrt(ss / (kVolReturns - 1));
        s.volatility_20d_pct = sd * std::sqrt(static_cast<double>(periods_per_year)) * 100.0;
    }
    return s;
}

} // namespace fincept::services
