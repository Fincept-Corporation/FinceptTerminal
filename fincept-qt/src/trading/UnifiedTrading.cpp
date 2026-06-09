// Unified Trading — routes orders to live broker or paper trading engine

#include "trading/UnifiedTrading.h"

#include "core/logging/Logger.h"
#include "trading/AccountManager.h"
#include "trading/OrderValidator.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"
#include "trading/SmartOrderEngine.h"
#include "trading/StrategyPortfolio.h"
#include "trading/TradingEvents.h"

#include "storage/sqlite/Database.h"

#include <QJsonObject>
#include <QMutexLocker>
#include <QPointer>
#include <QSqlQuery>
#include <QThread>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

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

    // Pre-flight validation (Phase 3 §9).
    auto vr = OrderValidator::validate(order);
    if (!vr.valid) {
        const QString err = vr.errors.join("; ");
        publish(OrderFailedEvent{account_id, "PLACE", order.symbol, err, account.trading_mode});
        return {false, "", "Validation failed: " + err, account.trading_mode};
    }

    // Quantity freeze check (Phase 3 §17). Exchanges cap the max quantity per
    // single order (e.g. NSE NIFTY futures = 1800). place_order is synchronous;
    // an auto-split is inherently async (place_split_orders runs on a worker
    // thread with a callback). To keep place_order's sync contract, we REJECT
    // over-limit orders here with an actionable message rather than silently
    // turning into an async split. Callers that want the split should use
    // place_order_auto_split() instead.
    const int freeze = qty_freeze_limit(order.symbol, order.exchange);
    if (freeze > 0 && order.quantity > freeze) {
        const QString err =
            QString("Quantity %1 exceeds freeze limit %2 for %3 on %4 — "
                    "use Split Order (split size %2) or place_order_auto_split().")
                .arg(static_cast<qint64>(order.quantity))
                .arg(freeze)
                .arg(order.symbol, order.exchange);
        publish(OrderFailedEvent{account_id, "PLACE", order.symbol, err, account.trading_mode});
        return {false, "", err, account.trading_mode};
    }

    UnifiedOrderResponse resp = (account.trading_mode == "paper")
        ? place_paper_order_for_account(account_id, order)
        : place_live_order_for_account(account_id, order);

    if (resp.success) {
        publish(OrderPlacedEvent{account_id, resp.order_id, order.symbol, order.exchange,
                                 order.side, order.quantity, "PLACE", account.trading_mode});
    } else {
        publish(OrderFailedEvent{account_id, "PLACE", order.symbol, resp.message, account.trading_mode});
    }
    return resp;
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

    // Store the BARE symbol (no exchange prefix). Positions mark to market against
    // the live quote feed published under the bare symbol (broker:<id>:<acct>:quote:
    // <bare>), and this matches the Equity screen's convention. Re-prefixing here as
    // "NFO:NIFTY…" was why F&O positions never marked (the prefix never matched a
    // quote topic) and showed a frozen/garbage price.
    const QString symbol = order.symbol;
    const QString side_str = order_side_str(order.side);
    const QString type_str = order_type_str(order.order_type);

    // Forward the broker product (MIS/CNC/NRML) and exchange so the engine applies
    // per-product leverage and TAGS the position with its real product. Without it
    // the product defaulted to CNC (delivery), which made square-off-all skip the
    // position entirely (it filters to intraday) — the "doesn't exit" bug.
    const QString product = product_to_broker_str(order.product_type);

    // Resolve the order's reference / fill price. Limit & stop-limit carry their own
    // trigger; market and plain-stop fill at the caller-supplied live price
    // (order.price = LTP at click time). A market order with no usable price is
    // rejected with a clear message rather than the old hardcoded 1000.0 sentinel.
    std::optional<double> price_opt;
    if (order.order_type == OrderType::Limit || order.order_type == OrderType::StopLossLimit) {
        if (order.price > 0)
            price_opt = order.price;
    } else if (order.price > 0) {
        price_opt = order.price;
    }
    std::optional<double> stop_opt;
    if (order.stop_price > 0)
        stop_opt = order.stop_price;

    const bool is_market = (order.order_type == OrderType::Market);
    if (is_market && !price_opt)
        return {false, "", "No live price yet for a market fill — wait for quotes to load", "paper"};

    try {
        auto paper_order = pt_place_order(account.paper_portfolio_id, symbol, side_str, type_str, order.quantity,
                                          price_opt, stop_opt, /*reduce_only=*/false, order.exchange, product);
        if (is_market) {
            pt_fill_order(paper_order.id, *price_opt);
        } else {
            // Limit / stop / stop-limit: hand to the matcher so it fills on a real
            // market touch (driven centrally off the quote feed). Previously these
            // were placed and then left "pending" forever — never filled.
            OrderMatcher::instance().add_order(paper_order);
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
// Phase 1: Bulk Operations (OpenAlgo bridge)
// ============================================================================

ApiResponse<CancelAllResult> UnifiedTrading::cancel_all_orders(const QString& account_id) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return {false, std::nullopt, "Account not found: " + account_id};

    if (account.trading_mode == "paper") {
        CancelAllResult result;
        auto pending = pt_get_orders(account.paper_portfolio_id, "pending");
        for (const auto& order : pending) {
            try {
                pt_cancel_order(order.id);
                result.canceled_order_ids.append(order.id);
            } catch (...) {
                result.failed.append({order.id, "Cancel failed"});
            }
            result.total_attempted++;
        }
        publish(AllOrdersCancelledEvent{account_id, (int)result.canceled_order_ids.size(),
                                        (int)result.failed.size(), "paper"});
        return {true, result, {}};
    }

    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (!broker)
        return {false, std::nullopt, "Broker not found: " + account.broker_id};

    auto creds = AccountManager::instance().load_credentials(account_id);
    auto resp = broker->cancel_all_orders(creds);
    if (resp.success && resp.data.has_value()) {
        publish(AllOrdersCancelledEvent{account_id, (int)resp.data->canceled_order_ids.size(),
                                        (int)resp.data->failed.size(), "live"});
    }
    return resp;
}

ApiResponse<CloseAllResult> UnifiedTrading::close_all_positions(const QString& account_id) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return {false, std::nullopt, "Account not found: " + account_id};

    if (account.trading_mode == "paper") {
        CloseAllResult result;
        auto positions = pt_get_positions(account.paper_portfolio_id);
        for (const auto& pos : positions) {
            if (pos.quantity == 0) continue;
            result.total_positions++;
            // Close with the OPPOSITE side. Paper positions store quantity as a
            // positive magnitude with side="long"/"short", so the quantity sign is
            // unreliable (shorts are +) — derive the direction from `side`. Using
            // the sign sold a short instead of buying it back, so it never closed.
            const QString dir = pos.side.toLower();
            const bool is_short = dir == QLatin1String("short") || dir == QLatin1String("sell") || pos.quantity < 0;
            const QString side_str = is_short ? QStringLiteral("buy") : QStringLiteral("sell");
            const double fill = pos.current_price > 0.0 ? pos.current_price : pos.entry_price;
            try {
                auto paper_order = pt_place_order(account.paper_portfolio_id, pos.symbol,
                    side_str, "market", std::abs(pos.quantity), fill, std::nullopt, false);
                pt_fill_order(paper_order.id, fill);
                result.closed_symbols.append(pos.symbol);
                LOG_INFO("UnifiedTrading", QString("close_all paper: %1 %2 x%3 @ %4 (was %5)")
                                               .arg(side_str, pos.symbol).arg(std::abs(pos.quantity)).arg(fill).arg(pos.side));
            } catch (const std::exception& e) {
                result.failed.append({pos.symbol, e.what()});
                LOG_WARN("UnifiedTrading", QString("close_all paper failed for %1: %2").arg(pos.symbol, e.what()));
            }
        }
        publish(AllPositionsClosedEvent{account_id, (int)result.closed_symbols.size(),
                                        (int)result.failed.size(), "paper"});
        return {true, result, {}};
    }

    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (!broker)
        return {false, std::nullopt, "Broker not found: " + account.broker_id};

    auto creds = AccountManager::instance().load_credentials(account_id);
    auto resp = broker->close_all_positions(creds);
    if (resp.success && resp.data.has_value()) {
        publish(AllPositionsClosedEvent{account_id, (int)resp.data->closed_symbols.size(),
                                        (int)resp.data->failed.size(), "live"});
    }
    return resp;
}

