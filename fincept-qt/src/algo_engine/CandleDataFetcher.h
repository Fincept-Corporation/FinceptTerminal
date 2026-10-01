// src/algo_engine/CandleDataFetcher.h
#pragma once
#include "algo_engine/AlgoEngineTypes.h"
#include "trading/TradingTypes.h"

#include <QObject>
#include <QString>
#include <QVector>

#include <atomic>
#include <functional>
#include <memory>

class QNetworkAccessManager;

namespace fincept::algo {

enum class DataSource { Broker, YFinance, Auto };

inline QString data_source_to_string(DataSource s) {
    switch (s) {
        case DataSource::Broker:
            return QStringLiteral("Broker");
        case DataSource::YFinance:
            return QStringLiteral("YFinance");
        case DataSource::Auto:
            return QStringLiteral("Auto");
    }
    return QStringLiteral("Auto");
}

inline DataSource data_source_from_string(const QString& s) {
    if (s == "Broker")
        return DataSource::Broker;
    if (s == "YFinance")
        return DataSource::YFinance;
    return DataSource::Auto;
}

using CandleCallback = std::function<void(bool success, const QVector<OhlcvCandle>& candles, const QString& error)>;
using MultiCandleCallback =
    std::function<void(const QHash<QString, QVector<OhlcvCandle>>& data, const QStringList& errors)>;

class CandleDataFetcher : public QObject {
    Q_OBJECT
  public:
    static CandleDataFetcher& instance();

    void fetch(const QString& symbol, const QString& timeframe, int lookback_days, DataSource source,
               const QString& broker_id, const QString& account_id, CandleCallback callback);

    void fetch_multi(const QStringList& symbols, const QString& timeframe, int lookback_days, DataSource source,
                     const QString& broker_id, const QString& account_id, MultiCandleCallback callback);

  private:
    CandleDataFetcher() = default;
    Q_DISABLE_COPY(CandleDataFetcher)

    void fetch_from_broker(const QString& symbol, const QString& timeframe, int lookback_days, const QString& broker_id,
                           const QString& account_id, CandleCallback callback);

    // Native Yahoo Finance fetch (replaces the old Python yfinance fallback).
    void fetch_from_yahoo(const QStringList& symbols, const QString& timeframe, int lookback_days,
                          MultiCandleCallback callback);

    // One Yahoo chart request for `sym`. On failure with `bare_fallback_left` set it
    // re-issues once with the bare ticker (see fetch_from_yahoo) instead of reporting.
    void yahoo_fetch_one(const QString& sym, const QString& yahoo_symbol, bool bare_fallback_left,
                         const QString& interval, int aggregate, qint64 period1, qint64 period2, int64_t tf_ms,
                         std::shared_ptr<QHash<QString, QVector<OhlcvCandle>>> results,
                         std::shared_ptr<QStringList> errors, std::shared_ptr<std::atomic<int>> remaining,
                         MultiCandleCallback callback);

    static QVector<OhlcvCandle> broker_candles_to_ohlcv(const QVector<fincept::trading::BrokerCandle>& src,
                                                        const QString& timeframe);
    static QString timeframe_to_broker_resolution(const QString& tf);

    QNetworkAccessManager* yahoo_nam_ = nullptr; // lazy-created, browser UA for Yahoo
};

} // namespace fincept::algo
