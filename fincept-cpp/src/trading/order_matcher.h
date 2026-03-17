#pragma once
// Order Matcher — Touch-based order matching for paper trading
// Direct port of TypeScript OrderMatcher.ts (362 lines)

#include "portfolio/portfolio_types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <mutex>

namespace fincept::trading {

struct OrderFillEvent {
    std::string order_id;
    std::string symbol;
    std::string side;
    std::string order_type;
    double fill_price;
    double quantity;
    int64_t timestamp;
};

using OrderFillCallback = std::function<void(const OrderFillEvent&)>;

class OrderMatcher {
public:
    static OrderMatcher& instance();

    void add_order(const PtOrder& order);
    void remove_order(const std::string& order_id);
    std::vector<PtOrder> get_pending_orders(const std::string& symbol = "",
                                             const std::string& portfolio_id = "");

    // Check orders against current price, fill if conditions met
    void check_orders(const std::string& symbol, const PriceData& price,
                      const std::string& portfolio_id);

    // Load existing pending orders from DB on startup
    void load_orders(const std::vector<PtOrder>& orders);

    // Clear orders for a specific portfolio
    void clear_portfolio_orders(const std::string& portfolio_id);
    void clear_all();

    // Fill callback registration
    int on_order_fill(OrderFillCallback callback);
    void remove_fill_callback(int id);

    int pending_order_count() const;

    // --- SL/TP trigger engine ---
    // Register stop-loss / take-profit levels for an open position.
    // Replaces any existing trigger for the same portfolio+symbol pair.
    // sl_price=0 or tp_price=0 means "not set".
    void set_sl_tp(const std::string& portfolio_id,
                   const std::string& symbol,
                   const std::string& order_id,
                   double sl_price,
                   double tp_price);

    // Called from the portfolio refresh cycle every PORTFOLIO_REFRESH_INTERVAL.
    // If current_price crosses a registered SL or TP level the trigger fires,
    // a closing market order is placed via pt_place_order + pt_fill_order,
    // and the trigger is removed.
    void check_sl_tp_triggers(const std::string& portfolio_id,
                               const std::string& symbol,
                               double current_price);

    OrderMatcher(const OrderMatcher&) = delete;
    OrderMatcher& operator=(const OrderMatcher&) = delete;

private:
    OrderMatcher() = default;

    // Key: "portfolioId:symbol" → vector of pending orders
    std::unordered_map<std::string, std::vector<PtOrder>> pending_orders_;
    std::unordered_set<std::string> triggered_stops_;
    std::unordered_map<int, OrderFillCallback> fill_callbacks_;
    int next_callback_id_ = 1;
    mutable std::mutex mutex_;

    // SL/TP trigger storage
    struct SLTPTrigger {
        std::string portfolio_id;
        std::string symbol;
        std::string order_id;
        std::string position_side; // "buy" or "sell" — determines which direction triggers
        double sl_price = 0.0;
        double tp_price = 0.0;
        bool triggered  = false;
    };
    std::vector<SLTPTrigger> sl_tp_triggers_;
    std::mutex sl_tp_mutex_;

    std::string scoped_key(const std::string& portfolio_id, const std::string& symbol) const;

    bool should_fill(const PtOrder& order, const PriceData& price) const;
    bool check_limit(const PtOrder& order, const PriceData& price) const;
    bool check_stop(const PtOrder& order, const PriceData& price) const;
    bool check_stop_limit(const PtOrder& order, const PriceData& price);
    double get_fill_price(const PtOrder& order, const PriceData& price) const;
};

} // namespace fincept::trading
