#pragma once
// TradingNotificationBridge — subscribes to trading EventBus events and forwards
// the user-relevant ones (order placed/failed, bulk cancel/close, basket done) to
// NotificationService as OrderFill notifications. Closes the Phase 3 §14 gap where
// the trading layer did not auto-fire notifications on order events.
//
// Install once at startup: TradingNotificationBridge::instance().install();

#include <QObject>

namespace fincept::trading {

class TradingNotificationBridge : public QObject {
    Q_OBJECT
  public:
    static TradingNotificationBridge& instance();

    // Subscribe to the trading events on the EventBus. Idempotent.
    void install();
    void uninstall();

  private:
    TradingNotificationBridge() = default;
    bool installed_ = false;
    int sub_placed_ = 0;
    int sub_failed_ = 0;
    int sub_cancelled_ = 0;
    int sub_closed_ = 0;
    int sub_basket_ = 0;
};

} // namespace fincept::trading
