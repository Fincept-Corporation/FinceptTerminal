// Investment Portfolio MCP Tools — long-term portfolio management

#include "portfolio_tools_ext.h"
#include "../../portfolio/portfolio_service.h"
#include "../../core/logger.h"

namespace fincept::mcp::tools {

static constexpr const char* TAG_PORTFOLIO = "PortfolioExtTools";

std::vector<ToolDef> get_investment_portfolio_tools() {
    std::vector<ToolDef> tools;

    // ── create_investment_portfolio ─────────────────────────────────────
    {
        ToolDef t;
        t.name = "create_investment_portfolio";
        t.description = "Create a new investment portfolio for tracking long-term holdings.";
        t.category = "portfolio";
        t.input_schema.properties = {
            {"name", {{"type", "string"}, {"description", "Portfolio name"}}},
            {"owner", {{"type", "string"}, {"description", "Owner name"}}},
            {"currency", {{"type", "string"}, {"description", "Currency (default: USD)"}}},
            {"description", {{"type", "string"}, {"description", "Portfolio description"}}}
        };
        t.input_schema.required = {"name", "owner"};
        t.handler = [](const json& args) -> ToolResult {
            std::string name = args.value("name", "");
            std::string owner = args.value("owner", "");
            if (name.empty() || owner.empty())
                return ToolResult::fail("Missing 'name' or 'owner'");

            std::string currency = args.value("currency", "USD");
            std::string desc = args.value("description", "");

            try {
                auto p = portfolio::create_portfolio(name, owner, currency, desc);
                LOG_INFO(TAG_PORTFOLIO, "Created portfolio: %s", p.id.c_str());
                return ToolResult::ok("Portfolio created", {
                    {"id", p.id}, {"name", p.name}, {"owner", p.owner},
                    {"currency", p.currency}
                });
            } catch (const std::exception& e) {
                return ToolResult::fail(e.what());
            }
        };
        tools.push_back(std::move(t));
    }

    // ── list_investment_portfolios ──────────────────────────────────────
    {
        ToolDef t;
        t.name = "list_investment_portfolios";
        t.description = "List all investment portfolios.";
        t.category = "portfolio";
        t.handler = [](const json&) -> ToolResult {
            try {
                auto portfolios = portfolio::get_all_portfolios();
                json result = json::array();
                for (const auto& p : portfolios) {
                    result.push_back({
                        {"id", p.id}, {"name", p.name}, {"owner", p.owner},
                        {"currency", p.currency},
                        {"description", p.description.value_or("")},
                        {"created_at", p.created_at}
                    });
                }
                return ToolResult::ok_data(result);
            } catch (const std::exception& e) {
                return ToolResult::fail(e.what());
            }
        };
        tools.push_back(std::move(t));
    }

    // ── get_portfolio_assets ───────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_portfolio_assets";
        t.description = "Get all holdings/assets in an investment portfolio.";
        t.category = "portfolio";
        t.input_schema.properties = {
            {"portfolio_id", {{"type", "string"}, {"description", "Portfolio ID"}}}
        };
        t.input_schema.required = {"portfolio_id"};
        t.handler = [](const json& args) -> ToolResult {
            std::string id = args.value("portfolio_id", "");
            if (id.empty()) return ToolResult::fail("Missing 'portfolio_id'");

            try {
                auto assets = portfolio::get_assets(id);
                json result = json::array();
                for (const auto& a : assets) {
                    result.push_back({
                        {"id", a.id}, {"symbol", a.symbol},
                        {"quantity", a.quantity}, {"avg_buy_price", a.avg_buy_price},
                        {"first_purchase_date", a.first_purchase_date},
                        {"last_updated", a.last_updated}
                    });
                }
                return ToolResult::ok_data(result);
            } catch (const std::exception& e) {
                return ToolResult::fail(e.what());
            }
        };
        tools.push_back(std::move(t));
    }