ApiResponse<OrderPlaceResponse> UnifiedTrading::close_position(
    const QString& account_id, const QString& symbol,
    const QString& exchange, const QString& product_type)
{
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return {false, std::nullopt, "Account not found: " + account_id};

    if (account.trading_mode == "paper") {
        auto positions = pt_get_positions(account.paper_portfolio_id);
        for (const auto& pos : positions) {
            QString pos_sym = pos.symbol;
            if (pos_sym == symbol || pos_sym == exchange + ":" + symbol) {
                if (pos.quantity == 0) continue;
                // Close by the OPPOSITE of the position's `side` (quantity is a
                // positive magnitude; shorts are +, so the sign is unreliable).
                const QString dir = pos.side.toLower();
                const bool is_short = dir == QLatin1String("short") || dir == QLatin1String("sell") || pos.quantity < 0;
                const QString side_str = is_short ? QStringLiteral("buy") : QStringLiteral("sell");
                const double fill = pos.current_price > 0.0 ? pos.current_price : pos.entry_price;
                try {
                    auto paper_order = pt_place_order(account.paper_portfolio_id, pos.symbol,
                        side_str, "market", std::abs(pos.quantity), fill, std::nullopt, false);
                    pt_fill_order(paper_order.id, fill);
                    return {true, OrderPlaceResponse{true, paper_order.id, {}}, {}};
                } catch (const std::exception& e) {
                    return {false, std::nullopt, QString("Close failed: %1").arg(e.what())};
                }
            }
        }
        return {false, std::nullopt, "Position not found"};
    }

    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (!broker)
        return {false, std::nullopt, "Broker not found: " + account.broker_id};

    auto creds = AccountManager::instance().load_credentials(account_id);
    return broker->close_position(creds, symbol, exchange, product_type);
}

