// OrderThreadPool.cpp — see OrderThreadPool.h for why order traffic has its own executors.
#include "trading/OrderThreadPool.h"

#include <QString>
#include <QThreadPool>

namespace fincept::trading {

namespace {
constexpr int kOrderPoolThreads = 4;
constexpr int kBlockingPoolThreads = 16;
} // namespace

QThreadPool& order_thread_pool() {
    static QThreadPool* const pool = []() {
        auto* p = new QThreadPool();
        p->setObjectName(QStringLiteral("fincept-order-exec"));
        p->setMaxThreadCount(kOrderPoolThreads);
        // Threads never expire: BrokerHttp keeps one QNetworkAccessManager per
        // thread, and an expiring thread takes its keep-alive TLS connections
        // with it.
        p->setExpiryTimeout(-1);
        return p;
    }();
    return *pool;
}

QThreadPool& blocking_thread_pool() {
    static QThreadPool* const pool = []() {
        auto* p = new QThreadPool();
        p->setObjectName(QStringLiteral("fincept-order-blocking"));
        p->setMaxThreadCount(kBlockingPoolThreads);
        return p;
    }();
    return *pool;
}

} // namespace fincept::trading
