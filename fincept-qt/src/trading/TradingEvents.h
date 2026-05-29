#pragma once
// TradingEvents — typed event records for trading operations, plus helpers to
// publish them onto the application EventBus. Consumers (Action Center UI,
// latency tracker, notification bridge, audit log) subscribe to the string
// event names below.
//
// The EventBus carries QVariantMap payloads, so each typed struct provides a
// to_map() and the publish helpers fan out onto EventBus::publish(name, map).

#include "trading/TradingTypes.h"
#include "core/events/EventBus.h"

#include <QString>
#include <QVariantMap>

namespace fincept::trading {

// Event name constants (string keys for EventBus).
namespace events {
inline constexpr const char* kOrderPlaced = "trading.order_placed";
inline constexpr const char* kOrderFailed = "trading.order_failed";
inline constexpr const char* kSmartOrderNoAction = "trading.smart_order_no_action";
inline constexpr const char* kAllOrdersCancelled = "trading.all_orders_cancelled";
inline constexpr const char* kAllPositionsClosed = "trading.all_positions_closed";
inline constexpr const char* kBasketCompleted = "trading.basket_completed";
inline constexpr const char* kSplitCompleted = "trading.split_completed";
} // namespace events

struct OrderPlacedEvent {
    QString account_id;
    QString order_id;
    QString symbol;
    QString exchange;
    OrderSide action = OrderSide::Buy;
    double quantity = 0;
    QString order_type;   // "PLACE", "SMART", "BASKET", "SPLIT"
    QString mode;         // "live" or "paper"

    QVariantMap to_map() const {
        return {{"account_id", account_id}, {"order_id", order_id}, {"symbol", symbol},
                {"exchange", exchange}, {"action", action == OrderSide::Buy ? "BUY" : "SELL"},
                {"quantity", quantity}, {"order_type", order_type}, {"mode", mode}};
    }
};

struct OrderFailedEvent {
    QString account_id;
    QString order_type;
    QString symbol;
    QString error_message;
    QString mode;

    QVariantMap to_map() const {
        return {{"account_id", account_id}, {"order_type", order_type}, {"symbol", symbol},
                {"error", error_message}, {"mode", mode}};
    }
};

struct SmartOrderNoActionEvent {
    QString account_id;
    QString symbol;
    QString exchange;
    QString message;
    QVariantMap to_map() const {
        return {{"account_id", account_id}, {"symbol", symbol}, {"exchange", exchange},
                {"message", message}};
    }
};

struct AllOrdersCancelledEvent {
    QString account_id;
    int canceled_count = 0;
    int failed_count = 0;
    QString mode;
    QVariantMap to_map() const {
        return {{"account_id", account_id}, {"canceled_count", canceled_count},
                {"failed_count", failed_count}, {"mode", mode}};
    }
};

struct AllPositionsClosedEvent {
    QString account_id;
    int closed_count = 0;
    int failed_count = 0;
    QString mode;
    QVariantMap to_map() const {
        return {{"account_id", account_id}, {"closed_count", closed_count},
                {"failed_count", failed_count}, {"mode", mode}};
    }
};

struct BasketCompletedEvent {
    QString account_id;
    QString strategy;
    int successful = 0;
    int failed = 0;
    int total = 0;
    QString mode;
    QVariantMap to_map() const {
        return {{"account_id", account_id}, {"strategy", strategy}, {"successful", successful},
                {"failed", failed}, {"total", total}, {"mode", mode}};
    }
};

// Publish helpers — fan out a typed event onto the EventBus.
inline void publish(const OrderPlacedEvent& e) { EventBus::instance().publish(events::kOrderPlaced, e.to_map()); }
inline void publish(const OrderFailedEvent& e) { EventBus::instance().publish(events::kOrderFailed, e.to_map()); }
inline void publish(const SmartOrderNoActionEvent& e) { EventBus::instance().publish(events::kSmartOrderNoAction, e.to_map()); }
inline void publish(const AllOrdersCancelledEvent& e) { EventBus::instance().publish(events::kAllOrdersCancelled, e.to_map()); }
inline void publish(const AllPositionsClosedEvent& e) { EventBus::instance().publish(events::kAllPositionsClosed, e.to_map()); }
inline void publish(const BasketCompletedEvent& e) { EventBus::instance().publish(events::kBasketCompleted, e.to_map()); }

} // namespace fincept::trading
