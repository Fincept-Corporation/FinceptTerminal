#include "SmartOrderEngine.h"
#include "../core/logging/Logger.h"
#include <cmath>

namespace fincept::trading {

SmartOrderEngine& SmartOrderEngine::instance()
{
    static SmartOrderEngine engine;
    return engine;
}

QMutex* SmartOrderEngine::get_symbol_lock(
    const QString& symbol, const QString& exchange, const QString& product)
{
    std::string key = (symbol + ":" + exchange + ":" + product).toStdString();
    QMutexLocker locker(&lock_registry_mutex_);
    auto it = symbol_locks_.find(key);
    if (it == symbol_locks_.end())
        it = symbol_locks_.emplace(key, std::make_unique<QMutex>()).first;
    return it->second.get();
}

QVector<BrokerPosition> SmartOrderEngine::get_positions_cached(
    IBroker* broker, const BrokerCredentials& creds)
{
    QMutexLocker locker(&cache_mutex_);
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto it = position_cache_.find(creds.access_token);
    if (it != position_cache_.end() && (now - it->fetched_at_ms) < CACHE_TTL_MS) {
        return it->positions;
    }
    locker.unlock();

    auto resp = broker->get_positions(creds);
    QVector<BrokerPosition> positions;
    if (resp.success && resp.data.has_value())
        positions = resp.data.value();

    locker.relock();
    position_cache_[creds.access_token] = {positions, QDateTime::currentMSecsSinceEpoch()};
    return positions;
}

void SmartOrderEngine::invalidate_cache(const QString& auth_token)
{
    QMutexLocker locker(&cache_mutex_);
    position_cache_.remove(auth_token);
}

void SmartOrderEngine::clear_all_caches()
{
    QMutexLocker locker(&cache_mutex_);
    position_cache_.clear();
}

double SmartOrderEngine::find_current_position(
    const QVector<BrokerPosition>& positions,
    const QString& symbol, const QString& exchange, const QString& product)
{
    for (const auto& p : positions) {
        if (p.symbol == symbol && p.exchange == exchange
            && (product.isEmpty() || p.product_type == product)) {
            return p.quantity;
        }
    }
    return 0.0;
}

ApiResponse<SmartOrderResult> SmartOrderEngine::execute(
    IBroker* broker, const BrokerCredentials& creds, const SmartOrder& order)
{
    auto* lock = get_symbol_lock(order.symbol, order.exchange,
                                  product_type_str(order.product_type));
    QMutexLocker locker(lock);

    auto positions = get_positions_cached(broker, creds);
    double current = find_current_position(
        positions, order.symbol, order.exchange,
        product_type_str(order.product_type));

    double target = order.position_size;
    OrderSide action;
    double quantity;

    if (target == 0 && current == 0) {
        action = order.action;
        quantity = order.quantity;
        if (quantity <= 0) {
            return {true, SmartOrderResult{false, {}, {}, 0,
                "No action needed. Position is zero and no quantity specified."}, {}};
        }
    } else if (target == 0 && current > 0) {
        action = OrderSide::Sell;
        quantity = std::abs(current);
    } else if (target == 0 && current < 0) {
        action = OrderSide::Buy;
        quantity = std::abs(current);
    } else if (current == 0) {
        action = (target > 0) ? OrderSide::Buy : OrderSide::Sell;
        quantity = std::abs(target);
    } else if (target > current) {
        action = OrderSide::Buy;
        quantity = target - current;
    } else if (target < current) {
        action = OrderSide::Sell;
        quantity = current - target;
    } else {
        return {true, SmartOrderResult{false, {}, {}, 0,
            "No action needed. Position already at target."}, {}};
    }

    LOG_INFO("SmartOrder", QString("Adjusting %1:%2 from %3 to %4 → %5 %6")
        .arg(order.symbol, order.exchange)
        .arg(current).arg(target)
        .arg(action == OrderSide::Buy ? "BUY" : "SELL")
        .arg(quantity));

    UnifiedOrder unified;
    unified.symbol = order.symbol;
    unified.exchange = order.exchange;
    unified.side = action;
    unified.quantity = quantity;
    unified.order_type = order.order_type;
    unified.price = order.price;
    unified.stop_price = order.trigger_price;
    unified.product_type = order.product_type;

    auto resp = broker->place_order(creds, unified);

    invalidate_cache(creds.access_token);

    SmartOrderResult result;
    result.action_taken = true;
    result.order_id = resp.success ? resp.order_id : QString();
    result.executed_action = action;
    result.executed_quantity = quantity;
    result.message = resp.success
        ? QString("Adjusted position from %1 to %2").arg(current).arg(target)
        : resp.error;

    return {resp.success, result, resp.error};
}

} // namespace fincept::trading
