// src/algo_engine/AlgoMarket.h
#pragma once
// AlgoMarket — market context for the algo engine: which venue a symbol trades
// on (no NSE default), its Yahoo ticker, the exchange trading date of each bar
// (for session-anchored VWAP, monthly buckets and day rollover), session-aligned
// bar starts and calendar-aware annualisation. Thin layer over
// core/market/MarketSessions + InstrumentCurrency; Qt Core only, any thread.

#include "algo_engine/AlgoEngineTypes.h"

#include <QString>
#include <QVector>

namespace fincept::core::market {
struct Venue;
}

namespace fincept::algo {

/// Timeframe whose bars a "live" (per-tick) strategy is warmed up and backtested
/// on — the same bars DeploymentRunner aggregates for it.
inline QString live_bar_timeframe() {
    return QStringLiteral("5m");
}

/// `tf` with "live" replaced by live_bar_timeframe() (and empty by "1d").
QString effective_bar_timeframe(const QString& tf);

/// Bar length in seconds of a timeframe string ("live" → live_bar_timeframe()).
int bar_seconds_for(const QString& tf);

/// Venue of `symbol` with an optional exchange hint ("NSE", "NASDAQ", "XETRA",
/// "NFO"). Broker-style "NSE:RELIANCE-EQ" is understood. A bare symbol with no
/// hint resolves like MarketSessions does (a US listing). nullptr when unknown.
const fincept::core::market::Venue* algo_venue(const QString& symbol, const QString& exchange_hint = {});

/// Yahoo ticker for a symbol: kept as-is when it already carries a suffix /
/// Yahoo syntax ("VOD.L", "^NSEI", "EURUSD=X", "BTC-USD"); a bare symbol gets the
/// hint venue's Yahoo suffix ("RELIANCE" + "NSE" → "RELIANCE.NS"); a bare symbol
/// without a usable hint stays bare (a US listing on Yahoo).
QString yahoo_symbol_for(const QString& symbol, const QString& exchange_hint = {});

/// Stamp OhlcvCandle::session_date (exchange trading date, yyyymmdd) on every bar.
/// Without a venue the UTC date of the bar open is used.
void stamp_session_dates(QVector<OhlcvCandle>& candles, const fincept::core::market::Venue* venue);

/// Exchange trading date (yyyymmdd) of an instant; the UTC date without a venue.
int trading_day_key(const fincept::core::market::Venue* venue, int64_t utc_ms);

/// Bar periods per year for annualising: MarketSessions' bars_per_year for the
/// venue, else a 252-day / 6.5-hour fallback. 0 means "derive it from the data".
double algo_periods_per_year(const fincept::core::market::Venue* venue, int bar_seconds);

/// Start (epoch ms) of the session-aligned bar containing `ts_ms`; epoch-aligned
/// without a venue (24x7 markets are epoch-aligned either way).
int64_t session_bar_start_ms(const fincept::core::market::Venue* venue, int64_t ts_ms, int bar_seconds);

/// Venue id ("NSE") / ISO country ("IN") / currency of a venue; empty for nullptr.
QString venue_id_of(const fincept::core::market::Venue* venue);
QString venue_country_of(const fincept::core::market::Venue* venue);
QString venue_currency_of(const fincept::core::market::Venue* venue);

} // namespace fincept::algo
