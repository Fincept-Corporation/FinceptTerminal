#include "unified_trading.h"
#include "paper_trading.h"
#include "storage/database.h"
#include "screens/algo_trading/algo_types.h"
#include "core/logger.h"
#include <thread>
#include <chrono>
#include <unordered_set>

namespace fincept::trading {

UnifiedTrading& UnifiedTrading::instance() {
    static UnifiedTrading ut;
    return ut;
}

// ============================================================================
// Session Management
// ============================================================================

TradingSession UnifiedTrading::init_session(const std::string& broker,
                                              const std::string& mode,
                                              const std::string& paper_portfolio_id) {
    std::lock_guard lock(mutex_);

    TradingSession session;
    session.broker = broker;
    session.mode = (mode == "live") ? "live" : "paper";
    session.is_connected = false;

    // For paper mode, ensure portfolio exists or create one
    if (session.mode == "paper") {
        if (!paper_portfolio_id.empty()) {
            session.paper_portfolio_id = paper_portfolio_id;
        } else {
            // Create default paper portfolio
            auto portfolio = pt_create_portfolio(
                broker + " Paper Trading",
                1000000.0,  // 10 lakh INR
                "INR",
                1.0,
                "cross",
                0.0003      // 0.03% fee
            );
            session.paper_portfolio_id = portfolio.id;
        }
    }

    session_ = session;
    return session;
}

std::optional<TradingSession> UnifiedTrading::get_session() const {
    std::lock_guard lock(mutex_);
    return session_;
}

TradingSession UnifiedTrading::switch_mode(const std::string& mode) {
    std::lock_guard lock(mutex_);

    if (!session_) {
        // Auto-init if no session
        session_ = TradingSession{};
        session_->broker = "fyers";
    }

    session_->mode = (mode == "live") ? "live" : "paper";

    // Create paper portfolio if switching to paper and none exists
    if (session_->mode == "paper" && session_->paper_portfolio_id.empty()) {
        auto portfolio = pt_create_portfolio(
            session_->broker + " Paper Trading",
            1000000.0, "INR", 1.0, "cross", 0.0003
        );
        session_->paper_portfolio_id = portfolio.id;
    }

    return *session_;
}

// ============================================================================
// Order Routing
// ============================================================================

UnifiedOrderResponse UnifiedTrading::place_order(const UnifiedOrder& order) {
    std::lock_guard lock(mutex_);

    if (!session_) {
        return {false, "", "No active trading session. Call init_session first.", ""};
    }

    if (session_->mode == "paper") {
        return place_paper_order(*session_, order);
    }
    return place_live_order(*session_, order);
}

UnifiedOrderResponse UnifiedTrading::place_paper_order(const TradingSession& session,
                                                         const UnifiedOrder& order) {
    if (session.paper_portfolio_id.empty()) {
        return {false, "", "No paper portfolio configured", "paper"};
    }

    std::string symbol = order.exchange.empty() ? order.symbol
                         : order.exchange + ":" + order.symbol;

    std::string side_str = order_side_str(order.side);
    std::string type_str = order_type_str(order.order_type);

    std::optional<double> price_opt;
    if (order.order_type == OrderType::Market) {
        price_opt = 1000.0;  // placeholder for margin calc, filled at market
    } else if (order.price > 0) {
        price_opt = order.price;
    }

    std::optional<double> stop_opt;
    if (order.stop_price > 0) stop_opt = order.stop_price;

    try {
        auto paper_order = pt_place_order(
            session.paper_portfolio_id,
            symbol, side_str, type_str,
            order.quantity, price_opt, stop_opt, false
        );

        // Auto-fill market orders immediately
        if (order.order_type == OrderType::Market) {
            double fill_price = order.price > 0 ? order.price : 1000.0;
            pt_fill_order(paper_order.id, fill_price);
        }

        return {true, paper_order.id, "Paper order placed", "paper"};
    } catch (const std::exception& e) {
        return {false, "", std::string("Paper order failed: ") + e.what(), "paper"};
    }
}

UnifiedOrderResponse UnifiedTrading::place_live_order(const TradingSession& session,
                                                        const UnifiedOrder& order) {
    auto* broker = BrokerRegistry::instance().get(session.broker);
    if (!broker) {
        return {false, "", "Broker not found: " + session.broker, "live"};
    }

    auto creds = broker->load_credentials();
    if (creds.access_token.empty()) {
        return {false, "", "No credentials for " + session.broker + ". Please authenticate.", "live"};
    }

    auto result = broker->place_order(creds, order);
    return {result.success, result.order_id, result.error, "live"};
}

UnifiedOrderResponse UnifiedTrading::cancel_order(const std::string& order_id) {
    std::lock_guard lock(mutex_);

    if (!session_) {
        return {false, "", "No active trading session", ""};
    }

    if (session_->mode == "paper") {
        try {
            pt_cancel_order(order_id);
            return {true, order_id, "Paper order cancelled", "paper"};
        } catch (const std::exception& e) {
            return {false, "", std::string("Cancel failed: ") + e.what(), "paper"};
        }
    }

    // Live cancel
    auto* broker = BrokerRegistry::instance().get(session_->broker);
    if (!broker) return {false, "", "Broker not found", "live"};

    auto creds = broker->load_credentials();
    auto result = broker->cancel_order(creds, order_id);
    return {result.success, order_id, result.error, "live"};
}

// ============================================================================
// Order Bridge — polls pending algo signals and executes them
// ============================================================================

void UnifiedTrading::start_order_bridge() {
    if (bridge_running_.exchange(true)) return;  // already running

    bridge_thread_ = std::thread([this]() { bridge_loop(); });
    bridge_thread_.detach();
    LOG_INFO("UnifiedTrading", "Order signal bridge started");
}

void UnifiedTrading::stop_order_bridge() {
    bridge_running_.store(false);
    LOG_INFO("UnifiedTrading", "Order signal bridge stopping");
}

bool UnifiedTrading::is_bridge_running() const {
    return bridge_running_.load();
}

void UnifiedTrading::bridge_loop() {
    while (bridge_running_.load()) {
        try {
            int count = poll_and_execute_signals();
            if (count > 0) {
                LOG_INFO("UnifiedTrading", "Executed %d order signal(s)", count);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("UnifiedTrading", "Bridge error: %s", e.what());
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    bridge_running_.store(false);
}

int UnifiedTrading::poll_and_execute_signals() {
    // Get pending signals from DB (only from running deployments)
    auto signals = db::ops::get_pending_signals();
    if (signals.empty()) return 0;

    // Mark as processing
    for (auto& sig : signals) {
        db::ops::update_signal_status(sig.id, "processing");
    }

    std::unordered_set<std::string> processing_symbols;
    int executed = 0;

    for (auto& sig : signals) {
        if (sig.quantity <= 0) continue;

        // Concurrency check: skip duplicate symbols in same batch
        std::string clean_sym = sig.symbol;
        auto colon = clean_sym.find(':');
        if (colon != std::string::npos)
            clean_sym = clean_sym.substr(colon + 1);
        if (!processing_symbols.insert(clean_sym).second) {
            // Reset to pending for next cycle
            db::ops::update_signal_status(sig.id, "pending");
            continue;
        }

        // Parse exchange from symbol
        std::string exchange = "NSE";
        std::string symbol = sig.symbol;
        if (auto pos = sig.symbol.find(':'); pos != std::string::npos) {
            exchange = sig.symbol.substr(0, pos);
            symbol = sig.symbol.substr(pos + 1);
        }

        // Build order
        UnifiedOrder order;
        order.symbol = symbol;
        order.exchange = exchange;
        order.side = (sig.side == "buy") ? OrderSide::Buy : OrderSide::Sell;
        order.order_type = (sig.order_type == "limit") ? OrderType::Limit : OrderType::Market;
        order.quantity = sig.quantity;
        order.price = sig.price;
        order.stop_price = sig.stop_price;
        order.product_type = ProductType::Intraday;

        // Execute via unified trading
        auto result = place_order(order);

        std::string status = result.success ? "filled" : "failed";
        std::string error = result.success ? "" : result.message;
        db::ops::update_signal_status(sig.id, status, error);

        if (result.success) executed++;
    }

    return executed;
}

} // namespace fincept::trading
