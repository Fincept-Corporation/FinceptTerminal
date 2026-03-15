// Market Data MCP Tools — quote lookup, watchlist ops, symbol search

#include "market_data_tools.h"
#include "../../core/event_bus.h"
#include "../../core/logger.h"
#include "../../storage/database.h"

namespace fincept::mcp::tools {

static constexpr const char* TAG_MARKET = "MarketTools";

std::vector<ToolDef> get_market_data_tools() {
    std::vector<ToolDef> tools;

    // ── get_quote ───────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_quote";
        t.description = "Get a stock/crypto quote. Publishes a request event and returns confirmation.";
        t.category = "market-data";
        t.input_schema.properties = {
            {"symbol", {
                {"type", "string"},
                {"description", "Ticker symbol (e.g. AAPL, BTC-USD)"}
            }}
        };
        t.input_schema.required = {"symbol"};
        t.handler = [](const json& args) -> ToolResult {
            std::string symbol = args.value("symbol", "");
            if (symbol.empty()) return ToolResult::fail("Missing 'symbol'");

            core::EventBus::instance().publish("market.get_quote", {{"symbol", symbol}});
            LOG_DEBUG(TAG_MARKET, "Quote requested: %s", symbol.c_str());
            return ToolResult::ok("Quote request sent for " + symbol, {{"symbol", symbol}});
        };
        tools.push_back(std::move(t));
    }

    // ── search_symbol ───────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "search_symbol";
        t.description = "Search for a ticker symbol by company name or partial symbol.";
        t.category = "market-data";
        t.input_schema.properties = {
            {"query", {
                {"type", "string"},
                {"description", "Search query (company name or partial symbol)"}
            }}
        };
        t.input_schema.required = {"query"};
        t.handler = [](const json& args) -> ToolResult {
            std::string query = args.value("query", "");
            if (query.empty()) return ToolResult::fail("Missing 'query'");

            core::EventBus::instance().publish("market.search_symbol", {{"query", query}});
            return ToolResult::ok("Symbol search sent for: " + query);
        };
        tools.push_back(std::move(t));
    }

    // ── add_to_watchlist ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "add_to_watchlist";
        t.description = "Add a symbol to a watchlist. If watchlist_id is omitted, uses the default watchlist.";
        t.category = "market-data";
        t.input_schema.properties = {
            {"symbol", {
                {"type", "string"},
                {"description", "Ticker symbol to add"}
            }},
            {"watchlist_id", {
                {"type", "string"},
                {"description", "Watchlist ID (optional, uses default if omitted)"}
            }},
            {"notes", {
                {"type", "string"},
                {"description", "Optional notes about this symbol"}
            }}
        };
        t.input_schema.required = {"symbol"};
        t.handler = [](const json& args) -> ToolResult {
            std::string symbol = args.value("symbol", "");
            if (symbol.empty()) return ToolResult::fail("Missing 'symbol'");

            std::string watchlist_id = args.value("watchlist_id", "");
            std::string notes = args.value("notes", "");

            // If no watchlist specified, get the first one
            if (watchlist_id.empty()) {
                auto lists = db::ops::get_watchlists();
                if (lists.empty()) {
                    // Create default watchlist
                    auto wl = db::ops::create_watchlist("Default", "#3b82f6");
                    watchlist_id = wl.id;
                } else {
                    watchlist_id = lists[0].id;
                }
            }

            db::ops::add_watchlist_stock(watchlist_id, symbol, notes);
            LOG_INFO(TAG_MARKET, "Added %s to watchlist %s", symbol.c_str(), watchlist_id.c_str());

            core::EventBus::instance().publish("watchlist.updated", {
                {"watchlist_id", watchlist_id},
                {"action", "add"},
                {"symbol", symbol}
            });

            return ToolResult::ok("Added " + symbol + " to watchlist", {
                {"symbol", symbol},
                {"watchlist_id", watchlist_id}
            });
        };
        tools.push_back(std::move(t));
    }

    // ── remove_from_watchlist ───────────────────────────────────────────
    {
        ToolDef t;
        t.name = "remove_from_watchlist";
        t.description = "Remove a symbol from a watchlist.";
        t.category = "market-data";
        t.input_schema.properties = {
            {"symbol", {
                {"type", "string"},
                {"description", "Ticker symbol to remove"}
            }},
            {"watchlist_id", {
                {"type", "string"},
                {"description", "Watchlist ID (optional, searches all if omitted)"}
            }}
        };
        t.input_schema.required = {"symbol"};
        t.handler = [](const json& args) -> ToolResult {
            std::string symbol = args.value("symbol", "");
            if (symbol.empty()) return ToolResult::fail("Missing 'symbol'");

            std::string watchlist_id = args.value("watchlist_id", "");

            if (watchlist_id.empty()) {
                // Remove from all watchlists
                auto lists = db::ops::get_watchlists();
                for (const auto& wl : lists) {
                    db::ops::remove_watchlist_stock(wl.id, symbol);
                }
            } else {
                db::ops::remove_watchlist_stock(watchlist_id, symbol);
            }

            core::EventBus::instance().publish("watchlist.updated", {
                {"action", "remove"},
                {"symbol", symbol}
            });

            return ToolResult::ok("Removed " + symbol + " from watchlist");
        };
        tools.push_back(std::move(t));
    }

    // ── get_watchlists ──────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_watchlists";
        t.description = "Get all watchlists with their symbols.";
        t.category = "market-data";
        t.handler = [](const json&) -> ToolResult {
            auto lists = db::ops::get_watchlists();
            json result = json::array();

            for (const auto& wl : lists) {
                auto stocks = db::ops::get_watchlist_stocks(wl.id);
                json symbols = json::array();
                for (const auto& s : stocks) {
                    symbols.push_back({
                        {"symbol", s.symbol},
                        {"added_at", s.added_at},
                        {"notes", s.notes.value_or("")}
                    });
                }

                result.push_back({
                    {"id", wl.id},
                    {"name", wl.name},
                    {"color", wl.color},
                    {"description", wl.description.value_or("")},
                    {"symbols", symbols}
                });
            }

            return ToolResult::ok_data(result);
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
