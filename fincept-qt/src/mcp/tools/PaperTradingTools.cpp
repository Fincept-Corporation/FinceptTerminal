// PaperTradingTools.cpp — Paper Trading tab MCP tools

#include "mcp/tools/PaperTradingTools.h"

#include "core/logging/Logger.h"
#include "mcp/ToolSchemaBuilder.h"
#include "trading/OrderMatcher.h"
#include "trading/PaperTrading.h"

#include <algorithm>
#include <cmath>

namespace fincept::mcp::tools {

static constexpr const char* TAG = "PaperTradingTools";

std::vector<ToolDef> get_paper_trading_tools() {
    std::vector<ToolDef> tools;

    // ── pt_create_portfolio ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "pt_create_portfolio";
        t.description = "Create a paper trading portfolio with a starting balance.";
        t.category = "paper-trading";
        t.is_destructive = true; // persistent write; pt_place_order depends on it existing
        t.input_schema.properties = QJsonObject{
            {"name", QJsonObject{{"type", "string"}, {"description", "Portfolio name"}}},
            {"balance", QJsonObject{{"type", "number"}, {"description", "Starting cash balance"}}},
            {"currency", QJsonObject{{"type", "string"}, {"description", "Currency code (default: USD)"}}},
            {"leverage", QJsonObject{{"type", "number"},
                                     {"description", "Max leverage, 1-1000 (default: 1.0)"},
                                     {"minimum", 1},
                                     {"maximum", 1000}}},
            {"fee_rate", QJsonObject{{"type", "number"},
                                     {"description", "Trading fee as a fraction, 0-0.05 (default: 0.001 = 0.1%)"},
                                     {"minimum", 0},
                                     {"maximum", 0.05}}}};
        t.input_schema.required = {"name", "balance"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString name = args["name"].toString().trimmed();
            double balance = args["balance"].toDouble(0.0);
            if (name.isEmpty() || !std::isfinite(balance) || balance <= 0)
                return ToolResult::fail("Missing 'name' or 'balance' must be > 0");

            QString currency = args["currency"].toString("USD");
            double leverage = args["leverage"].toDouble(1.0);
            double fee_rate = args["fee_rate"].toDouble(0.001);
            // Declared bounds are enforced by the schema validator; this guards the engine
            // against NaN/inf and keeps a fee typed as a percent ("0.1" meaning 0.1%) from
            // being taken as a 10% fee per fill.
            if (!std::isfinite(leverage) || leverage < 1.0 || leverage > 1000.0)
                return ToolResult::fail("'leverage' must be between 1 and 1000");
            if (!std::isfinite(fee_rate) || fee_rate < 0.0 || fee_rate > 0.05)
                return ToolResult::fail("'fee_rate' is a fraction between 0 and 0.05 (0.001 = 0.1%)");

            try {
                auto p = trading::pt_create_portfolio(name, balance, currency, leverage, "cross", fee_rate);
                LOG_INFO(TAG, "Created paper portfolio: " + p.id);
                return ToolResult::ok("Portfolio created", QJsonObject{{"id", p.id},
                                                                       {"name", p.name},
                                                                       {"balance", p.balance},
                                                                       {"currency", p.currency},
                                                                       {"leverage", p.leverage},
                                                                       {"fee_rate", p.fee_rate}});
            } catch (const std::exception& e) {
                return ToolResult::fail(e.what());
            }
        };
        tools.push_back(std::move(t));
    }

