#pragma once
// Unified Trading — routes orders to live broker or paper trading engine

#include "trading/BrokerRegistry.h"
#include "trading/TradingTypes.h"

#include <QMutex>
#include <QThread>

#include <atomic>
#include <optional>

namespace fincept::trading {

class UnifiedTrading : public QObject {
    Q_OBJECT
  public:
    static UnifiedTrading& instance();

    // Session management
    TradingSession init_session(const QString& broker, const QString& mode, const QString& paper_portfolio_id = "");
    std::optional<TradingSession> get_session() const;
    TradingSession switch_mode(const QString& mode);

    // Order routing
    UnifiedOrderResponse place_order(const UnifiedOrder& order);
    UnifiedOrderResponse cancel_order(const QString& order_id);

    // Order bridge for algo trading
    void start_order_bridge();
    void stop_order_bridge();
    bool is_bridge_running() const;

    UnifiedTrading(const UnifiedTrading&) = delete;
    UnifiedTrading& operator=(const UnifiedTrading&) = delete;

  private:
    UnifiedTrading() = default;

    UnifiedOrderResponse place_paper_order(const TradingSession& session, const UnifiedOrder& order);
    UnifiedOrderResponse place_live_order(const TradingSession& session, const UnifiedOrder& order);

    std::optional<TradingSession> session_;
    mutable QMutex mutex_;

    // Order bridge
    std::atomic<bool> bridge_running_{false};
};

} // namespace fincept::trading
