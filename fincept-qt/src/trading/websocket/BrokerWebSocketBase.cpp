#include "trading/websocket/BrokerWebSocketBase.h"

namespace fincept::trading {

BrokerWebSocketBase::BrokerWebSocketBase(QObject* parent) : QObject(parent) {
    subscribe_debounce_.setSingleShot(true);
    subscribe_debounce_.setInterval(kSubscribeDebounceMs);
    connect(&subscribe_debounce_, &QTimer::timeout, this, [this]() { flush_subscribe_queue(); });

    connect(&health_timer_, &QTimer::timeout, this, [this]() {
        if (last_tick_ms_ == 0)
            return; // no ticks yet — connection may still be establishing
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if ((now - last_tick_ms_) > health_timeout_ms_) {
            emit error_occurred(QStringLiteral("Data stall: no ticks for %1ms").arg(now - last_tick_ms_));
            on_data_stall();
        }
    });
}

BrokerQuote BrokerWebSocketBase::merge_tick(const QString& key, const BrokerQuote& partial) {
    auto it = tick_cache_.find(key);
    if (it == tick_cache_.end()) {
        tick_cache_.insert(key, partial);
        return partial;
    }
    BrokerQuote& cached = it.value();
    if (partial.ltp != 0) cached.ltp = partial.ltp;
    if (partial.open != 0) cached.open = partial.open;
    if (partial.high != 0) cached.high = partial.high;
    if (partial.low != 0) cached.low = partial.low;
    if (partial.close != 0) cached.close = partial.close;
    if (partial.volume != 0) cached.volume = partial.volume;
    if (partial.change != 0) cached.change = partial.change;
    if (partial.change_pct != 0) cached.change_pct = partial.change_pct;
    if (partial.bid != 0) cached.bid = partial.bid;
    if (partial.ask != 0) cached.ask = partial.ask;
    if (partial.bid_size != 0) cached.bid_size = partial.bid_size;
    if (partial.ask_size != 0) cached.ask_size = partial.ask_size;
    if (partial.oi != 0) cached.oi = partial.oi;
    if (!partial.symbol.isEmpty()) cached.symbol = partial.symbol;
    if (partial.timestamp != 0) cached.timestamp = partial.timestamp;
    return cached;
}

void BrokerWebSocketBase::clear_tick_cache() {
    tick_cache_.clear();
}

void BrokerWebSocketBase::enqueue_subscribe(const QVector<qint64>& tokens) {
    for (qint64 t : tokens) {
        if (!subscribe_queue_.contains(t))
            subscribe_queue_.append(t);
    }
    if (!subscribe_debounce_.isActive())
        subscribe_debounce_.start();
}

QVector<qint64> BrokerWebSocketBase::take_subscribe_queue() {
    QVector<qint64> out = subscribe_queue_;
    subscribe_queue_.clear();
    return out;
}

void BrokerWebSocketBase::note_tick() {
    last_tick_ms_ = QDateTime::currentMSecsSinceEpoch();
}

void BrokerWebSocketBase::start_health_check(int interval_ms, int timeout_ms) {
    health_timeout_ms_ = timeout_ms;
    last_tick_ms_ = 0;
    health_timer_.start(interval_ms);
}

void BrokerWebSocketBase::stop_health_check() {
    health_timer_.stop();
}

} // namespace fincept::trading