ApiResponse<SmartOrderResult> UnifiedTrading::place_smart_order(
    const QString& account_id, const SmartOrder& order)
{
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return {false, std::nullopt, "Account not found: " + account_id};

    if (account.trading_mode == "paper") {
        // Paper mode: get paper positions, calculate delta, place paper order
        auto positions = pt_get_positions(account.paper_portfolio_id);
        double current = 0;
        QString paper_sym = order.exchange.isEmpty() ? order.symbol : order.exchange + ":" + order.symbol;
        for (const auto& p : positions) {
            if (p.symbol == paper_sym) {
                current = p.quantity;
                break;
            }
        }

        double target = order.position_size;
        OrderSide action;
        double quantity;

        if (target == 0 && current == 0) {
            action = order.action;
            quantity = order.quantity;
            if (quantity <= 0)
                return {true, SmartOrderResult{false, {}, {}, 0,
                    "No action needed. Position is zero."}, {}};
        } else if (target == 0 && current > 0) {
            action = OrderSide::Sell; quantity = std::abs(current);
        } else if (target == 0 && current < 0) {
            action = OrderSide::Buy; quantity = std::abs(current);
        } else if (current == 0) {
            action = (target > 0) ? OrderSide::Buy : OrderSide::Sell;
            quantity = std::abs(target);
        } else if (target > current) {
            action = OrderSide::Buy; quantity = target - current;
        } else if (target < current) {
            action = OrderSide::Sell; quantity = current - target;
        } else {
            return {true, SmartOrderResult{false, {}, {}, 0,
                "No action needed. Position already at target."}, {}};
        }

        auto side_str = (action == OrderSide::Buy) ? QString("buy") : QString("sell");
        try {
            auto paper_order = pt_place_order(account.paper_portfolio_id, paper_sym,
                side_str, "market", quantity, std::nullopt, std::nullopt, false);
            pt_fill_order(paper_order.id, 1000.0);
            return {true, SmartOrderResult{true, paper_order.id, action, quantity,
                QString("Paper: adjusted from %1 to %2").arg(current).arg(target)}, {}};
        } catch (const std::exception& e) {
            return {false, std::nullopt, QString("Paper smart order failed: %1").arg(e.what())};
        }
    }

    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (!broker)
        return {false, std::nullopt, "Broker not found: " + account.broker_id};

    auto creds = AccountManager::instance().load_credentials(account_id);
    return SmartOrderEngine::instance().execute(broker, creds, order);
}

