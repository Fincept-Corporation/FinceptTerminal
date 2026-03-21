// Order Matcher — Qt-adapted port
// Touch-based order matching for paper trading

#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
#include "core/logging/Logger.h"
#include "core/events/EventBus.h"
#include <QMutexLocker>
#include <QDateTime>
#include <algorithm>

namespace fincept::trading {

OrderMatcher& OrderMatcher::instance() {
    static OrderMatcher inst;
    return inst;
}

QString OrderMatcher::scoped_key(const QString& portfolio_id, const QString& symbol) const {
    return portfolio_id + ":" + symbol;
}

// ============================================================================
// Order management
// ============================================================================

void OrderMatcher::add_order(const PtOrder& order) {
    if (order.status != "pending") return;
    if (order.portfolio_id.isEmpty()) return;

    QMutexLocker lock(&mutex_);
    auto key = scoped_key(order.portfolio_id, order.symbol);
    pending_orders_[key].append(order);
    LOG_INFO("OrderMatcher", "Added " + order.order_type + " " + order.side +
             " order for " + order.symbol);
}

void OrderMatcher::remove_order(const QString& order_id) {
    QMutexLocker lock(&mutex_);
    for (auto it = pending_orders_.begin(); it != pending_orders_.end(); ++it) {
        auto& orders = it.value();
        for (int i = 0; i < orders.size(); ++i) {
            if (orders[i].id == order_id) {
                orders.removeAt(i);
                if (orders.isEmpty()) pending_orders_.erase(it);
                triggered_stops_.remove(order_id);
                return;
            }
        }
    }
}

QVector<PtOrder> OrderMatcher::get_pending_orders(const QString& symbol,
                                                    const QString& portfolio_id) {
    QMutexLocker lock(&mutex_);

    if (!symbol.isEmpty() && !portfolio_id.isEmpty()) {
        auto key = scoped_key(portfolio_id, symbol);
        return pending_orders_.value(key);
    }

    if (!symbol.isEmpty()) {
        QVector<PtOrder> result;
        QString suffix = ":" + symbol;
        for (auto it = pending_orders_.constBegin(); it != pending_orders_.constEnd(); ++it) {
            if (it.key().endsWith(suffix)) {
                result.append(it.value());
            }
        }
        return result;
    }

    QVector<PtOrder> all;
    for (auto it = pending_orders_.constBegin(); it != pending_orders_.constEnd(); ++it) {
        all.append(it.value());
    }
    return all;
}

void OrderMatcher::load_orders(const QVector<PtOrder>& orders) {
    for (const auto& order : orders) {
        if (order.status == "pending") add_order(order);
    }
    LOG_INFO("OrderMatcher", QString("Loaded %1 pending orders").arg(orders.size()));
}

void OrderMatcher::clear_portfolio_orders(const QString& portfolio_id) {
    QMutexLocker lock(&mutex_);
    QString prefix = portfolio_id + ":";
    QVector<QString> keys_to_delete;

    for (auto it = pending_orders_.constBegin(); it != pending_orders_.constEnd(); ++it) {
        if (it.key().startsWith(prefix)) {
            for (const auto& order : it.value()) {
                triggered_stops_.remove(order.id);
            }
            keys_to_delete.append(it.key());
        }
    }

    for (const auto& key : keys_to_delete) {
        pending_orders_.remove(key);
    }
}

void OrderMatcher::clear_all() {
    QMutexLocker lock(&mutex_);
    pending_orders_.clear();
    triggered_stops_.clear();
}

int OrderMatcher::on_order_fill(OrderFillCallback callback) {
    QMutexLocker lock(&mutex_);
    int id = next_callback_id_++;
    fill_callbacks_[id] = std::move(callback);
    return id;
}

void OrderMatcher::remove_fill_callback(int id) {
    QMutexLocker lock(&mutex_);
    fill_callbacks_.remove(id);
}

int OrderMatcher::pending_order_count() const {
    QMutexLocker lock(&mutex_);
    int count = 0;
    for (auto it = pending_orders_.constBegin(); it != pending_orders_.constEnd(); ++it) {
        count += it.value().size();
    }
    return count;
}

// ============================================================================
// Fill logic
// ============================================================================

bool OrderMatcher::check_limit(const PtOrder& order, const PriceData& price) const {
    if (!order.price || *order.price <= 0.0) return false;
    double limit_price = *order.price;
    if (order.side == "buy") {
        double ask = (price.ask > 0.0) ? price.ask : price.last;
        return ask <= limit_price;
    } else {
        double bid = (price.bid > 0.0) ? price.bid : price.last;
        return bid >= limit_price;
    }
}

bool OrderMatcher::check_stop(const PtOrder& order, const PriceData& price) const {
    if (!order.stop_price || *order.stop_price <= 0.0) return false;
    double stop = *order.stop_price;
    if (order.side == "buy") return price.last >= stop;
    return price.last <= stop;
}

bool OrderMatcher::check_stop_limit(const PtOrder& order, const PriceData& price) {
    if (!order.stop_price || *order.stop_price <= 0.0 ||
        !order.price || *order.price <= 0.0) return false;

    if (!triggered_stops_.contains(order.id)) {
        bool stop_triggered = (order.side == "buy")
            ? price.last >= *order.stop_price
            : price.last <= *order.stop_price;

        if (stop_triggered) {
            triggered_stops_.insert(order.id);
        } else {
            return false;
        }
    }
    return check_limit(order, price);
}

bool OrderMatcher::should_fill(const PtOrder& order, const PriceData& price) const {
    if (order.order_type == "limit") return check_limit(order, price);
    if (order.order_type == "stop") return check_stop(order, price);
    return false;
}

double OrderMatcher::get_fill_price(const PtOrder& order, const PriceData& price) const {
    if (order.order_type == "limit" || order.order_type == "stop_limit") {
        return order.price.value_or(price.last);
    }
    if (order.order_type == "stop") {
        return order.stop_price.value_or(price.last);
    }
    return price.last;
}

// ============================================================================
// Main check loop
// ============================================================================

void OrderMatcher::check_orders(const QString& symbol, const PriceData& price,
                                 const QString& portfolio_id) {
    QMutexLocker lock(&mutex_);

    auto key = scoped_key(portfolio_id, symbol);
    if (!pending_orders_.contains(key)) return;
    auto& orders = pending_orders_[key];
    if (orders.isEmpty()) return;

    QVector<QString> orders_to_remove;

    for (const auto& order : orders) {
        bool fill = false;

        if (order.order_type == "limit") {
            fill = check_limit(order, price);
        } else if (order.order_type == "stop") {
            fill = check_stop(order, price);
        } else if (order.order_type == "stop_limit") {
            fill = check_stop_limit(const_cast<PtOrder&>(order), price);
        }

        if (fill) {
            double fill_price = get_fill_price(order, price);
            try {
                pt_fill_order(order.id, fill_price);

                auto now_ms = QDateTime::currentMSecsSinceEpoch();
                OrderFillEvent event{order.id, order.symbol, order.side,
                                      order.order_type, fill_price, order.quantity, now_ms};

                for (auto cb_it = fill_callbacks_.constBegin(); cb_it != fill_callbacks_.constEnd(); ++cb_it) {
                    try { cb_it.value()(event); } catch (...) {}
                }

                EventBus::instance().publish("paper_trading.position_update", {
                    {"portfolio_id", portfolio_id},
                    {"symbol", symbol}
                });

                orders_to_remove.append(order.id);
            } catch (const std::exception& e) {
                LOG_ERROR("OrderMatcher", QString("Failed to fill order %1: %2").arg(order.id, e.what()));
            }
        }
    }

    for (const auto& oid : orders_to_remove) {
        orders.erase(std::remove_if(orders.begin(), orders.end(),
                     [&](const PtOrder& o) { return o.id == oid; }), orders.end());
        triggered_stops_.remove(oid);
    }

    if (orders.isEmpty()) {
        pending_orders_.remove(key);
    }
}

// ============================================================================
// SL/TP trigger engine
// ============================================================================

void OrderMatcher::set_sl_tp(const QString& portfolio_id,
                               const QString& symbol,
                               const QString& order_id,
                               double sl_price,
                               double tp_price) {
    QMutexLocker lock(&sl_tp_mutex_);
    sl_tp_triggers_.erase(
        std::remove_if(sl_tp_triggers_.begin(), sl_tp_triggers_.end(),
                       [&](const SLTPTrigger& t) {
                           return t.portfolio_id == portfolio_id && t.symbol == symbol;
                       }),
        sl_tp_triggers_.end());

    if (sl_price > 0.0 || tp_price > 0.0) {
        sl_tp_triggers_.append({portfolio_id, symbol, order_id, "",
                                 sl_price, tp_price, false});
    }
}

void OrderMatcher::check_sl_tp_triggers(const QString& portfolio_id,
                                         const QString& symbol,
                                         double current_price) {
    if (current_price <= 0.0) return;

    QMutexLocker lock(&sl_tp_mutex_);
    for (auto& trigger : sl_tp_triggers_) {
        if (trigger.triggered) continue;
        if (trigger.portfolio_id != portfolio_id || trigger.symbol != symbol) continue;

        const bool hit_sl = (trigger.sl_price > 0.0 && current_price <= trigger.sl_price);
        const bool hit_tp = (trigger.tp_price > 0.0 && current_price >= trigger.tp_price);
        if (!hit_sl && !hit_tp) continue;

        trigger.triggered = true;
        const char* reason = hit_sl ? "SL" : "TP";

        try {
            const auto positions = pt_get_positions(portfolio_id);
            for (const auto& pos : positions) {
                if (pos.symbol != symbol || pos.quantity <= 0.0) continue;
                QString close_side = (pos.side == "long") ? "sell" : "buy";
                auto close_order = pt_place_order(portfolio_id, symbol, close_side, "market",
                    pos.quantity, std::nullopt, std::nullopt, false);
                pt_fill_order(close_order.id, current_price);
                LOG_INFO("OrderMatcher", QString("%1 triggered on %2").arg(reason, symbol));
                break;
            }
        } catch (const std::exception& e) {
            LOG_ERROR("OrderMatcher", QString("SL/TP close failed: %1").arg(e.what()));
        }
    }

    sl_tp_triggers_.erase(
        std::remove_if(sl_tp_triggers_.begin(), sl_tp_triggers_.end(),
                       [](const SLTPTrigger& t) { return t.triggered; }),
        sl_tp_triggers_.end());
}

} // namespace fincept::trading
