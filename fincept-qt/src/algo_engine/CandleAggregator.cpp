// src/algo_engine/CandleAggregator.cpp
#include "algo_engine/CandleAggregator.h"

#include <QMutexLocker>

namespace fincept::algo {

CandleAggregator::CandleAggregator(const QString& symbol, Timeframe tf,
                                   int buffer_size, QObject* parent)
    : QObject(parent)
    , symbol_(symbol)
    , timeframe_(tf)
    , period_ms_(static_cast<int64_t>(timeframe_seconds(tf)) * 1000)
    , max_buffer_(buffer_size) {}

void CandleAggregator::on_tick(double price, double volume, int64_t timestamp_ms) {
    QMutexLocker lock(&mutex_);

    if (!has_first_tick_) {
        has_first_tick_ = true;
        int64_t candle_start = align_to_period(timestamp_ms);
        current_.open_time = candle_start;
        current_.close_time = candle_start + period_ms_;
        current_.open = price;
        current_.high = price;
        current_.low = price;
        current_.close = price;
        current_.volume = volume;
        current_.is_closed = false;
        return;
    }

    if (timestamp_ms >= current_.close_time) {
        close_current_and_open_new(timestamp_ms, price, volume);
        return;
    }

    current_.close = price;
    if (price > current_.high) current_.high = price;
    if (price < current_.low) current_.low = price;
    current_.volume += volume;
}

void CandleAggregator::close_current_and_open_new(int64_t tick_time,
                                                   double price, double volume) {
    current_.is_closed = true;
    OhlcvCandle closed = current_;

    buffer_.append(closed);
    if (buffer_.size() > max_buffer_)
        buffer_.removeFirst();

    int64_t new_start = align_to_period(tick_time);
    current_.open_time = new_start;
    current_.close_time = new_start + period_ms_;
    current_.open = price;
    current_.high = price;
    current_.low = price;
    current_.close = price;
    current_.volume = volume;
    current_.is_closed = false;

    // unlock before emitting — receivers may call closed_candles()
    mutex_.unlock();
    emit candle_closed(closed);
    mutex_.lock();
}

int64_t CandleAggregator::align_to_period(int64_t timestamp_ms) const {
    return (timestamp_ms / period_ms_) * period_ms_;
}

QVector<OhlcvCandle> CandleAggregator::closed_candles() const {
    QMutexLocker lock(&mutex_);
    return buffer_;
}

OhlcvCandle CandleAggregator::current_candle() const {
    QMutexLocker lock(&mutex_);
    return current_;
}

void CandleAggregator::warm_from(const QVector<OhlcvCandle>& historical) {
    QMutexLocker lock(&mutex_);
    buffer_.clear();
    for (const auto& c : historical) {
        buffer_.append(c);
        if (buffer_.size() > max_buffer_)
            buffer_.removeFirst();
    }
}

} // namespace fincept::algo
