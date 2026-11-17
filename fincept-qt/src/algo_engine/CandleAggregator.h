// src/algo_engine/CandleAggregator.h
#pragma once
#include "algo_engine/AlgoEngineTypes.h"

#include <QMutex>
#include <QObject>
#include <QVector>

class QTimer;

namespace fincept::core::market {
struct Venue;
}

namespace fincept::algo {

/// Builds bars from live quotes.
///
///  * Bars are aligned to the venue's SESSION (NSE 1h: 09:15, 10:15, …; HKEX
///    restarts at the afternoon open) via MarketSessions — the same grid the
///    warm-up history uses — not to the UTC epoch. Without a venue (or for
///    24x7 markets) they are epoch-aligned.
///  * A bar closes when a tick of the next bar arrives OR, once
///    start_close_timer() runs, a few seconds after its end time even if no
///    further tick comes (a quiet market used to keep the last bar open — and
///    the strategy unevaluated — until the next trade). The timer acts only for a
///    known venue while its regular session is open (24x7 venues: always), so a
///    session's last bar still closes on the next session's first tick.
///  * Quote volume is the broker's CUMULATIVE day volume on every supported
///    broker; it is converted to per-bar deltas (summing it inflated bar volume
///    by orders of magnitude). A feed that reports per-trade volume (it goes
///    down without a new trading day) is detected and summed instead.
///  * Every bar carries its exchange trading date (session_date).
class CandleAggregator : public QObject {
    Q_OBJECT
  public:
    explicit CandleAggregator(const QString& symbol, Timeframe tf, int buffer_size = 200, QObject* parent = nullptr,
                              const fincept::core::market::Venue* venue = nullptr);

    void on_tick(double price, double volume, int64_t timestamp_ms);

    /// Start the 1 s timer that closes a bar after its end time without waiting
    /// for the next tick. Call on the thread the aggregator lives on.
    void start_close_timer();
    void stop_close_timer();

    QVector<OhlcvCandle> closed_candles() const;
    OhlcvCandle current_candle() const;
    int buffer_size() const { return max_buffer_; }

    /// Seed with history. A trailing bar that has not ended yet (a provider's
    /// in-progress bar) is dropped so the live bar does not duplicate it.
    void warm_from(const QVector<OhlcvCandle>& historical);

  signals:
    void candle_closed(const fincept::algo::OhlcvCandle& candle);

  private:
    void open_bar_locked(int64_t timestamp_ms, double price, double volume_delta);
    int64_t bar_start(int64_t timestamp_ms) const;
    int64_t bar_end(int64_t start_ms) const;
    double volume_delta_locked(double volume, int64_t timestamp_ms);
    void on_close_timer();

    QString symbol_;
    Timeframe timeframe_;
    int64_t period_ms_;
    int max_buffer_;
    const fincept::core::market::Venue* venue_ = nullptr;

    mutable QMutex mutex_;
    QVector<OhlcvCandle> buffer_;
    OhlcvCandle current_;
    bool has_current_ = false;

    // Volume delta state
    bool have_cum_ = false;
    double last_cum_ = 0;
    int last_cum_day_ = 0;
    bool per_trade_volume_ = false;

    QTimer* close_timer_ = nullptr;
};

} // namespace fincept::algo
