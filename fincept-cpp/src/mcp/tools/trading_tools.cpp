// Trading MCP Tools — paper trading operations

#include "trading_tools.h"
#include "../../trading/paper_trading.h"
#include "../../core/logger.h"

namespace fincept::mcp::tools {

static constexpr const char* TAG_TRADING = "TradingTools";

std::vector<ToolDef> get_trading_tools() {
    std::vector<ToolDef> tools;

    // ── pt_create_portfolio ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "pt_create_portfolio";
        t.description = "Create a paper trading portfolio with a starting balance.";
        t.category = "trading";
        t.input_schema.properties = {
            {"name", {{"type", "string"}, {"description", "Portfolio name"}}},
            {"balance", {{"type", "number"}, {"description", "Starting cash balance"}}},
            {"currency", {{"type", "string"}, {"description", "Currency code (default: USD)"}}},
            {"leverage", {{"type", "number"}, {"description", "Max leverage (default: 1.0)"}}},
            {"fee_rate", {{"type", "number"}, {"description", "Trading fee rate (default: 0.001)"}}}
        };
        t.input_schema.required = {"name", "balance"};
        t.handler = [](const json& args) -> ToolResult {
            std::string name = args.value("name", "");
            double balance = args.value("balance", 0.0);
            if (name.empty() || balance <= 0)
                return ToolResult::fail("Missing 'name' or 'balance' must be > 0");

            std::string currency = args.value("currency", "USD");
            double leverage = args.value("leverage", 1.0);
            double fee_rate = args.value("fee_rate", 0.001);

            try {
                auto p = trading::pt_create_portfolio(name, balance, currency, leverage, "cross", fee_rate);
                LOG_INFO(TAG_TRADING, "Created paper portfolio: %s", p.id.c_str());
                return ToolResult::ok("Portfolio created", {
                    {"id", p.id}, {"name", p.name}, {"balance", p.balance},
                    {"currency", p.currency}, {"leverage", p.leverage}, {"fee_rate", p.fee_rate}
                });
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
        t.category = "trading";
        t.handler = [](const json&) -> ToolResult {
            try {
                auto portfolios = trading::pt_list_portfolios();
                json result = json::array();
                for (const auto& p : portfolios) {
                    result.push_back({
                        {"id", p.id}, {"name", p.name},
                        {"balance", p.balance}, {"initial_balance", p.initial_balance},
                        {"currency", p.currency}, {"leverage", p.leverage},
                        {"fee_rate", p.fee_rate}, {"created_at", p.created_at}
                    });
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
        t.category = "trading";
        t.input_schema.properties = {
            {"portfolio_id", {{"type", "string"}, {"description", "Portfolio ID"}}}
        };
        t.input_schema.required = {"portfolio_id"};
        t.handler = [](const json& args) -> ToolResult {
            std::string id = args.value("portfolio_id", "");
            if (id.empty()) return ToolResult::fail("Missing 'portfolio_id'");

            try {
                auto p = trading::pt_get_portfolio(id);
                return ToolResult::ok_data({
                    {"id", p.id}, {"name", p.name},
                    {"balance", p.balance}, {"initial_balance", p.initial_balance},
                    {"currency", p.currency}, {"leverage", p.leverage},
                    {"fee_rate", p.fee_rate}, {"created_at", p.created_at}
                });
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
        t.description = "Place a paper trading order. Supports market, limit, stop, stop_limit.";
        t.category = "trading";
        t.input_schema.properties = {
            {"portfolio_id", {{"type", "string"}, {"description", "Portfolio ID"}}},
            {"symbol", {{"type", "string"}, {"description", "Trading symbol (e.g. BTC/USDT)"}}},
            {"side", {{"type", "string"}, {"description", "buy or sell"}}},
            {"order_type", {{"type", "string"}, {"description", "market, limit, stop, stop_limit"}}},
            {"quantity", {{"type", "number"}, {"description", "Order quantity"}}},
            {"price", {{"type", "number"}, {"description", "Limit price (required for limit/stop_limit)"}}},
            {"stop_price", {{"type", "number"}, {"description", "Stop trigger price"}}},
            {"reduce_only", {{"type", "boolean"}, {"description", "Only reduce existing position"}}}
        };
        t.input_schema.required = {"portfolio_id", "symbol", "side", "quantity"};
        t.handler = [](const json& args) -> ToolResult {
            std::string portfolio_id = args.value("portfolio_id", "");
            std::string symbol = args.value("symbol", "");
            std::string side = args.value("side", "");
            std::string order_type = args.value("order_type", "market");
            double quantity = args.value("quantity", 0.0);
            bool reduce_only = args.value("reduce_only", false);

            if (portfolio_id.empty() || symbol.empty() || side.empty() || quantity <= 0)
                return ToolResult::fail("Missing required: portfolio_id, symbol, side, quantity (>0)");

            std::optional<double> price;
            if (args.contains("price") && args["price"].is_number())
                price = args["price"].get<double>();

            std::optional<double> stop_price;
            if (args.contains("stop_price") && args["stop_price"].is_number())
                stop_price = args["stop_price"].get<double>();

            try {
                auto order = trading::pt_place_order(
                    portfolio_id, symbol, side, order_type, quantity,
                    price, stop_price, reduce_only);
                LOG_INFO(TAG_TRADING, "Paper order placed: %s %s %.4f %s",
                         side.c_str(), symbol.c_str(), quantity, order.id.c_str());
                return ToolResult::ok("Order placed", {
                    {"order_id", order.id}, {"status", order.status},
                    {"symbol", order.symbol}, {"side", order.side},
                    {"quantity", order.quantity}, {"order_type", order.order_type}
                });
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
        t.category = "trading";
        t.input_schema.properties = {
            {"order_id", {{"type", "string"}, {"description", "Order ID to cancel"}}}
        };
        t.input_schema.required = {"order_id"};
        t.handler = [](const json& args) -> ToolResult {
            std::string order_id = args.value("order_id", "");
            if (order_id.empty()) return ToolResult::fail("Missing 'order_id'");

            try {
                trading::pt_cancel_order(order_id);
                return ToolResult::ok("Order cancelled", {{"order_id", order_id}});
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
        t.category = "trading";
        t.input_schema.properties = {
            {"portfolio_id", {{"type", "string"}, {"description", "Portfolio ID"}}}
        };
        t.input_schema.required = {"portfolio_id"};
        t.handler = [](const json& args) -> ToolResult {
            std::string id = args.value("portfolio_id", "");
            if (id.empty()) return ToolResult::fail("Missing 'portfolio_id'");

            try {
                auto positions = trading::pt_get_positions(id);
                json result = json::array();
                for (const auto& pos : positions) {
                    result.push_back({
                        {"id", pos.id}, {"symbol", pos.symbol}, {"side", pos.side},
                        {"quantity", pos.quantity}, {"entry_price", pos.entry_price},
                        {"current_price", pos.current_price},
                        {"unrealized_pnl", pos.unrealized_pnl},
                        {"realized_pnl", pos.realized_pnl},
                        {"leverage", pos.leverage}, {"opened_at", pos.opened_at}
                    });
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
        t.category = "trading";
        t.input_schema.properties = {
            {"portfolio_id", {{"type", "string"}, {"description", "Portfolio ID"}}},
            {"status", {{"type", "string"}, {"description", "Filter: pending, filled, cancelled (optional)"}}}
        };
        t.input_schema.required = {"portfolio_id"};
        t.handler = [](const json& args) -> ToolResult {
            std::string id = args.value("portfolio_id", "");
            std::string status = args.value("status", "");
            if (id.empty()) return ToolResult::fail("Missing 'portfolio_id'");

            try {
                auto orders = trading::pt_get_orders(id, status);
                json result = json::array();
                for (const auto& o : orders) {
                    json entry = {
                        {"id", o.id}, {"symbol", o.symbol}, {"side", o.side},
                        {"order_type", o.order_type}, {"quantity", o.quantity},
                        {"filled_qty", o.filled_qty}, {"status", o.status},
                        {"reduce_only", o.reduce_only}, {"created_at", o.created_at}
                    };
                    if (o.price) entry["price"] = *o.price;
                    if (o.stop_price) entry["stop_price"] = *o.stop_price;
                    if (o.avg_price) entry["avg_price"] = *o.avg_price;
                    result.push_back(std::move(entry));
                }
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
        t.category = "trading";
        t.input_schema.properties = {
            {"portfolio_id", {{"type", "string"}, {"description", "Portfolio ID"}}}
        };
        t.input_schema.required = {"portfolio_id"};
        t.handler = [](const json& args) -> ToolResult {
            std::string id = args.value("portfolio_id", "");
            if (id.empty()) return ToolResult::fail("Missing 'portfolio_id'");

            try {
                auto stats = trading::pt_get_stats(id);
                return ToolResult::ok_data({
                    {"total_pnl", stats.total_pnl},
                    {"win_rate", stats.win_rate},
                    {"total_trades", stats.total_trades},
                    {"winning_trades", stats.winning_trades},
                    {"losing_trades", stats.losing_trades},
                    {"largest_win", stats.largest_win},
                    {"largest_loss", stats.largest_loss}
                });
            } catch (const std::exception& e) {
                return ToolResult::fail(e.what());
            }
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
