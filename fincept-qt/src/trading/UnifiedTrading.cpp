// Unified Trading — routes orders to live broker or paper trading engine

#include "trading/UnifiedTrading.h"

#include "core/logging/Logger.h"
#include "trading/AccountManager.h"
#include "trading/PaperTrading.h"

#include <QJsonObject>
#include <QMutexLocker>

namespace fincept::trading {

UnifiedTrading& UnifiedTrading::instance() {
    static UnifiedTrading ut;
    return ut;
}

// ============================================================================
// Session Management
// ============================================================================

TradingSession UnifiedTrading::init_session(const QString& broker, const QString& mode,
                                            const QString& paper_portfolio_id) {
    QMutexLocker lock(&mutex_);

    TradingSession session;
    session.broker = broker;
    session.mode = (mode == "live") ? "live" : "paper";
    session.is_connected = false;

    if (session.mode == "paper") {
        if (!paper_portfolio_id.isEmpty()) {
            session.paper_portfolio_id = paper_portfolio_id;
        } else {
            auto portfolio = pt_create_portfolio(broker + " Paper Trading", 1000000.0, "INR", 1.0, "cross", 0.0003);
            session.paper_portfolio_id = portfolio.id;
        }
    }

    session_ = session;
    return session;
}

std::optional<TradingSession> UnifiedTrading::get_session() const {
    QMutexLocker lock(&mutex_);
    return session_;
}

TradingSession UnifiedTrading::switch_mode(const QString& mode) {
    QMutexLocker lock(&mutex_);

    if (!session_) {
        session_ = TradingSession{};
        session_->broker = "fyers";
    }

    session_->mode = (mode == "live") ? "live" : "paper";

    if (session_->mode == "paper" && session_->paper_portfolio_id.isEmpty()) {
        auto portfolio =
            pt_create_portfolio(session_->broker + " Paper Trading", 1000000.0, "INR", 1.0, "cross", 0.0003);
        session_->paper_portfolio_id = portfolio.id;
    }

    return *session_;
}

// ============================================================================
// Order Routing
// ============================================================================

UnifiedOrderResponse UnifiedTrading::place_order(const UnifiedOrder& order) {
    QMutexLocker lock(&mutex_);

    if (!session_) {
        return {false, "", "No active trading session. Call init_session first.", ""};
    }

    if (session_->mode == "paper") {
        return place_paper_order(*session_, order);
    }
    return place_live_order(*session_, order);
}

UnifiedOrderResponse UnifiedTrading::place_paper_order(const TradingSession& session, const UnifiedOrder& order) {
    if (session.paper_portfolio_id.isEmpty()) {
        return {false, "", "No paper portfolio configured", "paper"};
    }

    QString symbol = order.exchange.isEmpty() ? order.symbol : order.exchange + ":" + order.symbol;

    QString side_str = order_side_str(order.side);
    QString type_str = order_type_str(order.order_type);

    std::optional<double> price_opt;
    if (order.order_type == OrderType::Market) {
        price_opt = 1000.0;
    } else if (order.price > 0) {
        price_opt = order.price;
    }

    std::optional<double> stop_opt;
    if (order.stop_price > 0)
        stop_opt = order.stop_price;

    try {
        auto paper_order = pt_place_order(session.paper_portfolio_id, symbol, side_str, type_str, order.quantity,
                                          price_opt, stop_opt, false);

        if (order.order_type == OrderType::Market) {
            double fill_price = order.price > 0 ? order.price : 1000.0;
            pt_fill_order(paper_order.id, fill_price);
        }

        return {true, paper_order.id, "Paper order placed", "paper"};
    } catch (const std::exception& e) {
        return {false, "", QString("Paper order failed: %1").arg(e.what()), "paper"};
    }
}

UnifiedOrderResponse UnifiedTrading::place_live_order(const TradingSession& session, const UnifiedOrder& order) {
    auto* broker = BrokerRegistry::instance().get(session.broker);
    if (!broker) {
        return {false, "", "Broker not found: " + session.broker, "live"};
    }

    auto creds = broker->load_credentials();
    if (creds.access_token.isEmpty()) {
        return {false, "", "No credentials for " + session.broker + ". Please authenticate.", "live"};
    }

    auto result = broker->place_order(creds, order);
    return {result.success, result.order_id, result.error, "live"};
}

UnifiedOrderResponse UnifiedTrading::cancel_order(const QString& order_id) {
    QMutexLocker lock(&mutex_);

    if (!session_) {
        return {false, "", "No active trading session", ""};
    }

    if (session_->mode == "paper") {
        try {
            pt_cancel_order(order_id);
            return {true, order_id, "Paper order cancelled", "paper"};
        } catch (const std::exception& e) {
            return {false, "", QString("Cancel failed: %1").arg(e.what()), "paper"};
        }
    }

    auto* broker = BrokerRegistry::instance().get(session_->broker);
    if (!broker)
        return {false, "", "Broker not found", "live"};

    auto creds = broker->load_credentials();
    auto result = broker->cancel_order(creds, order_id);
    return {result.success, order_id, result.error, "live"};
}

// ============================================================================
// Account-Aware Order Routing (new multi-account API)
// ============================================================================

UnifiedOrderResponse UnifiedTrading::place_order(const QString& account_id, const UnifiedOrder& order) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return {false, "", "Account not found: " + account_id, ""};

