#pragma once
// Unified Trading — routes orders to live broker or paper trading engine
// Mirrors: unified_trading.rs from the Tauri app
//
// Usage:
//   auto& ut = UnifiedTrading::instance();
//   ut.init_session("fyers", "paper");
//   auto result = ut.place_order(order);

#include "broker_types.h"
#include "broker_registry.h"
#include <mutex>
#include <optional>
#include <atomic>

namespace fincept::trading {

// ============================================================================
// Trading Session
// ============================================================================

struct TradingSession {
    std::string broker;
    std::string mode;                    // "live" or "paper"
    std::string paper_portfolio_id;
    bool is_connected = false;
};

struct UnifiedOrderResponse {
    bool success = false;
    std::string order_id;
    std::string message;
    std::string mode;                    // "live" or "paper"
};

// ============================================================================
// Unified Trading Service
// ============================================================================

class UnifiedTrading {
public:
    static UnifiedTrading& instance();

    // Session management
    TradingSession init_session(const std::string& broker, const std::string& mode,
                                 const std::string& paper_portfolio_id = "");
    std::optional<TradingSession> get_session() const;
    TradingSession switch_mode(const std::string& mode);

    // Order routing — dispatches to paper or live broker
    UnifiedOrderResponse place_order(const UnifiedOrder& order);
    UnifiedOrderResponse cancel_order(const std::string& order_id);

    // Order bridge for algo trading — polls pending signals and executes
    void start_order_bridge();
    void stop_order_bridge();
    bool is_bridge_running() const;

    UnifiedTrading(const UnifiedTrading&) = delete;
    UnifiedTrading& operator=(const UnifiedTrading&) = delete;

private:
    UnifiedTrading() = default;

    UnifiedOrderResponse place_paper_order(const TradingSession& session,
                                             const UnifiedOrder& order);
    UnifiedOrderResponse place_live_order(const TradingSession& session,
                                            const UnifiedOrder& order);

    // Order bridge loop
    void bridge_loop();
    int poll_and_execute_signals();

    std::optional<TradingSession> session_;
    mutable std::mutex mutex_;

    // Order bridge
    std::atomic<bool> bridge_running_{false};
    std::thread bridge_thread_;
};

} // namespace fincept::trading