ApiResponse<QVector<BrokerQuote>> UnifiedTrading::get_multi_quotes(
    const QString& account_id,
    const QVector<QPair<QString, QString>>& symbols)
{
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return {false, std::nullopt, "Account not found: " + account_id};

    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (!broker)
        return {false, std::nullopt, "Broker not found: " + account.broker_id};

    auto creds = AccountManager::instance().load_credentials(account_id);
    return broker->get_multi_quotes(creds, symbols);
}

ApiResponse<MarketDepth> UnifiedTrading::get_market_depth(
    const QString& account_id,
    const QString& symbol, const QString& exchange)
{
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty())
        return {false, std::nullopt, "Account not found: " + account_id};

    auto* broker = BrokerRegistry::instance().get(account.broker_id);
    if (!broker)
        return {false, std::nullopt, "Broker not found: " + account.broker_id};

    auto creds = AccountManager::instance().load_credentials(account_id);
    return broker->get_market_depth(creds, symbol, exchange);
}

// ============================================================================
// Basket & Split Orders
// ============================================================================

void UnifiedTrading::place_basket_orders(const QString& account_id, const BasketOrderRequest& basket,
                                         std::function<void(const BasketOrderResult&)> callback) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty()) {
        BasketOrderResult result;
        result.total = basket.orders.size();
        for (const auto& order : basket.orders) {
            result.results.append({order.symbol, order.exchange, false, {},
                                   "Account not found: " + account_id});
            result.failed++;
        }
        if (callback)
            callback(result);
        return;
    }

    const bool is_paper = (account.trading_mode == "paper");

    // Resolve broker + credentials up front on the calling thread (consistent
    // with the existing place_live_order_for_account helper).
    IBroker* broker = nullptr;
    BrokerCredentials creds;
    QString paper_portfolio_id = account.paper_portfolio_id;
    if (!is_paper) {
        broker = BrokerRegistry::instance().get(account.broker_id);
        if (!broker) {
            BasketOrderResult result;
            result.total = basket.orders.size();
            for (const auto& order : basket.orders) {
                result.results.append({order.symbol, order.exchange, false, {},
                                       "Broker not found: " + account.broker_id});
                result.failed++;
            }
            if (callback)
                callback(result);
            return;
        }
        creds = AccountManager::instance().load_credentials(account_id);
        if (creds.access_token.isEmpty()) {
            BasketOrderResult result;
            result.total = basket.orders.size();
            for (const auto& order : basket.orders) {
                result.results.append({order.symbol, order.exchange, false, {},
                                       "No credentials for account " + account.display_name +
                                           ". Please authenticate."});
                result.failed++;
            }
            if (callback)
                callback(result);
            return;
        }
    }

    // Order BUY legs first so that a basket which both buys and sells uses the
    // bought collateral before selling. stable_partition keeps relative order.
    QVector<UnifiedOrder> orders = basket.orders;
    std::stable_partition(orders.begin(), orders.end(),
                          [](const UnifiedOrder& o) { return o.side == OrderSide::Buy; });

    QPointer<UnifiedTrading> self = this;
    (void)QtConcurrent::run([self, orders, is_paper, broker, creds, paper_portfolio_id, callback]() {
        BasketOrderResult result;
        result.total = orders.size();

        constexpr int kBatchSize = 10;
        for (int i = 0; i < orders.size(); ++i) {
            // Pause between batches to respect broker rate limits.
            if (i > 0 && (i % kBatchSize) == 0)
                QThread::msleep(1000);

            const UnifiedOrder& order = orders.at(i);
            BasketOrderResult::OrderResult r;
            r.symbol = order.symbol;
            r.exchange = order.exchange;

            if (is_paper) {
                // Store the BARE symbol + forward exchange/product (same contract as
                // place_paper_order_for_account) so each leg marks to market and
                // squares off correctly. Market legs fill at the supplied price (no
                // 1000.0 sentinel — a market leg with no price fails cleanly); limit/
                // stop legs go to the matcher to fill on a real touch.
                const QString side_str = order_side_str(order.side);
                const QString type_str = order_type_str(order.order_type);
                const QString product = product_to_broker_str(order.product_type);
                const bool is_market = (order.order_type == OrderType::Market);
                std::optional<double> price_opt;
                if (order.price > 0)
                    price_opt = order.price;
                std::optional<double> stop_opt;
                if (order.stop_price > 0)
                    stop_opt = order.stop_price;
                if (is_market && !price_opt) {
                    r.success = false;
                    r.error = "No live price for market fill";
                } else {
                    try {
                        auto paper_order = pt_place_order(paper_portfolio_id, order.symbol, side_str, type_str,
                                                          order.quantity, price_opt, stop_opt, false,
                                                          order.exchange, product);
                        if (is_market)
                            pt_fill_order(paper_order.id, *price_opt);
                        else
                            OrderMatcher::instance().add_order(paper_order);
                        r.success = true;
                        r.order_id = paper_order.id;
                    } catch (const std::exception& e) {
                        r.success = false;
                        r.error = QString("Paper order failed: %1").arg(e.what());
                    }
                }
            } else {
                OrderPlaceResponse resp = broker->place_order(creds, order);
                r.success = resp.success;
                r.order_id = resp.order_id;
                r.error = resp.error;
            }

            if (r.success)
                result.successful++;
            else
                result.failed++;
            result.results.append(r);
        }

        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, result, callback]() {
                if (callback)
                    callback(result);
            },
            Qt::QueuedConnection);
    });
}

