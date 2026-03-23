// PortfolioToolsExt.cpp — Investment portfolio holdings MCP tools (Qt port)
// Uses PortfolioHoldingsRepository which manages a single flat list of holdings.

#include "mcp/tools/PortfolioToolsExt.h"

#include "core/logging/Logger.h"
#include "storage/repositories/PortfolioHoldingsRepository.h"

namespace fincept::mcp::tools {

static constexpr const char* TAG = "PortfolioExtTools";

std::vector<ToolDef> get_investment_portfolio_tools() {
    std::vector<ToolDef> tools;

    // ── get_holdings ───────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_holdings";
        t.description = "Get all active investment portfolio holdings (symbol, shares, avg cost).";
        t.category = "portfolio";
        t.handler = [](const QJsonObject&) -> ToolResult {
            auto result = PortfolioHoldingsRepository::instance().get_active();
            if (result.is_err())
                return ToolResult::fail("Failed to load holdings: " + QString::fromStdString(result.error()));

            QJsonArray arr;
            for (const auto& h : result.value()) {
                arr.append(QJsonObject{{"id", h.id},
                                       {"symbol", h.symbol},
                                       {"name", h.name},
                                       {"shares", h.shares},
                                       {"avg_cost", h.avg_cost},
                                       {"added_at", h.added_at},
                                       {"updated_at", h.updated_at}});
            }
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    // ── add_holding ────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "add_holding";
        t.description = "Add or increase a position in the investment portfolio.";
        t.category = "portfolio";
        t.input_schema.properties =
            QJsonObject{{"symbol", QJsonObject{{"type", "string"}, {"description", "Ticker symbol (e.g. AAPL)"}}},
                        {"shares", QJsonObject{{"type", "number"}, {"description", "Number of shares/units"}}},
                        {"avg_cost", QJsonObject{{"type", "number"}, {"description", "Average cost per share/unit"}}},
                        {"name", QJsonObject{{"type", "string"}, {"description", "Company/asset name (optional)"}}}};
        t.input_schema.required = {"symbol", "shares", "avg_cost"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString symbol = args["symbol"].toString().trimmed().toUpper();
            double shares = args["shares"].toDouble(0.0);
            double avg_cost = args["avg_cost"].toDouble(0.0);
            QString name = args["name"].toString();

            if (symbol.isEmpty() || shares <= 0 || avg_cost <= 0)
                return ToolResult::fail("Missing or invalid: symbol, shares (>0), avg_cost (>0)");

            auto r = PortfolioHoldingsRepository::instance().add(symbol, shares, avg_cost, name);
            if (r.is_err())
                return ToolResult::fail("Failed to add holding: " + QString::fromStdString(r.error()));

            LOG_INFO(TAG, QString("Added holding: %1 x%2 @ %3").arg(symbol).arg(shares).arg(avg_cost));

            return ToolResult::ok("Holding added", QJsonObject{{"id", static_cast<int>(r.value())},
                                                               {"symbol", symbol},
                                                               {"shares", shares},
                                                               {"avg_cost", avg_cost}});
        };
        tools.push_back(std::move(t));
    }

    // ── update_holding ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "update_holding";
        t.description = "Update shares and average cost for an existing holding by its ID.";
        t.category = "portfolio";
        t.input_schema.properties =
            QJsonObject{{"id", QJsonObject{{"type", "integer"}, {"description", "Holding ID"}}},
                        {"shares", QJsonObject{{"type", "number"}, {"description", "Updated share count"}}},
                        {"avg_cost", QJsonObject{{"type", "number"}, {"description", "Updated average cost"}}}};
        t.input_schema.required = {"id", "shares", "avg_cost"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            int id = args["id"].toInt(-1);
            double shares = args["shares"].toDouble(0.0);
            double avg_cost = args["avg_cost"].toDouble(0.0);

            if (id < 0 || shares <= 0 || avg_cost <= 0)
                return ToolResult::fail("Missing or invalid: id, shares (>0), avg_cost (>0)");

            auto r = PortfolioHoldingsRepository::instance().update(id, shares, avg_cost);
            if (r.is_err())
                return ToolResult::fail("Failed to update holding: " + QString::fromStdString(r.error()));

            return ToolResult::ok("Holding updated", QJsonObject{{"id", id}});
        };
        tools.push_back(std::move(t));
    }

    // ── remove_holding ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "remove_holding";
        t.description = "Remove a holding from the portfolio by its ID.";
        t.category = "portfolio";
        t.input_schema.properties =
            QJsonObject{{"id", QJsonObject{{"type", "integer"}, {"description", "Holding ID"}}}};
        t.input_schema.required = {"id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            int id = args["id"].toInt(-1);
            if (id < 0)
                return ToolResult::fail("Missing or invalid 'id'");

            auto r = PortfolioHoldingsRepository::instance().remove(id);
            if (r.is_err())
                return ToolResult::fail("Failed to remove holding: " + QString::fromStdString(r.error()));

            return ToolResult::ok("Holding removed", QJsonObject{{"id", id}});
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