    // ── add_portfolio_asset ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "add_portfolio_asset";
        t.description = "Buy/add an asset to an investment portfolio.";
        t.category = "portfolio";
        t.input_schema.properties = {
            {"portfolio_id", {{"type", "string"}, {"description", "Portfolio ID"}}},
            {"symbol", {{"type", "string"}, {"description", "Ticker symbol (e.g. AAPL)"}}},
            {"quantity", {{"type", "number"}, {"description", "Number of shares/units"}}},
            {"price", {{"type", "number"}, {"description", "Purchase price per unit"}}}
        };
        t.input_schema.required = {"portfolio_id", "symbol", "quantity", "price"};
        t.handler = [](const json& args) -> ToolResult {
            std::string portfolio_id = args.value("portfolio_id", "");
            std::string symbol = args.value("symbol", "");
            double quantity = args.value("quantity", 0.0);
            double price = args.value("price", 0.0);

            if (portfolio_id.empty() || symbol.empty() || quantity <= 0 || price <= 0)
                return ToolResult::fail("Missing required params or invalid values");

            try {
                portfolio::add_asset(portfolio_id, symbol, quantity, price);
                LOG_INFO(TAG_PORTFOLIO, "Added %s x%.2f @ %.2f to %s",
                         symbol.c_str(), quantity, price, portfolio_id.c_str());
                return ToolResult::ok("Asset added", {
                    {"symbol", symbol}, {"quantity", quantity}, {"price", price}
                });
            } catch (const std::exception& e) {
                return ToolResult::fail(e.what());
            }
        };
        tools.push_back(std::move(t));
    }

    // ── sell_portfolio_asset ───────────────────────────────────────────
    {
        ToolDef t;
        t.name = "sell_portfolio_asset";
        t.description = "Sell an asset from an investment portfolio.";
        t.category = "portfolio";
        t.input_schema.properties = {
            {"portfolio_id", {{"type", "string"}, {"description", "Portfolio ID"}}},
            {"symbol", {{"type", "string"}, {"description", "Ticker symbol"}}},
            {"quantity", {{"type", "number"}, {"description", "Number of shares/units to sell"}}},
            {"price", {{"type", "number"}, {"description", "Sale price per unit"}}}
        };
        t.input_schema.required = {"portfolio_id", "symbol", "quantity", "price"};
        t.handler = [](const json& args) -> ToolResult {
            std::string portfolio_id = args.value("portfolio_id", "");
            std::string symbol = args.value("symbol", "");
            double quantity = args.value("quantity", 0.0);
            double price = args.value("price", 0.0);

            if (portfolio_id.empty() || symbol.empty() || quantity <= 0 || price <= 0)
                return ToolResult::fail("Missing required params or invalid values");

            try {
                portfolio::sell_asset(portfolio_id, symbol, quantity, price);
                return ToolResult::ok("Asset sold", {
                    {"symbol", symbol}, {"quantity", quantity}, {"price", price}
                });
            } catch (const std::exception& e) {
                return ToolResult::fail(e.what());
            }
        };
        tools.push_back(std::move(t));
    }

    // ── get_portfolio_transactions ─────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_portfolio_transactions";
        t.description = "Get transaction history for an investment portfolio.";
        t.category = "portfolio";
        t.input_schema.properties = {
            {"portfolio_id", {{"type", "string"}, {"description", "Portfolio ID"}}},
            {"limit", {{"type", "integer"}, {"description", "Max transactions to return (default: all)"}}}
        };
        t.input_schema.required = {"portfolio_id"};
        t.handler = [](const json& args) -> ToolResult {
            std::string id = args.value("portfolio_id", "");
            int limit = args.value("limit", -1);
            if (id.empty()) return ToolResult::fail("Missing 'portfolio_id'");

            try {
                auto txns = portfolio::get_transactions(id, limit);
                json result = json::array();
                for (const auto& tx : txns) {
                    result.push_back({
                        {"id", tx.id}, {"symbol", tx.symbol},
                        {"type", tx.transaction_type},
                        {"quantity", tx.quantity}, {"price", tx.price},
                        {"total_value", tx.total_value},
                        {"date", tx.transaction_date},
                        {"notes", tx.notes.value_or("")}
                    });
                }
                return ToolResult::ok_data(result);
            } catch (const std::exception& e) {
                return ToolResult::fail(e.what());
            }
        };
        tools.push_back(std::move(t));
    }

    // ── export_portfolio ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "export_portfolio";
        t.description = "Export an investment portfolio as JSON or CSV.";
        t.category = "portfolio";
        t.input_schema.properties = {
            {"portfolio_id", {{"type", "string"}, {"description", "Portfolio ID"}}},
            {"format", {{"type", "string"}, {"description", "Export format: json or csv"}}}
        };
        t.input_schema.required = {"portfolio_id", "format"};
        t.handler = [](const json& args) -> ToolResult {
            std::string id = args.value("portfolio_id", "");
            std::string format = args.value("format", "json");
            if (id.empty()) return ToolResult::fail("Missing 'portfolio_id'");

            try {
                std::string data;
                if (format == "csv") {
                    data = portfolio::export_portfolio_csv(id);
                } else {
                    data = portfolio::export_portfolio_json(id);
                }
                return ToolResult::ok("Exported portfolio", {
                    {"format", format}, {"data", data}
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
