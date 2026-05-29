// src/algo_engine/CandleDataFetcher.cpp
#include "algo_engine/CandleDataFetcher.h"
#include "core/logging/Logger.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/instruments/InstrumentService.h"

#include <QDateTime>
#include <QJsonArray>
#include <QSet>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
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
    if (tf == "1m")  return {QStringLiteral("1m"), 1};
    if (tf == "3m")  return {QStringLiteral("1m"), 3};
    if (tf == "5m")  return {QStringLiteral("5m"), 1};
    if (tf == "15m") return {QStringLiteral("15m"), 1};
    if (tf == "30m") return {QStringLiteral("30m"), 1};
    if (tf == "1h")  return {QStringLiteral("60m"), 1};
    if (tf == "4h")  return {QStringLiteral("60m"), 4};
    if (tf == "1d")  return {QStringLiteral("1d"), 1};
    return {QStringLiteral("1d"), 1};
}

// Yahoo caps how far back intraday intervals reach. Clamp the request window so
// we don't get an empty response for asking too much history.
int yahoo_max_lookback_days(const QString& native_interval) {
    if (native_interval == "1m") return 7;
    if (native_interval == "5m" || native_interval == "15m" || native_interval == "30m") return 60;
    if (native_interval == "60m") return 730;
    return 100000; // daily / no practical cap
}

// Mirror the old Python symbol_to_yf: bare symbols are assumed NSE (.NS).
QString symbol_to_yahoo(const QString& symbol) {
    const QString s = symbol.trimmed().toUpper();
    if (s.contains('.') || s.contains('/') || s.contains('-'))
        return s;
    return s + QStringLiteral(".NS");
}

