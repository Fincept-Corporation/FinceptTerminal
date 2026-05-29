// src/algo_engine/CandleDataFetcher.h
#pragma once
#include "algo_engine/AlgoEngineTypes.h"
#include "trading/TradingTypes.h"

#include <QObject>
#include <QString>
#include <QVector>
#include <functional>

class QNetworkAccessManager;

namespace fincept::algo {

enum class DataSource { Broker, YFinance, Auto };

inline QString data_source_to_string(DataSource s) {
    switch (s) {
    case DataSource::Broker:   return QStringLiteral("Broker");
    case DataSource::YFinance: return QStringLiteral("YFinance");
    case DataSource::Auto:     return QStringLiteral("Auto");
    }
    return QStringLiteral("Auto");
}

inline DataSource data_source_from_string(const QString& s) {
    if (s == "Broker")   return DataSource::Broker;
    if (s == "YFinance") return DataSource::YFinance;
    return DataSource::Auto;
}

using CandleCallback = std::function<void(bool success, const QVector<OhlcvCandle>& candles, const QString& error)>;
using MultiCandleCallback = std::function<void(const QHash<QString, QVector<OhlcvCandle>>& data, const QStringList& errors)>;

class CandleDataFetcher : public QObject {
    Q_OBJECT
public:
    static CandleDataFetcher& instance();

    void fetch(const QString& symbol, const QString& timeframe, int lookback_days,
               DataSource source, const QString& broker_id, const QString& account_id,
               CandleCallback callback);

    void fetch_multi(const QStringList& symbols, const QString& timeframe, int lookback_days,
                     DataSource source, const QString& broker_id, const QString& account_id,
                     MultiCandleCallback callback);

private:
    CandleDataFetcher() = default;
    Q_DISABLE_COPY(CandleDataFetcher)

    void fetch_from_broker(const QString& symbol, const QString& timeframe, int lookback_days,
                           const QString& broker_id, const QString& account_id,
                           CandleCallback callback);

    // Native Yahoo Finance fetch (replaces the old Python yfinance fallback).
    void fetch_from_yahoo(const QStringList& symbols, const QString& timeframe,
                          int lookback_days, MultiCandleCallback callback);

    static QVector<OhlcvCandle> broker_candles_to_ohlcv(const QVector<fincept::trading::BrokerCandle>& src,
                                                        const QString& timeframe);
    static QString timeframe_to_broker_resolution(const QString& tf);

    QNetworkAccessManager* yahoo_nam_ = nullptr; // lazy-created, browser UA for Yahoo
};

} // namespace fincept::algo