    // ── pt_list_portfolios ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "pt_list_portfolios";
        t.description = "List all paper trading portfolios.";
        t.category = "paper-trading";
        t.handler = [](const QJsonObject&) -> ToolResult {
            try {
                auto portfolios = trading::pt_list_portfolios();
                QJsonArray result;
                for (const auto& p : portfolios) {
                    result.append(QJsonObject{{"id", p.id},
                                              {"name", p.name},
                                              {"balance", p.balance},
                                              {"initial_balance", p.initial_balance},
                                              {"currency", p.currency},
                                              {"leverage", p.leverage},
                                              {"fee_rate", p.fee_rate},
                                              {"created_at", p.created_at}});
                }
                return ToolResult::ok_data(result);
            } catch (const std::exception& e) {
                return ToolResult::fail(e.what());
            }
        };
        tools.push_back(std::move(t));
    }

    // ── pt_get_portfolio ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "pt_get_portfolio";
        t.description = "Get a specific paper trading portfolio by ID.";
        t.category = "paper-trading";
        t.input_schema.properties =
            QJsonObject{{"portfolio_id", QJsonObject{{"type", "string"}, {"description", "Portfolio ID"}}}};
        t.input_schema.required = {"portfolio_id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString id = args["portfolio_id"].toString();
            if (id.isEmpty())
                return ToolResult::fail("Missing 'portfolio_id'");

            try {
                auto p = trading::pt_get_portfolio(id);
                return ToolResult::ok_data(QJsonObject{{"id", p.id},
                                                       {"name", p.name},
                                                       {"balance", p.balance},
                                                       {"initial_balance", p.initial_balance},
                                                       {"currency", p.currency},
                                                       {"leverage", p.leverage},
                                                       {"fee_rate", p.fee_rate},
                                                       {"created_at", p.created_at}});
            } catch (const std::exception& e) {
                return ToolResult::fail(e.what());
            }
        };
        tools.push_back(std::move(t));
    }

    // ── pt_place_order ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "pt_place_order";
        t.description = "Place a paper trading order. Side must be buy/sell; "
                        "order_type one of market/limit/stop/stop_limit. Quantity > 0. The order is queued with the "
                        "paper matching engine (status 'pending') and fills when a matching price tick arrives — "
                        "check pt_get_orders for the outcome. MARKET orders must also pass `price` = the current "
                        "market price (get it from get_quote / get_ticker first): it is the reference the margin "
                        "check needs, and a market order without it is rejected.";
        t.category = "paper-trading";
        // Phase 6.3: even paper trades should confirm — the LLM's intent may
        // not match the user's. Real-broker tools (when added) will use
        // ExplicitConfirm + is_destructive=true.
        t.auth_required = AuthLevel::Authenticated;
        t.is_destructive = true;
        t.input_schema = ToolSchemaBuilder()
                             .string("portfolio_id", "Portfolio ID")
                             .required()
                             .string("symbol", "Trading symbol (e.g. BTC/USDT)")
                             .required()
                             .length(1, 32)
                             .string("side", "Order side")
                             .required()
                             .enums({"buy", "sell"})
                             .string("order_type", "Order type")
                             .default_str("market")
                             .enums({"market", "limit", "stop", "stop_limit"})
                             .number("quantity", "Order quantity (must be > 0)")
                             .required()
                             .min(0.0)
                             .number("price", "Limit price (required for limit/stop_limit orders); for market orders "
                                              "the current market price, used as the margin reference")
                             .number("stop_price", "Stop trigger price (required for stop/stop_limit)")
                             .boolean("reduce_only", "Only reduce existing position")
                             .default_bool(false)
                             .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString portfolio_id = args["portfolio_id"].toString();
            QString symbol = args["symbol"].toString();
            QString side = args["side"].toString();
            QString order_type = args["order_type"].toString("market");
            double quantity = args["quantity"].toDouble(0.0);
            bool reduce_only = args["reduce_only"].toBool(false);

            if (portfolio_id.isEmpty() || symbol.isEmpty() || side.isEmpty() || quantity <= 0)
                return ToolResult::fail("Missing required: portfolio_id, symbol, side, quantity (>0)");

            std::optional<double> price;
            if (args.contains("price") && args["price"].isDouble())
                price = args["price"].toDouble();

            std::optional<double> stop_price;
            if (args.contains("stop_price") && args["stop_price"].isDouble())
                stop_price = args["stop_price"].toDouble();

            try {
                auto order = trading::pt_place_order(portfolio_id, symbol, side, order_type, quantity, price,
                                                     stop_price, reduce_only);
                // The Crypto/Equity screens make this same hand-off right after
                // pt_place_order(). The engine only stores the order as "pending" in the DB
                // and blocks its margin; it is the in-memory OrderMatcher that decides when
                // it fills (market orders on the next tick, limit/stop when price crosses).
                // Without it an order placed through this tool sat "pending" with its margin
                // blocked until someone cancelled it, and could never fill.
                trading::OrderMatcher::instance().add_order(order);
                // %.4f is printf syntax, not a QString place marker — it left
                // the quantity unformatted, shifted order.id off the end and
                // logged "QString::arg: Argument missing".
                LOG_INFO(TAG, QString("Paper order placed: %1 %2 %3 %4")
                                  .arg(side, symbol)
                                  .arg(quantity, 0, 'f', 4)
                                  .arg(order.id));
                return ToolResult::ok("Order queued with the paper matching engine (it fills when a matching price "
                                      "tick arrives — see pt_get_orders)",
                                      QJsonObject{{"order_id", order.id},
                                                  {"status", order.status},
                                                  {"symbol", order.symbol},
                                                  {"side", order.side},
                                                  {"quantity", order.quantity},
                                                  {"order_type", order.order_type}});
            } catch (const std::exception& e) {
                return ToolResult::fail(e.what());
            }
        };
        tools.push_back(std::move(t));
    }

    // ── pt_cancel_order ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "pt_cancel_order";
        t.description = "Cancel a pending paper trading order.";
        t.category = "paper-trading";
        t.is_destructive = true; // mutation tool — penalise on read-style queries
        t.input_schema.properties =
            QJsonObject{{"order_id", QJsonObject{{"type", "string"}, {"description", "Order ID to cancel"}}}};
        t.input_schema.required = {"order_id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString order_id = args["order_id"].toString();
            if (order_id.isEmpty())
                return ToolResult::fail("Missing 'order_id'");

            try {
                trading::pt_cancel_order(order_id);
                // Every screen pairs the two calls. pt_cancel_order() releases the margin and
                // marks the DB row cancelled, but a copy of the order stays in the
                // in-memory matcher; without this it is only discarded when it later tries
                // (and fails) to fill.
                trading::OrderMatcher::instance().remove_order(order_id);
                return ToolResult::ok("Order cancelled", QJsonObject{{"order_id", order_id}});
            } catch (const std::exception& e) {
                return ToolResult::fail(e.what());
            }
        };
        tools.push_back(std::move(t));
    }

    // ── pt_get_positions ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "pt_get_positions";
        t.description = "Get all open positions for a paper trading portfolio.";
        t.category = "paper-trading";
        t.input_schema.properties =
            QJsonObject{{"portfolio_id", QJsonObject{{"type", "string"}, {"description", "Portfolio ID"}}}};
        t.input_schema.required = {"portfolio_id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString id = args["portfolio_id"].toString();
            if (id.isEmpty())
                return ToolResult::fail("Missing 'portfolio_id'");

            try {
                auto positions = trading::pt_get_positions(id);
                QJsonArray result;
                for (const auto& pos : positions) {
                    result.append(QJsonObject{{"id", pos.id},
                                              {"symbol", pos.symbol},
                                              {"side", pos.side},
                                              {"quantity", pos.quantity},
                                              {"entry_price", pos.entry_price},
                                              {"current_price", pos.current_price},
                                              {"unrealized_pnl", pos.unrealized_pnl},
                                              {"realized_pnl", pos.realized_pnl},
                                              {"leverage", pos.leverage},
                                              {"opened_at", pos.opened_at}});
                }
                return ToolResult::ok_data(result);
            } catch (const std::exception& e) {
                return ToolResult::fail(e.what());
            }
        };
        tools.push_back(std::move(t));
    }

    // ── pt_get_orders ──────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "pt_get_orders";
        t.description = "Get orders for a paper trading portfolio, optionally filtered by status.";
        t.category = "paper-trading";
        t.input_schema.properties = QJsonObject{
            {"portfolio_id", QJsonObject{{"type", "string"}, {"description", "Portfolio ID"}}},
            {"status", QJsonObject{{"type", "string"},
                                   {"description", "Filter: pending, filled, cancelled (optional)"}}},
            {"limit", QJsonObject{{"type", "integer"},
                                  {"description", "Max orders, newest first (default 50, max 500)"},
                                  {"minimum", 1},
                                  {"maximum", 500}}}};
        t.input_schema.required = {"portfolio_id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString id = args["portfolio_id"].toString();
            QString status = args["status"].toString();
            const int limit = std::clamp(args["limit"].toInt(50), 1, 500);
            if (id.isEmpty())
                return ToolResult::fail("Missing 'portfolio_id'");

            try {
                auto orders = trading::pt_get_orders(id, status);
                const auto total = orders.size();
                QJsonArray result;
                for (const auto& o : orders) {
                    if (result.size() >= limit)
                        break;
                    QJsonObject entry{{"id", o.id},
                                      {"symbol", o.symbol},
                                      {"side", o.side},
                                      {"order_type", o.order_type},
                                      {"quantity", o.quantity},
                                      {"filled_qty", o.filled_qty},
                                      {"status", o.status},
                                      {"reduce_only", o.reduce_only},
                                      {"created_at", o.created_at}};
                    if (o.price)
                        entry["price"] = *o.price;
                    if (o.stop_price)
                        entry["stop_price"] = *o.stop_price;
                    if (o.avg_price)
                        entry["avg_price"] = *o.avg_price;
                    result.append(entry);
                }
                if (total > result.size())
                    return ToolResult::ok(QStringLiteral("Showing the %1 newest of %2 orders — filter with `status` "
                                                         "or raise `limit` (max 500).")
                                              .arg(result.size())
                                              .arg(total),
                                          result);
                return ToolResult::ok_data(result);
            } catch (const std::exception& e) {
                return ToolResult::fail(e.what());
            }
        };
        tools.push_back(std::move(t));
    }

    // ── pt_get_stats ───────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "pt_get_stats";
        t.description = "Get trading statistics for a paper portfolio (PnL, win rate, trade counts).";
        t.category = "paper-trading";
        t.input_schema.properties =
            QJsonObject{{"portfolio_id", QJsonObject{{"type", "string"}, {"description", "Portfolio ID"}}}};
        t.input_schema.required = {"portfolio_id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString id = args["portfolio_id"].toString();
            if (id.isEmpty())
                return ToolResult::fail("Missing 'portfolio_id'");

            try {
                auto stats = trading::pt_get_stats(id);
                return ToolResult::ok_data(QJsonObject{{"total_pnl", stats.total_pnl},
                                                       {"win_rate", stats.win_rate},
                                                       {"total_trades", static_cast<qint64>(stats.total_trades)},
                                                       {"winning_trades", static_cast<qint64>(stats.winning_trades)},
                                                       {"losing_trades", static_cast<qint64>(stats.losing_trades)},
                                                       {"largest_win", stats.largest_win},
                                                       {"largest_loss", stats.largest_loss}});
            } catch (const std::exception& e) {
                return ToolResult::fail(e.what());
            }
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
