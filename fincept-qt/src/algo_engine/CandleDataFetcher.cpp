// src/algo_engine/CandleDataFetcher.cpp
#include "algo_engine/CandleDataFetcher.h"

#include "core/logging/Logger.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/HistoricalDataService.h"
#include "trading/instruments/InstrumentService.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSet>
#include <QUrl>
#include <QtConcurrent>

#include <atomic>
#include <memory>

namespace fincept::algo {

namespace {

// ── Yahoo Finance interval mapping ──────────────────────────────────────────
// Yahoo's chart API supports 1m, 2m, 5m, 15m, 30m, 60m/1h, 1d, ... but NOT 3m or
// 4h. For those we fetch the nearest finer native interval and aggregate N→1.
struct YahooInterval {
    QString interval; // native Yahoo interval string
    int aggregate;    // how many native bars compose one requested bar (>=1)
};

YahooInterval yahoo_interval(const QString& tf) {
    if (tf == "1m")
        return {QStringLiteral("1m"), 1};
    if (tf == "3m")
        return {QStringLiteral("1m"), 3};
    if (tf == "5m")
        return {QStringLiteral("5m"), 1};
    if (tf == "10m")
        return {QStringLiteral("5m"), 2}; // Yahoo has no 10m either
    if (tf == "15m")
        return {QStringLiteral("15m"), 1};
    if (tf == "30m")
        return {QStringLiteral("30m"), 1};
    if (tf == "1h")
        return {QStringLiteral("60m"), 1};
    if (tf == "4h")
        return {QStringLiteral("60m"), 4};
    if (tf == "1d")
        return {QStringLiteral("1d"), 1};
    return {QStringLiteral("1d"), 1};
}

// Yahoo caps how far back intraday intervals reach. Clamp the request window so
// we don't get an empty response for asking too much history.
int yahoo_max_lookback_days(const QString& native_interval) {
    if (native_interval == "1m")
        return 7;
    if (native_interval == "5m" || native_interval == "15m" || native_interval == "30m")
        return 60;
    if (native_interval == "60m")
        return 730;
    return 100000; // daily / no practical cap
}

// Mirror the old Python symbol_to_yf: bare symbols are assumed NSE (.NS).
QString symbol_to_yahoo(const QString& symbol) {
    const QString s = symbol.trimmed().toUpper();
    if (s.contains('.') || s.contains('/') || s.contains('-'))
        return s;
    return s + QStringLiteral(".NS");
}

// Aggregate `factor` consecutive bars into one. A group never spans two UTC days:
// the old flat grouping paired the last bar(s) of one session with the first bar(s)
// of the next whenever the session's bar count was not a multiple of `factor`
// (4h from 7 hourly bars, 10m from 75 five-minute bars) — mixing overnight gaps into
// a single OHLC bar and shifting every later bar off the session open. Intraday
// sessions (NSE/BSE/US) sit inside one UTC date, and 24h markets get clean blocks.
// A short group at a session end is a real (shorter) bar and is kept; only a short
// FINAL group is dropped, since that is the bar that may still be forming.
QVector<OhlcvCandle> aggregate_candles(const QVector<OhlcvCandle>& in, int factor) {
    if (factor <= 1)
        return in;
    constexpr int64_t kDayMs = 86400000LL;
    QVector<OhlcvCandle> out;
    out.reserve(in.size() / factor + 1);
    int i = 0;
    while (i < in.size()) {
        const int64_t day = in[i].open_time / kDayMs;
        int end = i;
        while (end < in.size() && end - i < factor && in[end].open_time / kDayMs == day)
            ++end;
        const int count = end - i;
        const bool final_group = (end >= in.size());
        if (count == factor || !final_group) {
            OhlcvCandle c = in[i];
            c.volume = 0;
            for (int j = i; j < end; ++j) {
                c.high = std::max(c.high, in[j].high);
                c.low = std::min(c.low, in[j].low);
                c.volume += in[j].volume;
            }
            c.close = in[end - 1].close;
            c.close_time = in[end - 1].close_time;
            c.is_closed = true;
            out.append(c);
        }
        i = end;
    }
    return out;
}

// Parse a Yahoo v8 chart response into OHLCV candles (epoch-ms, close_time set).
QVector<OhlcvCandle> parse_yahoo_chart(const QJsonObject& root, int64_t tf_ms, QString* err) {
    QVector<OhlcvCandle> candles;
    const auto chart = root.value("chart").toObject();
    if (!chart.value("error").isNull()) {
        if (err)
            *err = chart.value("error").toObject().value("description").toString(QStringLiteral("Yahoo error"));
        return candles;
    }
    const auto results = chart.value("result").toArray();
    if (results.isEmpty()) {
        if (err)
            *err = QStringLiteral("Empty result");
        return candles;
    }
    const auto result = results.first().toObject();
    const auto timestamps = result.value("timestamp").toArray();
    const auto quote = result.value("indicators").toObject().value("quote").toArray();
    if (timestamps.isEmpty() || quote.isEmpty()) {
        if (err)
            *err = QStringLiteral("No candles");
        return candles;
    }
    const auto q = quote.first().toObject();
    const auto opens = q.value("open").toArray();
    const auto highs = q.value("high").toArray();
    const auto lows = q.value("low").toArray();
    const auto closes = q.value("close").toArray();
    const auto volumes = q.value("volume").toArray();

    candles.reserve(timestamps.size());
    for (int i = 0; i < timestamps.size(); ++i) {
        // Yahoo emits null for gaps/halts — skip those rows (matches yfinance dropna).
        if (i >= opens.size() || opens[i].isNull() || highs[i].isNull() || lows[i].isNull() || closes[i].isNull())
            continue;
        OhlcvCandle c;
        c.open_time = static_cast<int64_t>(timestamps[i].toDouble()) * 1000; // sec → ms
        c.close_time = c.open_time + tf_ms;
        c.open = opens[i].toDouble();
        c.high = highs[i].toDouble();
        c.low = lows[i].toDouble();
        c.close = closes[i].toDouble();
        c.volume = (i < volumes.size() && !volumes[i].isNull()) ? volumes[i].toDouble() : 0.0;
        c.is_closed = true;
        candles.append(c);
    }
    return candles;
}

// ── Broker symbol resolution ─────────────────────────────────────────────────
// Broker symbol resolution + token handling moved to trading/HistoricalDataService.

} // namespace

CandleDataFetcher& CandleDataFetcher::instance() {
    static CandleDataFetcher s;
    return s;
}

QVector<OhlcvCandle> CandleDataFetcher::broker_candles_to_ohlcv(const QVector<trading::BrokerCandle>& src,
                                                                const QString& timeframe) {
    const int64_t tf_ms = static_cast<int64_t>(timeframe_seconds(timeframe_from_string(timeframe))) * 1000;
    QVector<OhlcvCandle> out;
    out.reserve(src.size());
    for (const auto& c : src) {
        OhlcvCandle o;
        // BrokerCandle.timestamp is epoch seconds; OhlcvCandle uses epoch-ms.
        int64_t ts = c.timestamp;
        if (ts > 0 && ts < 100000000000LL) // < ~year 5138 in seconds → treat as seconds
            ts *= 1000;
        o.open_time = ts;
        o.close_time = ts + tf_ms;
        o.open = c.open;
        o.high = c.high;
        o.low = c.low;
        o.close = c.close;
        o.volume = c.volume;
        o.is_closed = true;
        out.append(o);
    }
    return out;
}

QString CandleDataFetcher::timeframe_to_broker_resolution(const QString& tf) {
    if (tf == "1m")
        return QStringLiteral("1");
    if (tf == "3m")
        return QStringLiteral("3");
    if (tf == "5m")
        return QStringLiteral("5");
    if (tf == "10m")
        return QStringLiteral("10");
    if (tf == "15m")
        return QStringLiteral("15");
    if (tf == "30m")
        return QStringLiteral("30");
    if (tf == "1h")
        return QStringLiteral("60");
    if (tf == "4h")
        return QStringLiteral("240");
    if (tf == "1d")
        return QStringLiteral("D");
    return QStringLiteral("D");
}

void CandleDataFetcher::fetch(const QString& symbol, const QString& timeframe, int lookback_days, DataSource source,
                              const QString& broker_id, const QString& account_id, CandleCallback callback) {
    if (source == DataSource::Broker || (source == DataSource::Auto && !broker_id.isEmpty())) {
        fetch_from_broker(
            symbol, timeframe, lookback_days, broker_id, account_id,
            [callback, symbol, timeframe, lookback_days, this](bool ok, const QVector<OhlcvCandle>& candles,
                                                               const QString& err) {
                if (ok && !candles.isEmpty()) {
                    callback(true, candles, {});
                    return;
                }
                LOG_WARN("CandleDataFetcher",
                         QString("Broker fetch failed for %1, falling back to Yahoo: %2").arg(symbol, err));
                fetch_from_yahoo(
                    {symbol}, timeframe, lookback_days,
                    [callback, symbol](const QHash<QString, QVector<OhlcvCandle>>& data, const QStringList& errors) {
                        if (data.contains(symbol)) {
                            callback(true, data[symbol], {});
                        } else {
                            callback(false, {}, errors.isEmpty() ? "No data" : errors.first());
                        }
                    });
            });
        return;
    }

    // YFinance, or Auto without a connected broker → native Yahoo.
    fetch_from_yahoo({symbol}, timeframe, lookback_days,
                     [callback, symbol](const QHash<QString, QVector<OhlcvCandle>>& data, const QStringList& errors) {
                         if (data.contains(symbol)) {
                             callback(true, data[symbol], {});
                         } else {
                             callback(false, {}, errors.isEmpty() ? "No data" : errors.first());
                         }
                     });
}

void CandleDataFetcher::fetch_multi(const QStringList& symbols, const QString& timeframe, int lookback_days,
                                    DataSource source, const QString& broker_id, const QString& account_id,
                                    MultiCandleCallback callback) {
    if (source == DataSource::Broker || (source == DataSource::Auto && !broker_id.isEmpty())) {
        QPointer<CandleDataFetcher> self = this;
        auto results = std::make_shared<QHash<QString, QVector<OhlcvCandle>>>();
        auto errors = std::make_shared<QStringList>();
        auto failed = std::make_shared<QStringList>();
        auto remaining = std::make_shared<std::atomic<int>>(symbols.size());

        for (const auto& sym : symbols) {
            fetch_from_broker(sym, timeframe, lookback_days, broker_id, account_id,
                              [self, sym, results, errors, failed, remaining, callback, timeframe,
                               lookback_days](bool ok, const QVector<OhlcvCandle>& candles, const QString& err) {
                                  // Broker callbacks are marshalled to self's (main) thread, so the
                                  // shared containers are mutated serially — no extra locking needed.
                                  if (ok && !candles.isEmpty()) {
                                      results->insert(sym, candles);
                                  } else {
                                      failed->append(sym);
                                      errors->append(QString("%1: %2").arg(sym, err));
                                  }
                                  if (remaining->fetch_sub(1) != 1)
                                      return;
                                  // Per-symbol Yahoo fallback: retry only the symbols the broker
                                  // couldn't serve, then merge Yahoo data over the broker results.
                                  if (failed->isEmpty() || !self) {
                                      callback(*results, *errors);
                                      return;
                                  }
                                  self->fetch_from_yahoo(
                                      *failed, timeframe, lookback_days,
                                      [results, errors, callback](const QHash<QString, QVector<OhlcvCandle>>& ydata,
                                                                  const QStringList& yerr) {
                                          for (auto it = ydata.begin(); it != ydata.end(); ++it)
                                              results->insert(it.key(), it.value());
                                          QStringList merged = *errors;
                                          merged += yerr;
                                          callback(*results, merged);
                                      });
                              });
        }
        return;
    }

    fetch_from_yahoo(symbols, timeframe, lookback_days, callback);
}

void CandleDataFetcher::fetch_from_broker(const QString& symbol, const QString& timeframe, int lookback_days,
                                          const QString& broker_id, const QString& account_id,
                                          CandleCallback callback) {
    // Broker history (symbol resolution + bare-symbol fallback + cache) now lives
    // in the shared trading::HistoricalDataService; convert its BrokerCandle
    // result to the algo OhlcvCandle here. (Yahoo fallback is layered in fetch().)
    // 10m has no portable broker resolution (HistoricalDataService passes unknown
    // strings straight through, and most brokers reject "10m"): fetch 5m and merge
    // pairs, the same way the Yahoo path synthesises it.
    const bool merge_10m = (timeframe == QLatin1String("10m"));
    const QString tf = merge_10m ? QStringLiteral("5m") : timeframe;
    trading::HistoricalDataService::instance().fetch(
        symbol, tf, lookback_days, broker_id, account_id,
        [callback, tf, merge_10m](bool ok, const QVector<trading::BrokerCandle>& candles, const QString& err) {
            if (ok && !candles.isEmpty()) {
                auto out = CandleDataFetcher::broker_candles_to_ohlcv(candles, tf);
                if (merge_10m)
                    out = aggregate_candles(out, 2);
                callback(true, out, {});
            } else {
                callback(false, {}, err);
            }
        });
}

void CandleDataFetcher::fetch_from_yahoo(const QStringList& symbols, const QString& timeframe, int lookback_days,
                                         MultiCandleCallback callback) {
    if (symbols.isEmpty()) {
        callback({}, {});
        return;
    }
    if (!yahoo_nam_)
        yahoo_nam_ = new QNetworkAccessManager(this);

    const YahooInterval yi = yahoo_interval(timeframe);
    const int max_days = yahoo_max_lookback_days(yi.interval);
    int days = lookback_days > 0 ? lookback_days : max_days;
    if (days > max_days)
        days = max_days;

    const qint64 period2 = QDateTime::currentSecsSinceEpoch();
    const qint64 period1 = period2 - static_cast<qint64>(days) * 86400;
    const int64_t tf_ms = static_cast<int64_t>(timeframe_seconds(timeframe_from_string(timeframe))) * 1000;

    auto results = std::make_shared<QHash<QString, QVector<OhlcvCandle>>>();
    auto errors = std::make_shared<QStringList>();
    auto remaining = std::make_shared<std::atomic<int>>(symbols.size());

    for (const QString& sym : symbols) {
        const QString yf = symbol_to_yahoo(sym);
        // symbol_to_yahoo() appends ".NS" to a bare ticker (NSE default). A US ticker
        // typed bare ("AAPL" — the Strategy list's own placeholder) then 404s as
        // "AAPL.NS"; allow exactly one retry on the bare symbol before giving up.
        const bool bare_fallback = (yf != sym.trimmed().toUpper());
        yahoo_fetch_one(sym, yf, bare_fallback, yi.interval, yi.aggregate, period1, period2, tf_ms, results, errors,
                        remaining, callback);
    }
}

void CandleDataFetcher::yahoo_fetch_one(const QString& sym, const QString& yahoo_symbol, bool bare_fallback_left,
                                        const QString& interval, int aggregate, qint64 period1, qint64 period2,
                                        int64_t tf_ms,
                                        std::shared_ptr<QHash<QString, QVector<OhlcvCandle>>> results,
                                        std::shared_ptr<QStringList> errors,
                                        std::shared_ptr<std::atomic<int>> remaining, MultiCandleCallback callback) {
    const QString url = QString("https://query1.finance.yahoo.com/v8/finance/chart/%1"
                                "?period1=%2&period2=%3&interval=%4&includePrePost=false")
                            .arg(yahoo_symbol)
                            .arg(period1)
                            .arg(period2)
                            .arg(interval);

    QNetworkRequest req{QUrl(url)};
    // Yahoo's chart endpoint 429s detailed browser UA strings (and python-requests),
    // but accepts a bare "Mozilla/5.0" — verified empirically.
    req.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");
    req.setRawHeader("Accept", "application/json");
    // A stalled connection must not hold a scan / backtest / deployment warm-up open
    // forever (every caller blocks its UI state on this callback).
    req.setTransferTimeout(20000);

    QNetworkReply* reply = yahoo_nam_->get(req);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, sym, bare_fallback_left, interval, aggregate, period1, period2, tf_ms, results, errors,
             remaining, callback]() {
                reply->deleteLater();
                const QByteArray body = reply->readAll();

                QString failure;
                if (reply->error() != QNetworkReply::NoError) {
                    failure = reply->errorString();
                } else {
                    QString perr;
                    auto candles = parse_yahoo_chart(QJsonDocument::fromJson(body).object(), tf_ms, &perr);
                    candles = aggregate_candles(candles, aggregate);
                    if (!candles.isEmpty()) {
                        results->insert(sym, candles);
                    } else {
                        failure = perr.isEmpty() ? QStringLiteral("No data") : perr;
                    }
                }

                if (!failure.isEmpty() && bare_fallback_left) {
                    LOG_DEBUG("CandleDataFetcher",
                              QString("%1: '.NS' lookup failed (%2) — retrying as a bare symbol").arg(sym, failure));
                    yahoo_fetch_one(sym, sym.trimmed().toUpper(), false, interval, aggregate, period1, period2, tf_ms,
                                    results, errors, remaining, callback);
                    return; // this symbol is still outstanding
                }
                if (!failure.isEmpty())
                    errors->append(QString("%1: %2").arg(sym, failure));

                if (remaining->fetch_sub(1) == 1)
                    callback(*results, *errors);
            });
}

} // namespace fincept::algo
