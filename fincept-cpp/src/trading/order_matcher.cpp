// Order Matcher — Touch-based order matching for paper trading
// Direct port of TypeScript OrderMatcher.ts

#include "order_matcher.h"
#include "paper_trading.h"
#include "core/logger.h"
#include "core/event_bus.h"
#include "core/notification.h"
#include <chrono>
#include <algorithm>

namespace fincept::trading {

// ============================================================================
// Singleton
// ============================================================================

OrderMatcher& OrderMatcher::instance() {
    static OrderMatcher inst;
    return inst;
}

// ============================================================================
// Key helper
// ============================================================================

std::string OrderMatcher::scoped_key(const std::string& portfolio_id,
                                      const std::string& symbol) const {
    return portfolio_id + ":" + symbol;
}

// ============================================================================
// Order management
// ============================================================================

void OrderMatcher::add_order(const PtOrder& order) {
    if (order.status != "pending") {
        LOG_WARN("OrderMatcher", "Skipping non-pending order: %s", order.id.c_str());
        return;
    }
    if (order.portfolio_id.empty()) {
        LOG_ERROR("OrderMatcher", "Order %s missing portfolio_id", order.id.c_str());
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto key = scoped_key(order.portfolio_id, order.symbol);
    pending_orders_[key].push_back(order);
    LOG_INFO("OrderMatcher", "Added %s %s order for %s (portfolio: %s): %s",
             order.order_type.c_str(), order.side.c_str(), order.symbol.c_str(),
             order.portfolio_id.c_str(), order.id.c_str());
}

void OrderMatcher::remove_order(const std::string& order_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [key, orders] : pending_orders_) {
        auto it = std::find_if(orders.begin(), orders.end(),
                               [&](const PtOrder& o) { return o.id == order_id; });
        if (it != orders.end()) {
            orders.erase(it);
            if (orders.empty()) pending_orders_.erase(key);
            triggered_stops_.erase(order_id);
            return;
        }
    }
}

std::vector<PtOrder> OrderMatcher::get_pending_orders(const std::string& symbol,
                                                       const std::string& portfolio_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!symbol.empty() && !portfolio_id.empty()) {
        auto key = scoped_key(portfolio_id, symbol);
        auto it = pending_orders_.find(key);
        return (it != pending_orders_.end()) ? it->second : std::vector<PtOrder>{};
    }

    if (!symbol.empty()) {
        std::vector<PtOrder> result;
        std::string suffix = ":" + symbol;
        for (const auto& [key, orders] : pending_orders_) {
            if (key.size() >= suffix.size() &&
                key.compare(key.size() - suffix.size(), suffix.size(), suffix) == 0) {
                result.insert(result.end(), orders.begin(), orders.end());
            }
        }
        return result;
    }

    std::vector<PtOrder> all;
    for (const auto& [_, orders] : pending_orders_) {
        all.insert(all.end(), orders.begin(), orders.end());
    }
    return all;
}

void OrderMatcher::load_orders(const std::vector<PtOrder>& orders) {
    for (const auto& order : orders) {
        if (order.status == "pending") add_order(order);
    }
    LOG_INFO("OrderMatcher", "Loaded %zu pending orders", orders.size());
}

void OrderMatcher::clear_portfolio_orders(const std::string& portfolio_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string prefix = portfolio_id + ":";
    std::vector<std::string> keys_to_delete;

    for (auto& [key, orders] : pending_orders_) {
        if (key.compare(0, prefix.size(), prefix) == 0) {
            for (const auto& order : orders) {
                triggered_stops_.erase(order.id);
            }
            keys_to_delete.push_back(key);
        }
    }

    for (const auto& key : keys_to_delete) {
        pending_orders_.erase(key);
    }
}

void OrderMatcher::clear_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_orders_.clear();
    triggered_stops_.clear();
}

int OrderMatcher::on_order_fill(OrderFillCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    int id = next_callback_id_++;
    fill_callbacks_[id] = std::move(callback);
    return id;
}

void OrderMatcher::remove_fill_callback(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    fill_callbacks_.erase(id);
}

int OrderMatcher::pending_order_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    int count = 0;
    for (const auto& [_, orders] : pending_orders_) {
        count += static_cast<int>(orders.size());
    }
    return count;
}

// ============================================================================
// Fill logic — touch-based matching
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

    if (order.side == "buy") {
        return price.last >= stop;
    } else {
        return price.last <= stop;
    }
}

