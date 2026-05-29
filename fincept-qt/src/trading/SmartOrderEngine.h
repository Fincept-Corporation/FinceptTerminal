#pragma once

#include "TradingTypes.h"
#include "BrokerInterface.h"
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QDateTime>
#include <memory>
#include <string>
#include <unordered_map>

namespace fincept::trading {

class SmartOrderEngine {
public:
    static SmartOrderEngine& instance();

    ApiResponse<SmartOrderResult> execute(
        IBroker* broker,
        const BrokerCredentials& creds,
        const SmartOrder& order);

    void invalidate_cache(const QString& auth_token);
    void clear_all_caches();

private:
    SmartOrderEngine() = default;

    QMutex* get_symbol_lock(const QString& symbol,
                            const QString& exchange,
                            const QString& product);

    struct PositionCache {
        QVector<BrokerPosition> positions;
        qint64 fetched_at_ms = 0;
    };

    QVector<BrokerPosition> get_positions_cached(
        IBroker* broker, const BrokerCredentials& creds);

    double find_current_position(
        const QVector<BrokerPosition>& positions,
        const QString& symbol,
        const QString& exchange,
        const QString& product);

    std::unordered_map<std::string, std::unique_ptr<QMutex>> symbol_locks_;
    QMutex lock_registry_mutex_;

    QHash<QString, PositionCache> position_cache_;
    QMutex cache_mutex_;
    static constexpr qint64 CACHE_TTL_MS = 1000;
};

} // namespace fincept::trading
