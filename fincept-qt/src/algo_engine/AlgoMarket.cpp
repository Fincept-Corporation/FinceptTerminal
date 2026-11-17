// src/algo_engine/AlgoMarket.cpp
#include "algo_engine/AlgoMarket.h"

#include "algo_engine/PerformanceMetrics.h"
#include "core/market/MarketSessions.h"

#include <QDateTime>
#include <QTimeZone>

#include <cmath>

namespace fincept::algo {

namespace mkt = fincept::core::market;

QString effective_bar_timeframe(const QString& tf) {
    const QString t = tf.trimmed();
    if (t.isEmpty())
        return QStringLiteral("1d");
    if (t.compare(QStringLiteral("live"), Qt::CaseInsensitive) == 0)
        return live_bar_timeframe();
    return t;
}

int bar_seconds_for(const QString& tf) {
    return timeframe_seconds(timeframe_from_string(effective_bar_timeframe(tf)));
}

namespace {

// "NSE:RELIANCE-EQ" → ("NSE", "RELIANCE"); broker series suffixes dropped.
void algo_split_broker_symbol(const QString& in, QString& symbol, QString& hint) {
    symbol = in.trimmed().toUpper();
    const int colon = symbol.indexOf(QLatin1Char(':'));
    if (colon > 0) {
        if (hint.isEmpty())
            hint = symbol.left(colon);
        symbol = symbol.mid(colon + 1);
    }
    for (const char* series : {"-EQ", "-BE", "-BZ", "-SM"}) {
        if (symbol.endsWith(QLatin1String(series))) {
            symbol.chop(3);
            break;
        }
    }
}

} // namespace

const mkt::Venue* algo_venue(const QString& symbol, const QString& exchange_hint) {
    QString sym;
    QString hint = exchange_hint.trimmed().toUpper();
    algo_split_broker_symbol(symbol, sym, hint);
    if (sym.isEmpty())
        return hint.isEmpty() ? nullptr : mkt::venue_by_code(hint);
    return mkt::venue_for_symbol(sym, hint);
}

QString yahoo_symbol_for(const QString& symbol, const QString& exchange_hint) {
    QString s;
    QString hint = exchange_hint.trimmed().toUpper();
    algo_split_broker_symbol(symbol, s, hint);
    if (s.isEmpty())
        return s;
    // Already Yahoo syntax: suffix, FX pair, index, futures, crypto pair.
    if (s.contains(QLatin1Char('.')) || s.contains(QLatin1Char('=')) || s.startsWith(QLatin1Char('^')) ||
        s.contains(QLatin1Char('-')) || s.contains(QLatin1Char('/')))
        return s;
    if (hint.isEmpty())
        return s;
    const mkt::Venue* v = mkt::venue_by_code(hint);
    if (!v || v->suffixes.isEmpty())
        return s;
    return s + QLatin1Char('.') + v->suffixes.first();
}

int trading_day_key(const mkt::Venue* venue, int64_t utc_ms) {
    const QDateTime utc = QDateTime::fromMSecsSinceEpoch(utc_ms, QTimeZone::UTC);
    return perf::day_key(venue ? mkt::trading_date(*venue, utc) : utc.date());
}

void stamp_session_dates(QVector<OhlcvCandle>& candles, const mkt::Venue* venue) {
    if (candles.isEmpty())
        return;
    // Daily+ bars from Yahoo are stamped at the session open (or UTC midnight for
    // some feeds) — either way the venue's trading date of the open is the bar's
    // date. Intraday bars map one-to-one. A small cache avoids a time-zone
    // conversion per bar for runs of bars inside the same UTC hour.
    int64_t cached_hour = -1;
    int cached_key = 0;
    for (auto& c : candles) {
        const int64_t hour = c.open_time / 3600000LL;
        if (hour != cached_hour) {
            cached_hour = hour;
            cached_key = trading_day_key(venue, c.open_time);
        }
        c.session_date = cached_key;
    }
}

double algo_periods_per_year(const mkt::Venue* venue, int bar_seconds) {
    if (bar_seconds <= 0)
        return 0.0;
    if (venue) {
        const double bpy = mkt::bars_per_year(*venue, bar_seconds);
        if (bpy > 0)
            return bpy;
    }
    if (bar_seconds >= 86400)
        return 252.0 * 86400.0 / bar_seconds;
    return 252.0 * std::ceil(6.5 * 3600.0 / bar_seconds);
}

int64_t session_bar_start_ms(const mkt::Venue* venue, int64_t ts_ms, int bar_seconds) {
    if (bar_seconds <= 0)
        return ts_ms;
    const int64_t period = static_cast<int64_t>(bar_seconds) * 1000;
    if (!venue || venue->is_24x7())
        return (ts_ms / period) * period;
    const QDateTime start =
        mkt::session_bucket_start(*venue, QDateTime::fromMSecsSinceEpoch(ts_ms, QTimeZone::UTC), bar_seconds);
    if (!start.isValid())
        return (ts_ms / period) * period;
    return start.toMSecsSinceEpoch();
}

QString venue_id_of(const mkt::Venue* venue) {
    return venue ? venue->id : QString();
}

QString venue_country_of(const mkt::Venue* venue) {
    if (!venue)
        return QString();
    return venue->country.isEmpty() ? venue->id : venue->country;
}

QString venue_currency_of(const mkt::Venue* venue) {
    return venue ? venue->currency : QString();
}

} // namespace fincept::algo