bool OrderMatcher::check_stop_limit(const PtOrder& order, const PriceData& price) {
    if (!order.stop_price || *order.stop_price <= 0.0 ||
        !order.price || *order.price <= 0.0) {
        return false;
    }

    // Check if stop has been triggered
    if (triggered_stops_.find(order.id) == triggered_stops_.end()) {
        bool stop_triggered = (order.side == "buy")
            ? price.last >= *order.stop_price
            : price.last <= *order.stop_price;

        if (stop_triggered) {
            triggered_stops_.insert(order.id);
            LOG_INFO("OrderMatcher", "Stop triggered for order %s", order.id.c_str());
        } else {
            return false;
        }
    }

    // Stop triggered — now check limit
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

void OrderMatcher::check_orders(const std::string& symbol, const PriceData& price,
                                 const std::string& portfolio_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto key = scoped_key(portfolio_id, symbol);
    auto it = pending_orders_.find(key);
    if (it == pending_orders_.end() || it->second.empty()) return;

    std::vector<std::string> orders_to_remove;

    for (const auto& order : it->second) {
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
                LOG_INFO("OrderMatcher", "Filling order %s at %.4f", order.id.c_str(), fill_price);
                pt_fill_order(order.id, fill_price);

                auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();

                OrderFillEvent event{
                    order.id, order.symbol, order.side, order.order_type,
                    fill_price, order.quantity, now
                };

                for (const auto& [_, callback] : fill_callbacks_) {
                    try { callback(event); }
                    catch (const std::exception& e) {
                        LOG_ERROR("OrderMatcher", "Fill callback error: %s", e.what());
                    }
                }

                // Notify position update via EventBus
                core::EventBus::instance().publish("paper_trading.position_update", {
                    {"portfolio_id", portfolio_id},
                    {"symbol", symbol}
                });

                // OS + ImGui notification for background fills
                {
                    char msg[128];
                    std::snprintf(msg, sizeof(msg), "%s %s %.4f @ %.2f",
                                  order.side == "buy" ? "BUY" : "SELL",
                                  order.symbol.c_str(), order.quantity, fill_price);
                    core::notify::send("Order Filled", msg, core::NotifyLevel::Success);
                }

                orders_to_remove.push_back(order.id);
            } catch (const std::exception& e) {
                LOG_ERROR("OrderMatcher", "Failed to fill order %s: %s", order.id.c_str(), e.what());
            }
        }
    }

    // Remove filled orders
    for (const auto& oid : orders_to_remove) {
        auto& orders = it->second;
        orders.erase(
            std::remove_if(orders.begin(), orders.end(),
                           [&](const PtOrder& o) { return o.id == oid; }),
            orders.end());
        triggered_stops_.erase(oid);
    }

    if (it->second.empty()) {
        pending_orders_.erase(it);
    }
}

// ============================================================================
// SL/TP trigger engine
// ============================================================================

void OrderMatcher::set_sl_tp(const std::string& portfolio_id,
                               const std::string& symbol,
                               const std::string& order_id,
                               double sl_price,
                               double tp_price) {
    std::lock_guard<std::mutex> lock(sl_tp_mutex_);
    // Remove any existing trigger for this portfolio+symbol (one trigger per position)
    sl_tp_triggers_.erase(
        std::remove_if(sl_tp_triggers_.begin(), sl_tp_triggers_.end(),
                       [&](const SLTPTrigger& t) {
                           return t.portfolio_id == portfolio_id && t.symbol == symbol;
                       }),
        sl_tp_triggers_.end());

    if (sl_price > 0.0 || tp_price > 0.0) {
        sl_tp_triggers_.push_back({portfolio_id, symbol, order_id, "",
                                    sl_price, tp_price, false});
        LOG_INFO("OrderMatcher", "SL/TP registered: portfolio=%s symbol=%s sl=%.4f tp=%.4f",
                 portfolio_id.c_str(), symbol.c_str(), sl_price, tp_price);
    }
}

void OrderMatcher::check_sl_tp_triggers(const std::string& portfolio_id,
                                         const std::string& symbol,
                                         double current_price) {
    if (current_price <= 0.0) return;

    std::lock_guard<std::mutex> lock(sl_tp_mutex_);
    for (auto& trigger : sl_tp_triggers_) {
        if (trigger.triggered) continue;
        if (trigger.portfolio_id != portfolio_id || trigger.symbol != symbol) continue;

        const bool hit_sl = (trigger.sl_price > 0.0 && current_price <= trigger.sl_price);
        const bool hit_tp = (trigger.tp_price > 0.0 && current_price >= trigger.tp_price);

        if (!hit_sl && !hit_tp) continue;

        trigger.triggered = true;
        const char* reason = hit_sl ? "SL" : "TP";
        const double trig_price = hit_sl ? trigger.sl_price : trigger.tp_price;
        LOG_INFO("OrderMatcher", "%s triggered: symbol=%s price=%.4f level=%.4f",
                 reason, symbol.c_str(), current_price, trig_price);

        // Place a closing market order (opposite side, full remaining quantity)
        try {
            // Get current position to find side and quantity
            const auto positions = pt_get_positions(portfolio_id);
            for (const auto& pos : positions) {
                if (pos.symbol != symbol) continue;
                if (pos.quantity <= 0.0) continue;

                // Closing side is opposite of position side
                const std::string close_side = (pos.side == "long") ? "sell" : "buy";
                auto close_order = pt_place_order(
                    portfolio_id, symbol, close_side, "market",
                    pos.quantity, std::nullopt, std::nullopt, false);
                pt_fill_order(close_order.id, current_price);
                LOG_INFO("OrderMatcher", "%s close filled: side=%s qty=%.6f at %.4f",
                         reason, close_side.c_str(), pos.quantity, current_price);
                {
                    char msg[128];
                    std::snprintf(msg, sizeof(msg), "%s hit on %s — closed %.4f @ %.2f",
                                  reason, symbol.c_str(), pos.quantity, current_price);
                    core::notify::send("Stop Triggered", msg, core::NotifyLevel::Warning);
                }
                break;
            }
        } catch (const std::exception& e) {
            LOG_ERROR("OrderMatcher", "SL/TP close failed: %s", e.what());
        }
    }

    // Prune triggered entries
    sl_tp_triggers_.erase(
        std::remove_if(sl_tp_triggers_.begin(), sl_tp_triggers_.end(),
                       [](const SLTPTrigger& t) { return t.triggered; }),
        sl_tp_triggers_.end());
}

} // namespace fincept::trading
