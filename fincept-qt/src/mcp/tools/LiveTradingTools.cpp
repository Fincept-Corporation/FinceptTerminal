// LiveTradingTools.cpp — Live broker trading MCP tools (Phase 3 §15)
//
// Exposes UnifiedTrading / broker order, account-state, and market-data
// operations to the MCP layer so the AI can place/cancel live orders and read
// account state. All order-mutating tools are is_destructive=true so the
// Phase 6.12 confirmation modal fires before execution.
//
// Account resolution: every tool takes an optional account_id. If omitted and
// exactly one active account exists, that account is used; otherwise the tool
// fails and asks the caller to specify account_id.

#include "mcp/tools/LiveTradingTools.h"

#include "core/logging/Logger.h"
#include "mcp/ToolSchemaBuilder.h"
#include "trading/AccountManager.h"
#include "trading/ActionCenter.h"
#include "trading/BrokerInterface.h"
#include "trading/BrokerRegistry.h"
#include "trading/TradingTypes.h"
#include "trading/UnifiedTrading.h"

namespace fincept::mcp::tools {

static constexpr const char* TAG = "LiveTradingTools";

namespace {

using namespace fincept::trading;

// Resolve the account to operate on. Returns true on success and writes the
// resolved id into `out`. On failure, writes a caller-friendly message into
// `err`. If `arg` is non-empty it must name a real account; if empty, falls
// back to the single active account (else asks the caller to specify one).
bool resolve_account(const QString& arg, QString& out, QString& err) {
    auto& mgr = AccountManager::instance();
    if (!arg.isEmpty()) {
        if (!mgr.has_account(arg)) {
            err = QString("Unknown account_id: %1").arg(arg);
            return false;
        }
        out = arg;
        return true;
    }
    auto active = mgr.active_accounts();
    if (active.isEmpty()) {
        err = "No active broker accounts — connect a broker account first";
        return false;
    }
    if (active.size() > 1) {
        QStringList ids;
        for (const auto& a : active)
            ids.append(QString("%1 (%2)").arg(a.account_id, a.display_name));
        err = "Multiple active accounts — specify account_id. Active: " + ids.join(", ");
        return false;
    }
    out = active.first().account_id;
    return true;
}

// Resolve the IBroker* + credentials for an account, for account-state and
// market-data tools that hit the broker directly (no UnifiedTrading helper).
bool resolve_broker(const QString& account_id, IBroker*& broker, BrokerCredentials& creds, QString& err) {
    auto& mgr = AccountManager::instance();
    auto account = mgr.get_account(account_id);
    broker = BrokerRegistry::instance().get(account.broker_id);
    if (!broker) {
        err = QString("No broker registered for id: %1").arg(account.broker_id);
        return false;
    }
    creds = mgr.load_credentials(account_id);
    return true;
}

OrderSide parse_side(const QString& s) {
    return s.trimmed().toUpper() == "SELL" ? OrderSide::Sell : OrderSide::Buy;
}

// MARKET / LIMIT / SL / SL-M (OpenAlgo price-type vocabulary).
OrderType parse_order_type(const QString& s) {
    const QString u = s.trimmed().toUpper();
    if (u == "LIMIT")
        return OrderType::Limit;
    if (u == "SL" || u == "SL-L" || u == "SLL" || u == "STOP_LIMIT")
        return OrderType::StopLossLimit;
    if (u == "SL-M" || u == "SLM" || u == "STOP" || u == "STOP_MARKET")
        return OrderType::StopLoss;
    return OrderType::Market;
}

// MIS / CNC / NRML / etc. → ProductType. Empty defaults to Intraday (MIS).
ProductType parse_product(const QString& s) {
    const QString u = s.trimmed().toUpper();
    if (u == "CNC" || u == "DELIVERY")
        return ProductType::Delivery;
    if (u == "NRML" || u == "MARGIN")
        return ProductType::Margin;
    if (u == "CO" || u == "COVER")
        return ProductType::CoverOrder;
    if (u == "BO" || u == "BRACKET")
        return ProductType::BracketOrder;
    if (u == "MTF")
        return ProductType::MTF;
    return ProductType::Intraday; // MIS / default
}

QJsonObject order_info_to_json(const BrokerOrderInfo& o) {
    return QJsonObject{{"order_id", o.order_id},
                       {"exchange_order_id", o.exchange_order_id},
                       {"symbol", o.symbol},
                       {"exchange", o.exchange},
                       {"side", o.side},
                       {"order_type", o.order_type},
                       {"product_type", o.product_type},
                       {"quantity", o.quantity},
                       {"price", o.price},
                       {"trigger_price", o.trigger_price},
                       {"filled_qty", o.filled_qty},
                       {"avg_price", o.avg_price},
                       {"status", o.status},
                       {"timestamp", o.timestamp},
                       {"message", o.message}};
}

QJsonObject quote_to_json(const BrokerQuote& q) {
    return QJsonObject{{"symbol", q.symbol},
                       {"ltp", q.ltp},
                       {"open", q.open},
                       {"high", q.high},
                       {"low", q.low},
                       {"close", q.close},
                       {"volume", q.volume},
                       {"change", q.change},
                       {"change_pct", q.change_pct},
                       {"bid", q.bid},
                       {"ask", q.ask},
                       {"bid_size", q.bid_size},
                       {"ask_size", q.ask_size},
                       {"oi", static_cast<double>(q.oi)},
                       {"oi_change_pct", q.oi_change_pct},
                       {"timestamp", static_cast<double>(q.timestamp)}};
}

} // namespace

std::vector<ToolDef> get_live_trading_tools() {
    std::vector<ToolDef> tools;

    // ════════════════════════════════════════════════════════════════════
    // Order management (is_destructive = true — AI must confirm)
    // ════════════════════════════════════════════════════════════════════

    // ── live_place_order ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "live_place_order";
        t.description = "Place a LIVE broker order. Real money. action BUY/SELL; "
                        "order_type MARKET/LIMIT/SL/SL-M; product MIS/CNC/NRML. "
                        "Requires confirmation.";
        t.category = "live-trading";
        t.auth_required = AuthLevel::Authenticated;
        t.is_destructive = true;
        t.input_schema = ToolSchemaBuilder()
            .string("account_id", "Broker account ID (optional if exactly one active account)")
            .string("symbol", "Trading symbol (e.g. SBIN, RELIANCE)").required().length(1, 64)
            .string("exchange", "Exchange (e.g. NSE, BSE, NFO, MCX)").required()
            .string("action", "Order side").required().enums({"BUY", "SELL"})
            .number("quantity", "Order quantity (must be > 0)").required().min(0.0)
            .string("order_type", "Price type").default_str("MARKET")
                .enums({"MARKET", "LIMIT", "SL", "SL-M"})
            .number("price", "Limit price (required for LIMIT / SL)")
            .number("trigger_price", "Trigger price (required for SL / SL-M)")
            .string("product", "Product type").default_str("MIS")
                .enums({"MIS", "CNC", "NRML"})
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString account_id, err;
            if (!resolve_account(args["account_id"].toString(), account_id, err))
                return ToolResult::fail(err);

            QString symbol = args["symbol"].toString().trimmed();
            QString exchange = args["exchange"].toString().trimmed();
            double quantity = args["quantity"].toDouble(0.0);
            if (symbol.isEmpty() || exchange.isEmpty() || quantity <= 0)
                return ToolResult::fail("Missing required: symbol, exchange, quantity (>0)");

            UnifiedOrder order;
            order.symbol = symbol;
            order.exchange = exchange;
            order.side = parse_side(args["action"].toString());
            order.order_type = parse_order_type(args["order_type"].toString("MARKET"));
            order.quantity = quantity;
            order.price = args["price"].toDouble(0.0);
            order.stop_price = args["trigger_price"].toDouble(0.0);
            order.product_type = parse_product(args["product"].toString("MIS"));

            // Semi-Auto gate (headless): the AI is placing a live order, so in
            // Semi-Auto mode queue it for human approval (surfaces in the
            // status-bar Pending Orders popover) instead of sending it.
            if (ActionCenter::instance().should_queue(account_id, "placeorder")) {
                const QString pid = ActionCenter::instance().queue_order(
                    account_id, "placeorder", ActionCenter::serialize_unified_order(order));
                if (pid.isEmpty())
                    return ToolResult::fail("Failed to queue order for approval");
                return ToolResult::ok("Order queued for approval",
                                      QJsonObject{{"pending_id", pid},
                                                  {"account_id", account_id},
                                                  {"status", "queued_for_approval"}});
            }

            auto resp = UnifiedTrading::instance().place_order(account_id, order);
            if (!resp.success)
                return ToolResult::fail(resp.message.isEmpty() ? "Order placement failed" : resp.message);

            LOG_INFO(TAG, QString("Live order placed: %1 %2 %3 -> %4")
                              .arg(args["action"].toString(), symbol)
                              .arg(quantity).arg(resp.order_id));
            return ToolResult::ok("Order placed", QJsonObject{{"order_id", resp.order_id},
                                                              {"account_id", account_id},
                                                              {"symbol", symbol},
                                                              {"exchange", exchange},
                                                              {"mode", resp.mode},
                                                              {"message", resp.message}});
        };
        tools.push_back(std::move(t));
    }

    // ── live_smart_order ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "live_smart_order";
        t.description = "Place a position-aware LIVE smart order. Targets a net "
                        "position_size (positive=long, negative=short, 0=flatten) "
                        "and places the delta order. Requires confirmation.";
        t.category = "live-trading";
        t.auth_required = AuthLevel::Authenticated;
        t.is_destructive = true;
        t.input_schema = ToolSchemaBuilder()
            .string("account_id", "Broker account ID (optional if exactly one active account)")
            .string("symbol", "Trading symbol").required().length(1, 64)
            .string("exchange", "Exchange (e.g. NSE, NFO)").required()
            .number("position_size", "Target net position (positive=long, negative=short, 0=flatten)").required()
            .string("order_type", "Price type").default_str("MARKET")
                .enums({"MARKET", "LIMIT", "SL", "SL-M"})
            .number("price", "Limit price (required for LIMIT / SL)")
            .string("product", "Product type").default_str("MIS")
                .enums({"MIS", "CNC", "NRML"})
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString account_id, err;
            if (!resolve_account(args["account_id"].toString(), account_id, err))
                return ToolResult::fail(err);

            QString symbol = args["symbol"].toString().trimmed();
            QString exchange = args["exchange"].toString().trimmed();
            if (symbol.isEmpty() || exchange.isEmpty())
                return ToolResult::fail("Missing required: symbol, exchange");
            if (!args.contains("position_size"))
                return ToolResult::fail("Missing required: position_size");

            SmartOrder order;
            order.symbol = symbol;
            order.exchange = exchange;
            order.position_size = args["position_size"].toDouble(0.0);
            order.order_type = parse_order_type(args["order_type"].toString("MARKET"));
            order.price = args["price"].toDouble(0.0);
            order.product_type = parse_product(args["product"].toString("MIS"));

            auto resp = UnifiedTrading::instance().place_smart_order(account_id, order);
            if (!resp.success)
                return ToolResult::fail(resp.error.isEmpty() ? "Smart order failed" : resp.error);

            const auto& r = resp.data.value();
            LOG_INFO(TAG, QString("Live smart order: %1 -> action_taken=%2 %3")
                              .arg(symbol).arg(r.action_taken).arg(r.order_id));
            return ToolResult::ok(r.message.isEmpty() ? "Smart order processed" : r.message,
                                  QJsonObject{{"account_id", account_id},
                                              {"symbol", symbol},
                                              {"action_taken", r.action_taken},
                                              {"order_id", r.order_id},
                                              {"executed_action", order_side_str(r.executed_action)},
                                              {"executed_quantity", r.executed_quantity},
                                              {"message", r.message}});
        };
        tools.push_back(std::move(t));
    }

    // ── live_cancel_order ──────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "live_cancel_order";
        t.description = "Cancel a single open LIVE order by order_id. Requires confirmation.";
        t.category = "live-trading";
        t.auth_required = AuthLevel::Authenticated;
        t.is_destructive = true;
        t.input_schema = ToolSchemaBuilder()
            .string("account_id", "Broker account ID (optional if exactly one active account)")
            .string("order_id", "Broker order ID to cancel").required()
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString account_id, err;
            if (!resolve_account(args["account_id"].toString(), account_id, err))
                return ToolResult::fail(err);

            QString order_id = args["order_id"].toString().trimmed();
            if (order_id.isEmpty())
                return ToolResult::fail("Missing 'order_id'");

            auto resp = UnifiedTrading::instance().cancel_order(account_id, order_id);
            if (!resp.success)
                return ToolResult::fail(resp.message.isEmpty() ? "Cancel failed" : resp.message);

            LOG_INFO(TAG, QString("Live order cancelled: %1 (%2)").arg(order_id, account_id));
            return ToolResult::ok("Order cancelled", QJsonObject{{"order_id", order_id},
                                                                 {"account_id", account_id},
                                                                 {"message", resp.message}});
        };
        tools.push_back(std::move(t));
    }

    // ── live_cancel_all_orders ─────────────────────────────────────────
    {
        ToolDef t;
        t.name = "live_cancel_all_orders";
        t.description = "Cancel ALL open LIVE orders for an account. Requires confirmation.";
        t.category = "live-trading";
        t.auth_required = AuthLevel::Authenticated;
        t.is_destructive = true;
        t.input_schema = ToolSchemaBuilder()
            .string("account_id", "Broker account ID (optional if exactly one active account)")
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString account_id, err;
            if (!resolve_account(args["account_id"].toString(), account_id, err))
                return ToolResult::fail(err);

            auto resp = UnifiedTrading::instance().cancel_all_orders(account_id);
            if (!resp.success)
                return ToolResult::fail(resp.error.isEmpty() ? "Cancel-all failed" : resp.error);

            const auto& r = resp.data.value();
            QJsonArray canceled;
            for (const auto& id : r.canceled_order_ids)
                canceled.append(id);
            QJsonArray failed;
            for (const auto& f : r.failed)
                failed.append(QJsonObject{{"order_id", f.first}, {"error", f.second}});

            LOG_INFO(TAG, QString("Live cancel-all: %1/%2 cancelled (%3)")
                              .arg(canceled.size()).arg(r.total_attempted).arg(account_id));
            return ToolResult::ok("Cancel-all complete", QJsonObject{{"account_id", account_id},
                                                                     {"total_attempted", r.total_attempted},
                                                                     {"canceled_count", canceled.size()},
                                                                     {"failed_count", failed.size()},
                                                                     {"canceled_order_ids", canceled},
                                                                     {"failed", failed}});
        };
        tools.push_back(std::move(t));
    }

    // ── live_close_position ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "live_close_position";
        t.description = "Close a single LIVE position by symbol/exchange (places a "
                        "market counter-order). Requires confirmation.";
        t.category = "live-trading";
        t.auth_required = AuthLevel::Authenticated;
        t.is_destructive = true;
        t.input_schema = ToolSchemaBuilder()
            .string("account_id", "Broker account ID (optional if exactly one active account)")
            .string("symbol", "Symbol of the position to close").required().length(1, 64)
            .string("exchange", "Exchange of the position").required()
            .string("product", "Product type filter (optional: MIS/CNC/NRML)")
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString account_id, err;
            if (!resolve_account(args["account_id"].toString(), account_id, err))
                return ToolResult::fail(err);

            QString symbol = args["symbol"].toString().trimmed();
            QString exchange = args["exchange"].toString().trimmed();
            QString product = args["product"].toString().trimmed();
            if (symbol.isEmpty() || exchange.isEmpty())
                return ToolResult::fail("Missing required: symbol, exchange");

            auto resp = UnifiedTrading::instance().close_position(account_id, symbol, exchange, product);
            if (!resp.success)
                return ToolResult::fail(resp.error.isEmpty() ? "Close position failed" : resp.error);

            const auto& r = resp.data.value();
            LOG_INFO(TAG, QString("Live position closed: %1 %2 -> %3").arg(symbol, exchange, r.order_id));
            return ToolResult::ok("Position closed", QJsonObject{{"account_id", account_id},
                                                                {"symbol", symbol},
                                                                {"exchange", exchange},
                                                                {"order_id", r.order_id}});
        };
        tools.push_back(std::move(t));
    }

    // ── live_close_all_positions ───────────────────────────────────────
    {
        ToolDef t;
        t.name = "live_close_all_positions";
        t.description = "Square off ALL open LIVE positions for an account (market "
                        "counter-orders). Requires confirmation.";
        t.category = "live-trading";
        t.auth_required = AuthLevel::Authenticated;
        t.is_destructive = true;
        t.input_schema = ToolSchemaBuilder()
            .string("account_id", "Broker account ID (optional if exactly one active account)")
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString account_id, err;
            if (!resolve_account(args["account_id"].toString(), account_id, err))
                return ToolResult::fail(err);

            auto resp = UnifiedTrading::instance().close_all_positions(account_id);
            if (!resp.success)
                return ToolResult::fail(resp.error.isEmpty() ? "Close-all failed" : resp.error);

            const auto& r = resp.data.value();
            QJsonArray closed;
            for (const auto& s : r.closed_symbols)
                closed.append(s);
            QJsonArray failed;
            for (const auto& f : r.failed)
                failed.append(QJsonObject{{"symbol", f.first}, {"error", f.second}});

            LOG_INFO(TAG, QString("Live close-all: %1/%2 closed (%3)")
                              .arg(closed.size()).arg(r.total_positions).arg(account_id));
            return ToolResult::ok("Close-all complete", QJsonObject{{"account_id", account_id},
                                                                    {"total_positions", r.total_positions},
                                                                    {"closed_count", closed.size()},
                                                                    {"failed_count", failed.size()},
                                                                    {"closed_symbols", closed},
                                                                    {"failed", failed}});
        };
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // Account state (read-only — is_destructive = false)
    // ════════════════════════════════════════════════════════════════════

    // ── live_get_positions ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "live_get_positions";
        t.description = "Get current open positions for a live broker account.";
        t.category = "live-trading";
        t.input_schema = ToolSchemaBuilder()
            .string("account_id", "Broker account ID (optional if exactly one active account)")
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString account_id, err;
            if (!resolve_account(args["account_id"].toString(), account_id, err))
                return ToolResult::fail(err);

            IBroker* broker = nullptr;
            BrokerCredentials creds;
            if (!resolve_broker(account_id, broker, creds, err))
                return ToolResult::fail(err);

            auto resp = broker->get_positions(creds);
            if (!resp.success)
                return ToolResult::fail(resp.error.isEmpty() ? "Failed to fetch positions" : resp.error);

            QJsonArray result;
            for (const auto& p : resp.data.value_or(QVector<BrokerPosition>{})) {
                result.append(QJsonObject{{"symbol", p.symbol},
                                          {"exchange", p.exchange},
                                          {"product_type", p.product_type},
                                          {"quantity", p.quantity},
                                          {"avg_price", p.avg_price},
                                          {"ltp", p.ltp},
                                          {"pnl", p.pnl},
                                          {"pnl_pct", p.pnl_pct},
                                          {"day_pnl", p.day_pnl},
                                          {"side", p.side}});
            }
            return ToolResult::ok_data(result);
        };
        tools.push_back(std::move(t));
    }

    // ── live_get_holdings ──────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "live_get_holdings";
        t.description = "Get long-term holdings (demat) for a live broker account.";
        t.category = "live-trading";
        t.input_schema = ToolSchemaBuilder()
            .string("account_id", "Broker account ID (optional if exactly one active account)")
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString account_id, err;
            if (!resolve_account(args["account_id"].toString(), account_id, err))
                return ToolResult::fail(err);

            IBroker* broker = nullptr;
            BrokerCredentials creds;
            if (!resolve_broker(account_id, broker, creds, err))
                return ToolResult::fail(err);

            auto resp = broker->get_holdings(creds);
            if (!resp.success)
                return ToolResult::fail(resp.error.isEmpty() ? "Failed to fetch holdings" : resp.error);

            QJsonArray result;
            for (const auto& h : resp.data.value_or(QVector<BrokerHolding>{})) {
                result.append(QJsonObject{{"symbol", h.symbol},
                                          {"exchange", h.exchange},
                                          {"quantity", h.quantity},
                                          {"avg_price", h.avg_price},
                                          {"ltp", h.ltp},
                                          {"pnl", h.pnl},
                                          {"pnl_pct", h.pnl_pct},
                                          {"invested_value", h.invested_value},
                                          {"current_value", h.current_value}});
            }
            return ToolResult::ok_data(result);
        };
        tools.push_back(std::move(t));
    }

    // ── live_get_orders ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "live_get_orders";
        t.description = "Get the order book (all orders for the day) for a live broker account.";
        t.category = "live-trading";
        t.input_schema = ToolSchemaBuilder()
            .string("account_id", "Broker account ID (optional if exactly one active account)")
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString account_id, err;
            if (!resolve_account(args["account_id"].toString(), account_id, err))
                return ToolResult::fail(err);

            IBroker* broker = nullptr;
            BrokerCredentials creds;
            if (!resolve_broker(account_id, broker, creds, err))
                return ToolResult::fail(err);

            auto resp = broker->get_orders(creds);
            if (!resp.success)
                return ToolResult::fail(resp.error.isEmpty() ? "Failed to fetch orders" : resp.error);

            QJsonArray result;
            for (const auto& o : resp.data.value_or(QVector<BrokerOrderInfo>{}))
                result.append(order_info_to_json(o));
            return ToolResult::ok_data(result);
        };
        tools.push_back(std::move(t));
    }

    // ── live_get_trades ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "live_get_trades";
        t.description = "Get the trade book (executed fills) for a live broker account.";
        t.category = "live-trading";
        t.input_schema = ToolSchemaBuilder()
            .string("account_id", "Broker account ID (optional if exactly one active account)")
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString account_id, err;
            if (!resolve_account(args["account_id"].toString(), account_id, err))
                return ToolResult::fail(err);

            IBroker* broker = nullptr;
            BrokerCredentials creds;
            if (!resolve_broker(account_id, broker, creds, err))
                return ToolResult::fail(err);

            auto resp = broker->get_trade_book(creds);
            if (!resp.success)
                return ToolResult::fail(resp.error.isEmpty() ? "Failed to fetch trades" : resp.error);

            // Trade book is broker-specific JSON — pass through raw.
            return ToolResult::ok_data(resp.data.value_or(QJsonObject{}));
        };
        tools.push_back(std::move(t));
    }

    // ── live_get_funds ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "live_get_funds";
        t.description = "Get available balance, used margin, and total balance for a live broker account.";
        t.category = "live-trading";
        t.input_schema = ToolSchemaBuilder()
            .string("account_id", "Broker account ID (optional if exactly one active account)")
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString account_id, err;
            if (!resolve_account(args["account_id"].toString(), account_id, err))
                return ToolResult::fail(err);

            IBroker* broker = nullptr;
            BrokerCredentials creds;
            if (!resolve_broker(account_id, broker, creds, err))
                return ToolResult::fail(err);

            auto resp = broker->get_funds(creds);
            if (!resp.success)
                return ToolResult::fail(resp.error.isEmpty() ? "Failed to fetch funds" : resp.error);

            const auto& f = resp.data.value();
            return ToolResult::ok_data(QJsonObject{{"available_balance", f.available_balance},
                                                   {"used_margin", f.used_margin},
                                                   {"total_balance", f.total_balance},
                                                   {"collateral", f.collateral}});
        };
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // Market data (read-only — is_destructive = false)
    // ════════════════════════════════════════════════════════════════════

    // ── live_get_quote ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "live_get_quote";
        t.description = "Get a single live quote (LTP, bid/ask, OHLC, volume, OI) for a symbol.";
        t.category = "live-trading";
        t.input_schema = ToolSchemaBuilder()
            .string("account_id", "Broker account ID (optional if exactly one active account)")
            .string("symbol", "Trading symbol").required().length(1, 64)
            .string("exchange", "Exchange (e.g. NSE, NFO)").required()
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString account_id, err;
            if (!resolve_account(args["account_id"].toString(), account_id, err))
                return ToolResult::fail(err);

            QString symbol = args["symbol"].toString().trimmed();
            QString exchange = args["exchange"].toString().trimmed();
            if (symbol.isEmpty() || exchange.isEmpty())
                return ToolResult::fail("Missing required: symbol, exchange");

            auto resp = UnifiedTrading::instance().get_multi_quotes(account_id, {{symbol, exchange}});
            if (!resp.success)
                return ToolResult::fail(resp.error.isEmpty() ? "Failed to fetch quote" : resp.error);

            const auto& quotes = resp.data.value();
            if (quotes.isEmpty())
                return ToolResult::fail("No quote returned for " + symbol);
            return ToolResult::ok_data(quote_to_json(quotes.first()));
        };
        tools.push_back(std::move(t));
    }

    // ── live_get_multi_quotes ──────────────────────────────────────────
    {
        ToolDef t;
        t.name = "live_get_multi_quotes";
        t.description = "Get live quotes for multiple symbols in one call. Each entry "
                        "is \"EXCHANGE:SYMBOL\" (e.g. NSE:SBIN) or just SYMBOL.";
        t.category = "live-trading";
        t.input_schema = ToolSchemaBuilder()
            .string("account_id", "Broker account ID (optional if exactly one active account)")
            .array("symbols", "Symbols as \"EXCHANGE:SYMBOL\" or \"SYMBOL\"",
                   QJsonObject{{"type", "string"}}).required()
            .string("exchange", "Default exchange for entries without an EXCHANGE: prefix")
                .default_str("NSE")
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString account_id, err;
            if (!resolve_account(args["account_id"].toString(), account_id, err))
                return ToolResult::fail(err);

            QString default_exchange = args["exchange"].toString("NSE").trimmed();
            QVector<QPair<QString, QString>> pairs;
            for (const auto& v : args["symbols"].toArray()) {
                QString entry = v.toString().trimmed();
                if (entry.isEmpty())
                    continue;
                int colon = entry.indexOf(':');
                if (colon > 0)
                    pairs.append({entry.mid(colon + 1).trimmed(), entry.left(colon).trimmed()});
                else
                    pairs.append({entry, default_exchange});
            }
            if (pairs.isEmpty())
                return ToolResult::fail("Missing or empty 'symbols'");

            auto resp = UnifiedTrading::instance().get_multi_quotes(account_id, pairs);
            if (!resp.success)
                return ToolResult::fail(resp.error.isEmpty() ? "Failed to fetch quotes" : resp.error);

            QJsonArray result;
            for (const auto& q : resp.data.value())
                result.append(quote_to_json(q));
            return ToolResult::ok_data(result);
        };
        tools.push_back(std::move(t));
    }

    // ── live_get_depth ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "live_get_depth";
        t.description = "Get Level-2 market depth (bid/ask ladder) for a symbol.";
        t.category = "live-trading";
        t.input_schema = ToolSchemaBuilder()
            .string("account_id", "Broker account ID (optional if exactly one active account)")
            .string("symbol", "Trading symbol").required().length(1, 64)
            .string("exchange", "Exchange (e.g. NSE, NFO)").required()
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString account_id, err;
            if (!resolve_account(args["account_id"].toString(), account_id, err))
                return ToolResult::fail(err);

            QString symbol = args["symbol"].toString().trimmed();
            QString exchange = args["exchange"].toString().trimmed();
            if (symbol.isEmpty() || exchange.isEmpty())
                return ToolResult::fail("Missing required: symbol, exchange");

            auto resp = UnifiedTrading::instance().get_market_depth(account_id, symbol, exchange);
            if (!resp.success)
                return ToolResult::fail(resp.error.isEmpty() ? "Failed to fetch depth" : resp.error);

            const auto& d = resp.data.value();
            QJsonArray bids, asks;
            for (const auto& lvl : d.bids)
                bids.append(QJsonObject{{"price", lvl.price}, {"quantity", lvl.quantity}, {"orders", lvl.orders}});
            for (const auto& lvl : d.asks)
                asks.append(QJsonObject{{"price", lvl.price}, {"quantity", lvl.quantity}, {"orders", lvl.orders}});

            return ToolResult::ok_data(QJsonObject{{"symbol", d.symbol},
                                                   {"exchange", d.exchange},
                                                   {"ltp", d.ltp},
                                                   {"volume", d.volume},
                                                   {"oi", d.oi},
                                                   {"bids", bids},
                                                   {"asks", asks}});
        };
        tools.push_back(std::move(t));
    }

    // ── live_get_option_chain ──────────────────────────────────────────
    {
        ToolDef t;
        t.name = "live_get_option_chain";
        t.description = "Get the option chain (CE/PE quotes per strike) for an underlying and expiry.";
        t.category = "live-trading";
        t.input_schema = ToolSchemaBuilder()
            .string("account_id", "Broker account ID (optional if exactly one active account)")
            .string("underlying", "Underlying symbol (e.g. NIFTY, BANKNIFTY)").required().length(1, 64)
            .string("exchange", "Exchange (e.g. NFO, BFO)").required()
            .string("expiry", "Expiry date (broker format, e.g. 2026-05-29)").required()
            .integer("strike_count", "Number of strikes around ATM (0 = broker default)")
                .default_int(0).min(0.0)
            .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString account_id, err;
            if (!resolve_account(args["account_id"].toString(), account_id, err))
                return ToolResult::fail(err);

            QString underlying = args["underlying"].toString().trimmed();
            QString exchange = args["exchange"].toString().trimmed();
            QString expiry = args["expiry"].toString().trimmed();
            int strike_count = args["strike_count"].toInt(0);
            if (underlying.isEmpty() || exchange.isEmpty() || expiry.isEmpty())
                return ToolResult::fail("Missing required: underlying, exchange, expiry");

            IBroker* broker = nullptr;
            BrokerCredentials creds;
            if (!resolve_broker(account_id, broker, creds, err))
                return ToolResult::fail(err);

            auto resp = broker->get_option_chain(creds, underlying, exchange, expiry, strike_count);
            if (!resp.success)
                return ToolResult::fail(resp.error.isEmpty() ? "Failed to fetch option chain" : resp.error);

            QJsonArray result;
            for (const auto& e : resp.data.value_or(QVector<OptionChainEntry>{})) {
                result.append(QJsonObject{{"strike_price", e.strike_price},
                                          {"label", e.label},
                                          {"ce_symbol", e.ce_symbol},
                                          {"ce_quote", quote_to_json(e.ce_quote)},
                                          {"pe_symbol", e.pe_symbol},
                                          {"pe_quote", quote_to_json(e.pe_quote)}});
            }
            return ToolResult::ok_data(QJsonObject{{"underlying", underlying},
                                                   {"exchange", exchange},
                                                   {"expiry", expiry},
                                                   {"chain", result}});
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
