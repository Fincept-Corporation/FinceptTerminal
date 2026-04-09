// MarketsTools.cpp — Markets tab MCP tools (quote lookup, symbol search)

#include "mcp/tools/MarketsTools.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"

#include <QVariantMap>

namespace fincept::mcp::tools {

static constexpr const char* TAG = "MarketsTools";

std::vector<ToolDef> get_markets_tools() {
    std::vector<ToolDef> tools;

    // ── get_quote ───────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_quote";
        t.description = "Request a stock/crypto quote. Publishes a request event and returns confirmation. "
                        "The actual data is delivered to the Markets screen.";
        t.category = "markets";
        t.input_schema.properties = QJsonObject{
            {"symbol", QJsonObject{{"type", "string"}, {"description", "Ticker symbol (e.g. AAPL, BTC-USD)"}}}};
        t.input_schema.required = {"symbol"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString symbol = args["symbol"].toString().trimmed().toUpper();
            if (symbol.isEmpty())
                return ToolResult::fail("Missing 'symbol'");

            EventBus::instance().publish("market.get_quote", QVariantMap{{"symbol", symbol}});
            LOG_DEBUG(TAG, "Quote requested: " + symbol);
            return ToolResult::ok("Quote request sent for " + symbol, QJsonObject{{"symbol", symbol}});
        };
        tools.push_back(std::move(t));
    }

    // ── search_symbol ───────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "search_symbol";
        t.description = "Search for a ticker symbol by company name or partial symbol.";
        t.category = "markets";
        t.input_schema.properties =
            QJsonObject{{"query", QJsonObject{{"type", "string"},
                                              {"description", "Search query (company name or partial symbol)"}}}};
        t.input_schema.required = {"query"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString query = args["query"].toString().trimmed();
            if (query.isEmpty())
                return ToolResult::fail("Missing 'query'");

            EventBus::instance().publish("market.search_symbol", QVariantMap{{"query", query}});
            return ToolResult::ok("Symbol search sent for: " + query);
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
