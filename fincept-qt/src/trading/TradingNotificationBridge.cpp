#include "trading/TradingNotificationBridge.h"
#include "trading/TradingEvents.h"
#include "core/events/EventBus.h"
#include "services/notifications/NotificationService.h"

namespace fincept::trading {

using notifications::NotificationRequest;
using notifications::NotificationService;
using notifications::NotifLevel;
using notifications::NotifTrigger;

TradingNotificationBridge& TradingNotificationBridge::instance() {
    static TradingNotificationBridge b;
    return b;
}

void TradingNotificationBridge::install() {
    if (installed_)
        return;
    installed_ = true;

    auto& bus = EventBus::instance();

    sub_placed_ = bus.subscribe(events::kOrderPlaced, [](const QVariantMap& d) {
        NotificationRequest req;
        req.title = "Order Placed";
        req.message = QString("%1 %2 %3 %4 (%5) — %6")
                          .arg(d.value("action").toString(),
                               QString::number(d.value("quantity").toDouble()),
                               d.value("symbol").toString(),
                               d.value("exchange").toString(),
                               d.value("order_type").toString(),
                               d.value("mode").toString());
        req.level = NotifLevel::Info;
        req.trigger = NotifTrigger::OrderFill;
        NotificationService::instance().send(req);
    });

    sub_failed_ = bus.subscribe(events::kOrderFailed, [](const QVariantMap& d) {
        NotificationRequest req;
        req.title = "Order Failed";
        req.message = QString("%1 %2 — %3")
                          .arg(d.value("order_type").toString(),
                               d.value("symbol").toString(),
                               d.value("error").toString());
        req.level = NotifLevel::Warning;
        req.trigger = NotifTrigger::OrderFill;
        NotificationService::instance().send(req);
    });

    sub_cancelled_ = bus.subscribe(events::kAllOrdersCancelled, [](const QVariantMap& d) {
        NotificationRequest req;
        req.title = "All Orders Cancelled";
        req.message = QString("%1 cancelled, %2 failed (%3)")
                          .arg(d.value("canceled_count").toInt())
                          .arg(d.value("failed_count").toInt())
                          .arg(d.value("mode").toString());
        req.level = NotifLevel::Alert;
        req.trigger = NotifTrigger::OrderFill;
        NotificationService::instance().send(req);
    });

    sub_closed_ = bus.subscribe(events::kAllPositionsClosed, [](const QVariantMap& d) {
        NotificationRequest req;
        req.title = "All Positions Squared Off";
        req.message = QString("%1 closed, %2 failed (%3)")
                          .arg(d.value("closed_count").toInt())
                          .arg(d.value("failed_count").toInt())
                          .arg(d.value("mode").toString());
        req.level = NotifLevel::Alert;
        req.trigger = NotifTrigger::OrderFill;
        NotificationService::instance().send(req);
    });

    sub_basket_ = bus.subscribe(events::kBasketCompleted, [](const QVariantMap& d) {
        NotificationRequest req;
        req.title = "Basket Order Completed";
        req.message = QString("%1: %2/%3 placed (%4 failed)")
                          .arg(d.value("strategy").toString())
                          .arg(d.value("successful").toInt())
                          .arg(d.value("total").toInt())
                          .arg(d.value("failed").toInt());
        req.level = NotifLevel::Info;
        req.trigger = NotifTrigger::OrderFill;
        NotificationService::instance().send(req);
    });
}

void TradingNotificationBridge::uninstall() {
    if (!installed_)
        return;
    auto& bus = EventBus::instance();
    bus.unsubscribe(sub_placed_);
    bus.unsubscribe(sub_failed_);
    bus.unsubscribe(sub_cancelled_);
    bus.unsubscribe(sub_closed_);
    bus.unsubscribe(sub_basket_);
    installed_ = false;
}

} // namespace fincept::trading
