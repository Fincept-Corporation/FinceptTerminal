#include "RateLimiter.h"

#include <algorithm>

namespace fincept::trading {

OrderRateLimiter& OrderRateLimiter::instance() {
    static OrderRateLimiter limiter;
    return limiter;
}

OrderRateLimiter::OrderRateLimiter() {
    set_limit(BrokerId::Zerodha, 10);
    set_limit(BrokerId::AngelOne, 10);
    set_limit(BrokerId::Upstox, 10);
    set_limit(BrokerId::Fyers, 10);
    set_limit(BrokerId::Dhan, 20);
    set_limit(BrokerId::Kotak, 10);
    set_limit(BrokerId::Groww, 10);
    set_limit(BrokerId::AliceBlue, 10);
    set_limit(BrokerId::FivePaisa, 10);
    set_limit(BrokerId::IIFL, 10);
    set_limit(BrokerId::Motilal, 10);
    set_limit(BrokerId::Shoonya, 10);
    set_limit(BrokerId::Alpaca, 200);
    set_limit(BrokerId::IBKR, 50);
    set_limit(BrokerId::Tradier, 10);
    set_limit(BrokerId::SaxoBank, 10);
    set_limit(BrokerId::MetaTrader4, 10);
    // TickerAll publishes no REST rate limit (only a WebSocket RATE_LIMIT error
    // code), so stay deliberately below the MT4 bridge until a documented figure
    // exists. Raise it once tickerall.com states one.
    set_limit(BrokerId::MetaTrader5, 5);
}

OrderRateLimiter::BrokerLimit& OrderRateLimiter::get_or_create(BrokerId broker) {
    QMutexLocker locker(&registry_mutex_);
    int key = static_cast<int>(broker);
    auto it = limits_.find(key);
    if (it == limits_.end())
        it = limits_.emplace(key, std::make_unique<BrokerLimit>()).first;
    return *it->second;
}

void OrderRateLimiter::acquire(BrokerId broker) {
    auto& limit = get_or_create(broker);
    QMutexLocker locker(&limit.mutex);

    // Reserve this caller's slot while still holding the lock, then sleep outside
    // it. The old code released the lock, slept, and only then stamped
    // last_order_ms — so two threads arriving together both measured the same
    // "elapsed", both slept the same time and both fired in the same instant,
    // defeating the limit exactly when concurrent paths (basket + algo + split) hit
    // one broker. Each caller now claims the next free slot: last + interval.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 min_interval_ms = 1000 / limit.orders_per_second;
    if (limit.last_order_ms > now + 60000) // wall clock stepped backwards — don't sleep out the difference
        limit.last_order_ms = now;
    const qint64 slot = std::max(now, limit.last_order_ms + min_interval_ms);
    limit.last_order_ms = slot;
    locker.unlock();

    if (slot > now)
        QThread::msleep(static_cast<unsigned long>(slot - now));
}

void OrderRateLimiter::set_limit(BrokerId broker, int orders_per_second) {
    auto& limit = get_or_create(broker);
    QMutexLocker locker(&limit.mutex);
    limit.orders_per_second = (orders_per_second > 0) ? orders_per_second : DEFAULT_LIMIT;
}

int OrderRateLimiter::get_limit(BrokerId broker) const {
    QMutexLocker locker(&registry_mutex_);
    int key = static_cast<int>(broker);
    auto it = limits_.find(key);
    if (it == limits_.end())
        return DEFAULT_LIMIT;
    return it->second->orders_per_second;
}

} // namespace fincept::trading
