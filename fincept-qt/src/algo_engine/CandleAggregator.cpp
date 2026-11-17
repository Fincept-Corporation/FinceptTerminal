// src/algo_engine/CandleAggregator.cpp
#include "algo_engine/CandleAggregator.h"

#include "algo_engine/AlgoMarket.h"
#include "core/market/MarketSessions.h"

#include <QDateTime>
#include <QMutexLocker>
#include <QTimeZone>
#include <QTimer>

namespace fincept::algo {

namespace {
// How long after a bar's end the timer waits for a late tick before closing it.
constexpr int64_t kAggCloseGraceMs = 3000;
} // namespace

CandleAggregator::CandleAggregator(const QString& symbol, Timeframe tf, int buffer_size, QObject* parent,
                                   const fincept::core::market::Venue* venue)
    : QObject(parent),
      symbol_(symbol),
      timeframe_(tf),
      period_ms_(static_cast<int64_t>(timeframe_seconds(tf)) * 1000),
      max_buffer_(buffer_size),
      venue_(venue) {}

int64_t CandleAggregator::bar_start(int64_t timestamp_ms) const {
    return session_bar_start_ms(venue_, timestamp_ms, static_cast<int>(period_ms_ / 1000));
}

int64_t CandleAggregator::bar_end(int64_t start_ms) const {
    int64_t end = start_ms + period_ms_;
    // The last bar of a session ends at the close (NSE 1h: 15:15-15:30), not a
    // full period later — otherwise it would stay open overnight.
    if (venue_ && !venue_->is_24x7() && period_ms_ < 86400000LL) {
        const QDate day = fincept::core::market::trading_date(
            *venue_, QDateTime::fromMSecsSinceEpoch(start_ms, QTimeZone::UTC));
        const QDateTime close = fincept::core::market::session_close_utc(*venue_, day);
        if (close.isValid()) {
            const int64_t close_ms = close.toMSecsSinceEpoch();
            if (close_ms > start_ms && close_ms < end)
                end = close_ms;
        }
    }
    return end;
}

double CandleAggregator::volume_delta_locked(double volume, int64_t timestamp_ms) {
    if (!(volume > 0))
        return 0.0;
    if (per_trade_volume_)
        return volume;
    const int day = trading_day_key(venue_, timestamp_ms);
    if (!have_cum_) {
        // First quote: the cumulative figure covers the day so far, not this bar.
        have_cum_ = true;
        last_cum_ = volume;
        last_cum_day_ = day;
        return 0.0;
    }
    if (volume >= last_cum_) {
        const double d = volume - last_cum_;
        last_cum_ = volume;
        last_cum_day_ = day;
        return d;
    }
    if (day != last_cum_day_) {
        // New trading day: the cumulative counter restarted.
        last_cum_ = volume;
        last_cum_day_ = day;
        return volume;
    }
    // Volume went DOWN within the same day: this feed reports per-trade volume.
    per_trade_volume_ = true;
    return volume;
}

void CandleAggregator::open_bar_locked(int64_t timestamp_ms, double price, double volume_delta) {
    const int64_t start = bar_start(timestamp_ms);
    current_ = OhlcvCandle{};
    current_.open_time = start;
    current_.close_time = bar_end(start);
    current_.open = price;
    current_.high = price;
    current_.low = price;
    current_.close = price;
    current_.volume = volume_delta;
    current_.is_closed = false;
    current_.session_date = trading_day_key(venue_, start);
    has_current_ = true;
}

void CandleAggregator::on_tick(double price, double volume, int64_t timestamp_ms) {
    QMutexLocker lock(&mutex_);
    const double dv = volume_delta_locked(volume, timestamp_ms);

    if (!has_current_) {
        open_bar_locked(timestamp_ms, price, dv);
        return;
    }

    if (timestamp_ms >= current_.close_time) {
        current_.is_closed = true;
        const OhlcvCandle closed = current_;
        buffer_.append(closed);
        if (buffer_.size() > max_buffer_)
            buffer_.removeFirst();
        open_bar_locked(timestamp_ms, price, dv);
        // unlock before emitting — receivers may call closed_candles()
        lock.unlock();
        emit candle_closed(closed);
        return;
    }

    current_.close = price;
    if (price > current_.high)
        current_.high = price;
    if (price < current_.low)
        current_.low = price;
    current_.volume += dv;
}

void CandleAggregator::start_close_timer() {
    if (!close_timer_) {
        close_timer_ = new QTimer(this);
        close_timer_->setInterval(1000);
        connect(close_timer_, &QTimer::timeout, this, &CandleAggregator::on_close_timer);
    }
    close_timer_->start();
}

void CandleAggregator::stop_close_timer() {
    if (close_timer_)
        close_timer_->stop();
}

void CandleAggregator::on_close_timer() {
    QMutexLocker lock(&mutex_);
    if (!has_current_)
        return;
    const int64_t now = QDateTime::currentMSecsSinceEpoch();
    if (now < current_.close_time + kAggCloseGraceMs)
        return;
    // Only while the venue's regular session is open. The last bar of a session
    // keeps closing on the next tick (the next session's first trade) as before, so
    // a bar-close signal can never send an order after the market has closed.
    // Without a known venue the session hours are unknown: keep the old tick-only close.
    if (!venue_)
        return;
    if (!venue_->is_24x7() &&
        fincept::core::market::session_status(*venue_, QDateTime::fromMSecsSinceEpoch(now, QTimeZone::UTC)).state !=
            fincept::core::market::SessionState::Open)
        return;
    // The bar has ended and no tick of the next bar arrived: close it now so the
    // strategy is evaluated on time. The next tick opens a fresh bar.
    current_.is_closed = true;
    const OhlcvCandle closed = current_;
    buffer_.append(closed);
    if (buffer_.size() > max_buffer_)
        buffer_.removeFirst();
    has_current_ = false;
    lock.unlock();
    emit candle_closed(closed);
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
    const int64_t now = QDateTime::currentMSecsSinceEpoch();
    int n = historical.size();
    // Providers return the bar that is still forming as the last one; the live
    // aggregator builds that bar itself.
    auto still_forming = [&](const OhlcvCandle& c) {
        if (period_ms_ < 86400000LL)
            return c.close_time > now;
        // Daily bars are stamped a full day long; the bar is complete once the
        // venue's session for that date has closed.
        if (!venue_ || venue_->is_24x7())
            return c.close_time > now;
        const QDate day =
            fincept::core::market::trading_date(*venue_, QDateTime::fromMSecsSinceEpoch(c.open_time, QTimeZone::UTC));
        const QDateTime close = fincept::core::market::session_close_utc(*venue_, day);
        return close.isValid() && close.toMSecsSinceEpoch() > now;
    };
    while (n > 0 && still_forming(historical[n - 1]))
        --n;
    for (int i = 0; i < n; ++i) {
        OhlcvCandle c = historical[i];
        if (c.session_date == 0)
            c.session_date = trading_day_key(venue_, c.open_time);
        buffer_.append(c);
        if (buffer_.size() > max_buffer_)
            buffer_.removeFirst();
    }
}

} // namespace fincept::algo
