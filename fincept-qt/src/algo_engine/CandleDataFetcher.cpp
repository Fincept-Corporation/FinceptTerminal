// src/algo_engine/CandleDataFetcher.cpp
#include "algo_engine/CandleDataFetcher.h"
#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QtConcurrent>

namespace fincept::algo {

CandleDataFetcher& CandleDataFetcher::instance() {
    static CandleDataFetcher s;
    return s;
}

QVector<OhlcvCandle> CandleDataFetcher::broker_candles_to_ohlcv(
    const QVector<trading::BrokerCandle>& src) {
    QVector<OhlcvCandle> out;
    out.reserve(src.size());
    for (const auto& c : src) {
        OhlcvCandle o;
        o.open_time = c.timestamp;
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
                         QString("Broker fetch failed for %1, falling back to yfinance: %2")
                             .arg(symbol, err));
                fetch_from_yfinance({symbol}, timeframe, lookback_days,
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

    // YFinance or Auto without broker
    fetch_from_yfinance({symbol}, timeframe, lookback_days,
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
        auto remaining = std::make_shared<std::atomic<int>>(symbols.size());

        for (const auto& sym : symbols) {
            fetch_from_broker(sym, timeframe, lookback_days, broker_id, account_id,
                [self, sym, results, errors, remaining, callback, symbols, timeframe, lookback_days]
                (bool ok, const QVector<OhlcvCandle>& candles, const QString& err) {
                    if (ok && !candles.isEmpty()) {
                        results->insert(sym, candles);
                    } else {
                        errors->append(QString("%1: %2").arg(sym, err));
                    }
                    if (remaining->fetch_sub(1) == 1) {
                        if (!results->isEmpty()) {
                            callback(*results, *errors);
                        } else if (self) {
                            self->fetch_from_yfinance(symbols, timeframe, lookback_days, callback);
                        }
                    }
                });
        }
        return;
    }

    fetch_from_yfinance(symbols, timeframe, lookback_days, callback);
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

        QDateTime to = QDateTime::currentDateTime();
        QDateTime from = to.addDays(-lookback_days);
        QString resolution = timeframe_to_broker_resolution(timeframe);

        auto result = broker->get_history(creds, symbol, resolution,
                                          from.toString("yyyy-MM-dd"),
                                          to.toString("yyyy-MM-dd"));

        if (!self) return;
        if (result.success) {
            auto candles = broker_candles_to_ohlcv(result.data.value_or({}));
            QMetaObject::invokeMethod(self, [callback, candles]() {
                callback(true, candles, {});
            }, Qt::QueuedConnection);
        } else {
            QMetaObject::invokeMethod(self, [callback, result]() {
                callback(false, {}, result.error);
            }, Qt::QueuedConnection);
        }
    });
}

void CandleDataFetcher::fetch_from_yfinance(const QStringList& symbols, const QString& timeframe,
                                              int lookback_days, MultiCandleCallback callback) {
    QJsonObject params;
    params["symbols"] = QJsonArray::fromStringList(symbols);
    params["timeframe"] = timeframe;
    params["lookback_days"] = lookback_days;
    auto json = QJsonDocument(params).toJson(QJsonDocument::Compact);

    QPointer<CandleDataFetcher> self = this;
    python::PythonRunner::instance().run(
        QStringLiteral("algo_trading/fetch_candles.py"), {json},
        [self, callback](python::PythonResult result) {
            if (!self) return;

            if (!result.success) {
                QMetaObject::invokeMethod(self, [callback, result]() {
                    callback({}, {result.error});
                }, Qt::QueuedConnection);
                return;
            }

            auto doc = QJsonDocument::fromJson(
                python::extract_json(result.output).toUtf8());
            auto root = doc.object();

            QHash<QString, QVector<OhlcvCandle>> data;
            QStringList errors;

            if (!root.value("success").toBool()) {
                errors.append(root.value("error").toString());
            } else {
                auto symbols_data = root.value("data").toObject();
                for (auto it = symbols_data.begin(); it != symbols_data.end(); ++it) {
                    QVector<OhlcvCandle> candles;
                    for (const auto& cv : it.value().toArray()) {
                        auto co = cv.toObject();
                        OhlcvCandle c;
                        c.open_time = static_cast<int64_t>(co.value("t").toDouble());
                        c.open = co.value("o").toDouble();
                        c.high = co.value("h").toDouble();
                        c.low = co.value("l").toDouble();
                        c.close = co.value("c").toDouble();
                        c.volume = co.value("v").toDouble();
                        c.is_closed = true;
                        candles.append(c);
                    }
                    data.insert(it.key(), candles);
                }
            }

            QMetaObject::invokeMethod(self, [callback, data, errors]() {
                callback(data, errors);
            }, Qt::QueuedConnection);
        });
}

} // namespace fincept::algo