    if (account.trading_mode == "paper")
        return place_paper_order_for_account(account_id, order);
    return place_live_order_for_account(account_id, order);
}

UnifiedOrderResponse UnifiedTrading::cancel_order(const QString& account_id, const QString& order_id) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return {false, "", "Account not found: " + account_id, ""};

    if (account.trading_mode == "paper") {
        try {
            pt_cancel_order(order_id);
            return {true, order_id, "Paper order cancelled", "paper"};
        } catch (const std::exception& e) {
            return {false, "", QString("Cancel failed: %1").arg(e.what()), "paper"};
        }
    }

    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (!broker)
        return {false, "", "Broker not found: " + account.broker_id, "live"};

    auto creds = AccountManager::instance().load_credentials(account_id);
    auto result = broker->cancel_order(creds, order_id);
    return {result.success, order_id, result.error, "live"};
}

UnifiedOrderResponse UnifiedTrading::modify_order(const QString& account_id, const QString& order_id,
                                                   const QJsonObject& modifications) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return {false, "", "Account not found: " + account_id, ""};

    if (account.trading_mode == "paper")
        return {false, "", "Modify not supported for paper orders", "paper"};

    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (!broker)
        return {false, "", "Broker not found: " + account.broker_id, "live"};

    auto creds = AccountManager::instance().load_credentials(account_id);
    auto result = broker->modify_order(creds, order_id, modifications);
    return {result.success, order_id, result.error, "live"};
}

UnifiedOrderResponse UnifiedTrading::place_paper_order_for_account(const QString& account_id,
                                                                    const UnifiedOrder& order) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.paper_portfolio_id.isEmpty())
        return {false, "", "No paper portfolio for this account", "paper"};

    QString symbol = order.exchange.isEmpty() ? order.symbol : order.exchange + ":" + order.symbol;
    QString side_str = order_side_str(order.side);
    QString type_str = order_type_str(order.order_type);

    std::optional<double> price_opt;
    if (order.order_type == OrderType::Market)
        price_opt = 1000.0;
    else if (order.price > 0)
        price_opt = order.price;

    std::optional<double> stop_opt;
    if (order.stop_price > 0)
        stop_opt = order.stop_price;

    try {
        auto paper_order = pt_place_order(account.paper_portfolio_id, symbol, side_str, type_str, order.quantity,
                                          price_opt, stop_opt, false);
        if (order.order_type == OrderType::Market) {
            double fill_price = order.price > 0 ? order.price : 1000.0;
            pt_fill_order(paper_order.id, fill_price);
        }
        return {true, paper_order.id, "Paper order placed", "paper"};
    } catch (const std::exception& e) {
        return {false, "", QString("Paper order failed: %1").arg(e.what()), "paper"};
    }
}

UnifiedOrderResponse UnifiedTrading::place_live_order_for_account(const QString& account_id,
                                                                   const UnifiedOrder& order) {
    auto account = AccountManager::instance().get_account(account_id);
    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (!broker)
        return {false, "", "Broker not found: " + account.broker_id, "live"};

    auto creds = AccountManager::instance().load_credentials(account_id);
    if (creds.access_token.isEmpty())
        return {false, "", "No credentials for account " + account.display_name + ". Please authenticate.", "live"};

    auto result = broker->place_order(creds, order);
    return {result.success, result.order_id, result.error, "live"};
}

// ============================================================================
// Broadcast Order (multi-account simultaneous placement)
// ============================================================================

QVector<UnifiedTrading::BroadcastResult> UnifiedTrading::broadcast_order(const QStringList& account_ids,
                                                                         const UnifiedOrder& order) {
    QVector<BroadcastResult> results;
    results.reserve(account_ids.size());

    for (const auto& acct_id : account_ids) {
        auto account = AccountManager::instance().get_account(acct_id);
        if (account.account_id.isEmpty()) {
            results.append({acct_id, "Unknown", {false, "", "Account not found", ""}});
            continue;
        }
        auto response = place_order(acct_id, order);
        results.append({acct_id, account.display_name, response});
    }

    return results;
}

// ============================================================================
// Order Bridge (simplified — no detached threads in Qt)
// ============================================================================

void UnifiedTrading::start_order_bridge() {
    bridge_running_.store(true);
    LOG_INFO("UnifiedTrading", "Order signal bridge started");
}

void UnifiedTrading::stop_order_bridge() {
    bridge_running_.store(false);
    LOG_INFO("UnifiedTrading", "Order signal bridge stopping");
}

bool UnifiedTrading::is_bridge_running() const {
    return bridge_running_.load();
}

} // namespace fincept::trading
