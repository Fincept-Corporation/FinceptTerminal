#pragma once

#include "TradingTypes.h"
#include <QMutex>
#include <QMutexLocker>
#include <QDateTime>
#include <QThread>

#include <memory>
#include <unordered_map>

namespace fincept::trading {

class OrderRateLimiter {
public:
    static OrderRateLimiter& instance();

    void acquire(BrokerId broker);
    void set_limit(BrokerId broker, int orders_per_second);
    int get_limit(BrokerId broker) const;

private:
    OrderRateLimiter();

    struct BrokerLimit {
        int orders_per_second = 10;
        qint64 last_order_ms = 0;
        QMutex mutex;
    };

    BrokerLimit& get_or_create(BrokerId broker);

    std::unordered_map<int, std::unique_ptr<BrokerLimit>> limits_;
    mutable QMutex registry_mutex_;

    static constexpr int DEFAULT_LIMIT = 10;
};

} // namespace fincept::trading