void UnifiedTrading::place_split_orders(const QString& account_id, const SplitOrderRequest& request,
                                        std::function<void(const SplitOrderResult&)> callback) {
    auto account = AccountManager::instance().get_account(account_id);
    if (account.account_id.isEmpty()) {
        SplitOrderResult result;
        result.results.append({request.base_order.symbol, request.base_order.exchange, false, {},
                               "Account not found: " + account_id});
        result.chunks_failed++;
        if (callback)
            callback(result);
        return;
    }

    if (request.split_size <= 0) {
        SplitOrderResult result;
        result.results.append({request.base_order.symbol, request.base_order.exchange, false, {},
                               "Invalid split size"});
        result.chunks_failed++;
        if (callback)
            callback(result);
        return;
    }

    const int total = static_cast<int>(request.base_order.quantity);
    const int split_size = request.split_size;
    const int num_full = total / split_size;
    const int remainder = total % split_size;
    const int total_orders = num_full + (remainder > 0 ? 1 : 0);

    if (total_orders > 100) {
        SplitOrderResult result;
        result.results.append({request.base_order.symbol, request.base_order.exchange, false, {},
                               QString("Too many chunks (%1). Maximum is 100.").arg(total_orders)});
        result.chunks_failed++;
        if (callback)
            callback(result);
        return;
    }

    const bool is_paper = (account.trading_mode == "paper");

    IBroker* broker = nullptr;
    BrokerCredentials creds;
    QString paper_portfolio_id = account.paper_portfolio_id;
    if (!is_paper) {
        broker = BrokerRegistry::instance().get(account.broker_id);
        if (!broker) {
            SplitOrderResult result;
            result.results.append({request.base_order.symbol, request.base_order.exchange, false, {},
                                   "Broker not found: " + account.broker_id});
            result.chunks_failed++;
            if (callback)
                callback(result);
            return;
        }
        creds = AccountManager::instance().load_credentials(account_id);
        if (creds.access_token.isEmpty()) {
            SplitOrderResult result;
            result.results.append({request.base_order.symbol, request.base_order.exchange, false, {},
                                   "No credentials for account " + account.display_name +
                                       ". Please authenticate."});
            result.chunks_failed++;
            if (callback)
                callback(result);
            return;
        }
    }

    // Build the per-chunk quantity list.
    QVector<int> chunk_qtys;
    chunk_qtys.reserve(total_orders);
    for (int i = 0; i < num_full; ++i)
        chunk_qtys.append(split_size);
    if (remainder > 0)
        chunk_qtys.append(remainder);

    UnifiedOrder base = request.base_order;
    const int delay_ms = request.delay_between_ms;

    QPointer<UnifiedTrading> self = this;
    (void)QtConcurrent::run([self, base, chunk_qtys, delay_ms, is_paper, broker, creds,
                       paper_portfolio_id, callback]() {
        SplitOrderResult result;

        for (int i = 0; i < chunk_qtys.size(); ++i) {
            if (i > 0)
                QThread::msleep(delay_ms);

            UnifiedOrder chunk = base;
            chunk.quantity = chunk_qtys.at(i);

            BasketOrderResult::OrderResult r;
            r.symbol = chunk.symbol;
            r.exchange = chunk.exchange;

            if (is_paper) {
                // Bare symbol + forwarded exchange/product (same contract as the
                // keystone) so split chunks mark + square off; no 1000.0 sentinel.
                const QString side_str = order_side_str(chunk.side);
                const QString type_str = order_type_str(chunk.order_type);
                const QString product = product_to_broker_str(chunk.product_type);
                const bool is_market = (chunk.order_type == OrderType::Market);
                std::optional<double> price_opt;
                if (chunk.price > 0)
                    price_opt = chunk.price;
                std::optional<double> stop_opt;
                if (chunk.stop_price > 0)
                    stop_opt = chunk.stop_price;
                if (is_market && !price_opt) {
                    r.success = false;
                    r.error = "No live price for market fill";
                } else {
                    try {
                        auto paper_order = pt_place_order(paper_portfolio_id, chunk.symbol, side_str, type_str,
                                                          chunk.quantity, price_opt, stop_opt, false,
                                                          chunk.exchange, product);
                        if (is_market)
                            pt_fill_order(paper_order.id, *price_opt);
                        else
                            OrderMatcher::instance().add_order(paper_order);
                        r.success = true;
                        r.order_id = paper_order.id;
                    } catch (const std::exception& e) {
                        r.success = false;
                        r.error = QString("Paper order failed: %1").arg(e.what());
                    }
                }
            } else {
                OrderPlaceResponse resp = broker->place_order(creds, chunk);
                r.success = resp.success;
                r.order_id = resp.order_id;
                r.error = resp.error;
            }

            if (r.success) {
                result.chunks_successful++;
                result.total_quantity_placed += static_cast<int>(chunk.quantity);
            } else {
                result.chunks_failed++;
            }
            result.results.append(r);
        }

        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, result, callback]() {
                if (callback)
                    callback(result);
            },
            Qt::QueuedConnection);
    });
}

