// src/algo_engine/CandleAggregator.h
#pragma once
#include "algo_engine/AlgoEngineTypes.h"

#include <QMutex>
#include <QObject>
#include <QVector>

namespace fincept::algo {

class CandleAggregator : public QObject {
    Q_OBJECT
public:
    explicit CandleAggregator(const QString& symbol, Timeframe tf,
                              int buffer_size = 200, QObject* parent = nullptr);

    void on_tick(double price, double volume, int64_t timestamp_ms);

    QVector<OhlcvCandle> closed_candles() const;
    OhlcvCandle current_candle() const;
    int buffer_size() const { return max_buffer_; }

    void warm_from(const QVector<OhlcvCandle>& historical);

signals:
    void candle_closed(const fincept::algo::OhlcvCandle& candle);

private:
    void close_current_and_open_new(int64_t tick_time, double price, double volume);
    int64_t align_to_period(int64_t timestamp_ms) const;

    QString symbol_;
    Timeframe timeframe_;
    int64_t period_ms_;
    int max_buffer_;

    mutable QMutex mutex_;
    QVector<OhlcvCandle> buffer_;
    OhlcvCandle current_;
    bool has_first_tick_ = false;
};

} // namespace fincept::algo
