#pragma once
// Multi-horizon performance from a daily price series — one implementation for
// watchlist, dashboard and portfolio. Pure (Qt Core only): no fetch, no hub.
//
// Feed it the shared daily history topic so every screen reuses one fetch per
// symbol (30 min policy TTL) instead of adding Python calls:
//
//   const QString t = MarketDataService::performance_history_topic(sym);
//   // == history_topic(sym, kPerformanceHistoryPeriod, kPerformanceHistoryInterval)
//   //  = "market:history:<SYM>:1y:1d"  (value: QVector<HistoryPoint>)
//   DataHub::instance().subscribe(this, t, [=](const QVariant& v) {
//       const auto stats = compute_performance(v.value<QVector<HistoryPoint>>(), live_price);
//   });

#include "services/markets/MarketDataTypes.h"

#include <QDate>
#include <QVector>

namespace fincept::services {

/// Period / interval of the daily series compute_performance() is designed for.
inline constexpr char kPerformanceHistoryPeriod[] = "1y";
inline constexpr char kPerformanceHistoryInterval[] = "1d";

/// Percent values: 5.2 = +5.2 %. NaN (kMarketDataAbsent) = the series cannot
/// answer (too short, no bar near the anchor date, fewer than 21 closes, ...).
struct PerformanceStats {
    /// Change from the close on/before the anchor date to `last`. Anchors count
    /// back from `as_of` in calendar terms: 1W = 7 days, 1M/3M/6M = months,
    /// YTD = Dec 31 of the previous year, 1Y = one year. For YTD and 1Y the
    /// first bar is used when the series starts at most 4 days after the anchor
    /// (a "1y" series begins on the anchor's first trading day).
    double change_1w_pct = kMarketDataAbsent;
    double change_1m_pct = kMarketDataAbsent;
    double change_3m_pct = kMarketDataAbsent;
    double change_6m_pct = kMarketDataAbsent;
    double change_ytd_pct = kMarketDataAbsent;
    double change_1y_pct = kMarketDataAbsent;

    /// Highest high / lowest low of the bars dated within one year of `as_of`
    /// (and `live_price`, when given). For a younger listing: since listing.
    double high_52w = kMarketDataAbsent;
    double low_52w = kMarketDataAbsent;
    double from_52w_high_pct = kMarketDataAbsent; ///< (last / high_52w - 1) * 100, <= 0
    double from_52w_low_pct = kMarketDataAbsent;  ///< (last / low_52w - 1) * 100, >= 0

    /// Realised volatility: sample standard deviation of the last 20 daily log
    /// returns, annualised by sqrt(periods_per_year), in percent.
    double volatility_20d_pct = kMarketDataAbsent;

    /// The level changes are measured to: `live_price` when given, else the last close.
    double last = kMarketDataAbsent;
    /// Session date of the last bar (invalid when the series has no usable bar).
    QDate as_of{};
};

/// Derive PerformanceStats from DAILY bars (any order; bars without a positive
/// close are skipped). Bar dates are the exchange's session dates: daily bars
/// are stamped at local midnight, read correctly for every UTC offset in
/// (-12 h, +12 h].
///
/// `live_price` (optional, > 0): the current quote, so changes run to the live
/// price rather than the last close. `periods_per_year`: 252 for exchange-traded
/// instruments; pass 365 for crypto (QuoteData::quote_type == "CRYPTOCURRENCY").
PerformanceStats compute_performance(const QVector<HistoryPoint>& daily, double live_price = kMarketDataAbsent,
                                     int periods_per_year = 252);

} // namespace fincept::services