void UnifiedTrading::place_order_auto_split(const QString& account_id, const UnifiedOrder& order,
                                            std::function<void(const SplitOrderResult&)> callback) {
    const int freeze = qty_freeze_limit(order.symbol, order.exchange);

    if (freeze <= 0 || order.quantity <= freeze) {
        // No freeze limit (or within it) — place a single order synchronously
        // via the normal path, then deliver a one-element SplitOrderResult so
        // callers see a uniform shape. place_order's own freeze check is a
        // no-op here because quantity <= freeze.
        UnifiedOrderResponse resp = place_order(account_id, order);
        SplitOrderResult result;
        BasketOrderResult::OrderResult r;
        r.symbol = order.symbol;
        r.exchange = order.exchange;
        r.success = resp.success;
        r.order_id = resp.order_id;
        r.error = resp.success ? QString() : resp.message;
        result.results.append(r);
        if (resp.success) {
            result.chunks_successful = 1;
            result.total_quantity_placed = static_cast<int>(order.quantity);
        } else {
            result.chunks_failed = 1;
        }
        if (callback)
            callback(result);
        return;
    }

    // Over the freeze limit — auto-split into chunks of `freeze`.
    LOG_INFO("UnifiedTrading",
             QString("Auto-splitting %1 %2 (qty %3 > freeze %4) into chunks of %4")
                 .arg(account_id, order.symbol)
                 .arg(static_cast<qint64>(order.quantity))
                 .arg(freeze));
    SplitOrderRequest req;
    req.base_order = order;
    req.split_size = freeze;
    req.delay_between_ms = 100;
    place_split_orders(account_id, req, std::move(callback));
}

} // namespace fincept::trading