// Aggregate `factor` consecutive bars into one. Drops an incomplete trailing
// group so we never emit a misleading partial bar.
QVector<OhlcvCandle> aggregate_candles(const QVector<OhlcvCandle>& in, int factor) {
    if (factor <= 1)
        return in;
    QVector<OhlcvCandle> out;
    out.reserve(in.size() / factor + 1);
    for (int i = 0; i + factor <= in.size(); i += factor) {
        OhlcvCandle c = in[i];
        c.high = in[i].high;
        c.low = in[i].low;
        c.volume = 0;
        for (int j = 0; j < factor; ++j) {
            c.high = std::max(c.high, in[i + j].high);
            c.low = std::min(c.low, in[i + j].low);
            c.volume += in[i + j].volume;
        }
        c.close = in[i + factor - 1].close;
        c.close_time = in[i + factor - 1].close_time;
        c.is_closed = true;
        out.append(c);
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
        if (i >= opens.size() || opens[i].isNull() || highs[i].isNull() || lows[i].isNull() ||
            closes[i].isNull())
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
// Most broker get_history() implementations need more than a bare symbol:
//   - token brokers want EXCHANGE:SYMBOL:TOKEN (numeric instrument_token);
//   - tradejini wants the scrip id at parts[1];
//   - the rest resolve EXCHANGE:SYMBOL internally.
// We resolve via InstrumentService (warming its cache on a worker thread if
// needed). Unresolvable token brokers return "" so the caller skips to Yahoo.

const QSet<QString>& brokers_needing_token() {
    static const QSet<QString> s = {
        QStringLiteral("motilal"),   QStringLiteral("aliceblue"), QStringLiteral("fivepaisa"),
        QStringLiteral("iifl"),      QStringLiteral("ibkr"),      QStringLiteral("saxobank"),
    };
    return s;
}

// Worker-thread-safe synchronous instrument load. Uses the worker-safe loader
// (private named connection); never the main-thread load_from_db().
bool ensure_instruments_loaded_blocking(const QString& broker_id) {
    auto& svc = fincept::trading::InstrumentService::instance();
    if (svc.is_loaded(broker_id))
        return true;
    svc.load_from_db_worker(broker_id);
    return svc.is_loaded(broker_id);
}

// Build the symbol string <broker_id>::get_history expects. Empty result means a
// token was required but could not be resolved (caller should fall back to Yahoo).
QString resolve_broker_symbol(const QString& broker_id, const QString& bare_symbol,
                              const QString& exchange) {
    using fincept::trading::InstrumentService;
    if (bare_symbol.contains(':')) // already fully qualified by caller
        return bare_symbol;
    const QString sym = bare_symbol.trimmed().toUpper();
    const QString exch = exchange.isEmpty() ? QStringLiteral("NSE") : exchange.toUpper();

    if (broker_id == QStringLiteral("tradejini")) { // get_history reads parts[1]
        if (ensure_instruments_loaded_blocking(broker_id)) {
            auto inst = InstrumentService::instance().find(sym, exch, broker_id);
            if (inst) {
                const QString symid =
                    inst->brsymbol.isEmpty() ? QString::number(inst->instrument_token) : inst->brsymbol;
                return exch + ":" + symid;
            }
        }
        return exch + ":" + sym; // best-effort
    }
    if (brokers_needing_token().contains(broker_id)) { // EXCHANGE:SYMBOL:TOKEN
        if (!ensure_instruments_loaded_blocking(broker_id))
            return {};
        auto tok = InstrumentService::instance().instrument_token(sym, exch, broker_id);
        if (!tok.has_value() || tok.value() <= 0)
            return {};
        return exch + ":" + sym + ":" + QString::number(tok.value());
    }
    // Brokers whose get_history resolves the symbol internally via InstrumentService
    // (numeric token lookup) — warm the cache so that lookup succeeds, but pass the
    // BARE symbol: they default the exchange to NSE and a colon-form is unnecessary.
    static const QSet<QString> kInternalResolvers = {
        QStringLiteral("zerodha"), QStringLiteral("angelone"),
        QStringLiteral("dhan"),    QStringLiteral("upstox"),
    };
    if (kInternalResolvers.contains(broker_id))
        ensure_instruments_loaded_blocking(broker_id);
    // Everyone else accepts the bare symbol as-is (Fyers/Groww format it themselves;
    // Alpaca/Tradier/MetaApi want a plain ticker). Preserve the original string.
    return bare_symbol;
}

} // namespace

CandleDataFetcher& CandleDataFetcher::instance() {
    static CandleDataFetcher s;
    return s;
}

QVector<OhlcvCandle> CandleDataFetcher::broker_candles_to_ohlcv(
    const QVector<trading::BrokerCandle>& src, const QString& timeframe) {
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
    if (tf == "1m")  return QStringLiteral("1");
    if (tf == "3m")  return QStringLiteral("3");
    if (tf == "5m")  return QStringLiteral("5");
    if (tf == "15m") return QStringLiteral("15");
    if (tf == "30m") return QStringLiteral("30");
    if (tf == "1h")  return QStringLiteral("60");
    if (tf == "4h")  return QStringLiteral("240");
    if (tf == "1d")  return QStringLiteral("D");
    return QStringLiteral("D");
}

void CandleDataFetcher::fetch(const QString& symbol, const QString& timeframe,
                               int lookback_days, DataSource source,
                               const QString& broker_id, const QString& account_id,
                               CandleCallback callback) {
    if (source == DataSource::Broker ||
        (source == DataSource::Auto && !broker_id.isEmpty())) {
        fetch_from_broker(symbol, timeframe, lookback_days, broker_id, account_id,
            [callback, symbol, timeframe, lookback_days, this](bool ok, const QVector<OhlcvCandle>& candles, const QString& err) {
                if (ok && !candles.isEmpty()) {
                    callback(true, candles, {});
                    return;
                }
                LOG_WARN("CandleDataFetcher",
                         QString("Broker fetch failed for %1, falling back to Yahoo: %2")
                             .arg(symbol, err));
                fetch_from_yahoo({symbol}, timeframe, lookback_days,
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

void CandleDataFetcher::fetch_multi(const QStringList& symbols, const QString& timeframe,
                                     int lookback_days, DataSource source,
                                     const QString& broker_id, const QString& account_id,
                                     MultiCandleCallback callback) {
    if (source == DataSource::Broker ||
        (source == DataSource::Auto && !broker_id.isEmpty())) {
        QPointer<CandleDataFetcher> self = this;
        auto results = std::make_shared<QHash<QString, QVector<OhlcvCandle>>>();
        auto errors = std::make_shared<QStringList>();
        auto failed = std::make_shared<QStringList>();
        auto remaining = std::make_shared<std::atomic<int>>(symbols.size());

        for (const auto& sym : symbols) {
            fetch_from_broker(sym, timeframe, lookback_days, broker_id, account_id,
                [self, sym, results, errors, failed, remaining, callback, timeframe, lookback_days]
                (bool ok, const QVector<OhlcvCandle>& candles, const QString& err) {
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
                    self->fetch_from_yahoo(*failed, timeframe, lookback_days,
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

void CandleDataFetcher::fetch_from_broker(const QString& symbol, const QString& timeframe,
                                           int lookback_days, const QString& broker_id,
                                           const QString& account_id, CandleCallback callback) {
    QPointer<CandleDataFetcher> self = this;
    QtConcurrent::run([self, symbol, timeframe, lookback_days, broker_id, account_id, callback]() {
        auto* broker = trading::BrokerRegistry::instance().get(broker_id);
        if (!broker) {
            QMetaObject::invokeMethod(self, [callback]() {
                callback(false, {}, QStringLiteral("Broker not found"));
            }, Qt::QueuedConnection);
            return;
        }

        auto creds = trading::AccountManager::instance().load_credentials(account_id);
        if (creds.api_key.isEmpty()) {
            QMetaObject::invokeMethod(self, [callback]() {
                callback(false, {}, QStringLiteral("No credentials"));
            }, Qt::QueuedConnection);
            return;
        }

        const QDateTime to = QDateTime::currentDateTime();
        const QDateTime from = to.addDays(-lookback_days);
        const QString resolution = timeframe_to_broker_resolution(timeframe);
        const QString from_s = from.toString("yyyy-MM-dd");
        const QString to_s = to.toString("yyyy-MM-dd");

        // Build the broker-specific symbol string (EXCHANGE:SYMBOL[:TOKEN]); empty
        // means a token broker we couldn't resolve — fall straight through to Yahoo.
        const QString resolved = resolve_broker_symbol(broker_id, symbol, QStringLiteral("NSE"));

        auto has_data = [](const trading::ApiResponse<QVector<trading::BrokerCandle>>& r) {
            return r.success && r.data.value_or(QVector<trading::BrokerCandle>{}).size() > 0;
        };

        trading::ApiResponse<QVector<trading::BrokerCandle>> result;
        QString last_err;
        if (!resolved.isEmpty()) {
            result = broker->get_history(creds, resolved, resolution, from_s, to_s);
            if (!result.success)
                last_err = result.error;
        } else {
            last_err = QStringLiteral("symbol resolution failed (no instrument token)");
        }
        // Fallback: retry with the bare symbol if the resolved form yielded nothing.
        if (!has_data(result) && resolved != symbol) {
            auto r2 = broker->get_history(creds, symbol, resolution, from_s, to_s);
            if (has_data(r2))
                result = r2;
            else if (last_err.isEmpty())
                last_err = r2.error;
        }

        if (!self) return;
        if (has_data(result)) {
            auto candles = broker_candles_to_ohlcv(result.data.value_or(QVector<trading::BrokerCandle>{}), timeframe);
            QMetaObject::invokeMethod(self, [callback, candles]() {
                callback(true, candles, {});
            }, Qt::QueuedConnection);
        } else {
            const QString err = last_err.isEmpty() ? QStringLiteral("No data") : last_err;
            QMetaObject::invokeMethod(self, [callback, err]() {
                callback(false, {}, err);
            }, Qt::QueuedConnection);
        }
    });
}

void CandleDataFetcher::fetch_from_yahoo(const QStringList& symbols, const QString& timeframe,
                                          int lookback_days, MultiCandleCallback callback) {
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
    const int64_t tf_ms =
        static_cast<int64_t>(timeframe_seconds(timeframe_from_string(timeframe))) * 1000;

    auto results = std::make_shared<QHash<QString, QVector<OhlcvCandle>>>();
    auto errors = std::make_shared<QStringList>();
    auto remaining = std::make_shared<std::atomic<int>>(symbols.size());

    for (const QString& sym : symbols) {
        const QString yf = symbol_to_yahoo(sym);
        const QString url =
            QString("https://query1.finance.yahoo.com/v8/finance/chart/%1"
                    "?period1=%2&period2=%3&interval=%4&includePrePost=false")
                .arg(yf)
                .arg(period1)
                .arg(period2)
                .arg(yi.interval);

        QNetworkRequest req{QUrl(url)};
        // Yahoo blocks non-browser User-Agents — present a realistic one.
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 "
                      "(KHTML, like Gecko) Chrome/120.0 Safari/537.36");
        req.setRawHeader("Accept", "application/json");

        QPointer<CandleDataFetcher> self = this;
        QNetworkReply* reply = yahoo_nam_->get(req);
        connect(reply, &QNetworkReply::finished, this,
                [self, reply, sym, yf, tf_ms, yi, results, errors, remaining, callback]() {
                    reply->deleteLater();
                    const QByteArray body = reply->readAll();

                    if (reply->error() != QNetworkReply::NoError) {
                        errors->append(QString("%1: %2").arg(sym, reply->errorString()));
                    } else {
                        QString perr;
                        auto candles =
                            parse_yahoo_chart(QJsonDocument::fromJson(body).object(), tf_ms, &perr);
                        candles = aggregate_candles(candles, yi.aggregate);
                        if (!candles.isEmpty())
                            results->insert(sym, candles);
                        else
                            errors->append(QString("%1: %2").arg(sym, perr.isEmpty() ? "No data" : perr));
                    }

                    if (remaining->fetch_sub(1) == 1)
                        callback(*results, *errors);
                });
    }
}

} // namespace fincept::algo
