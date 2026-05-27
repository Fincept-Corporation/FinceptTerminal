#pragma once

#include "trading/TradingTypes.h"

#include <QVector>
#include <algorithm>

namespace fincept::ui {

struct CandleData {
    int64_t timestamp = 0;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    double volume = 0.0;

    static CandleData from(const trading::Candle& c) {
        return {c.timestamp, c.open, c.high, c.low, c.close, c.volume};
    }

    static CandleData from(const trading::BrokerCandle& c) {
        return {c.timestamp, c.open, c.high, c.low, c.close, c.volume};
    }

    static QVector<CandleData> from_candles(const QVector<trading::Candle>& src) {
        QVector<CandleData> out;
        out.reserve(src.size());
        for (const auto& c : src)
            out.append(from(c));
        return out;
    }

    static QVector<CandleData> from_broker_candles(const QVector<trading::BrokerCandle>& src) {
        QVector<CandleData> out;
        out.reserve(src.size());
        for (const auto& c : src)
            out.append(from(c));
        return out;
    }

    static void sort_by_time(QVector<CandleData>& candles) {
        std::sort(candles.begin(), candles.end(),
                  [](const CandleData& a, const CandleData& b) { return a.timestamp < b.timestamp; });
    }
};

} // namespace fincept::ui
