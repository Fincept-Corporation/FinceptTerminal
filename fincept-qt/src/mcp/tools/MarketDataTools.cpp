// MarketDataTools.cpp — Quote lookup, watchlist ops, symbol search (Qt port)

#include "mcp/tools/MarketDataTools.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "storage/repositories/WatchlistRepository.h"

#include <QVariantMap>

namespace fincept::mcp::tools {

static constexpr const char* TAG = "MarketTools";

std::vector<ToolDef> get_market_data_tools() {
    std::vector<ToolDef> tools;

    // ── get_quote ───────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_quote";
        t.description = "Request a stock/crypto quote. Publishes a request event and returns confirmation. "
                        "The actual data is delivered to the Markets screen.";
        t.category = "market-data";
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
        t.category = "market-data";
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

    // ── add_to_watchlist ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "add_to_watchlist";
        t.description = "Add a symbol to a watchlist. If watchlist_id is omitted, uses the first available watchlist.";
        t.category = "market-data";
        t.input_schema.properties = QJsonObject{
            {"symbol", QJsonObject{{"type", "string"}, {"description", "Ticker symbol to add"}}},
            {"watchlist_id", QJsonObject{{"type", "string"},
                                         {"description", "Watchlist ID (optional — uses first watchlist if omitted)"}}},
            {"name", QJsonObject{{"type", "string"}, {"description", "Company/asset name (optional)"}}}};
        t.input_schema.required = {"symbol"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString symbol = args["symbol"].toString().trimmed().toUpper();
            if (symbol.isEmpty())
                return ToolResult::fail("Missing 'symbol'");

            QString watchlist_id = args["watchlist_id"].toString();
            QString name = args["name"].toString();

            auto& repo = WatchlistRepository::instance();

            // Resolve watchlist
            if (watchlist_id.isEmpty()) {
                auto lists = repo.list_all();
                if (lists.is_err())
                    return ToolResult::fail("Failed to load watchlists: " + QString::fromStdString(lists.error()));
                if (lists.value().isEmpty()) {
                    auto created = repo.create("Default");
                    if (created.is_err())
                        return ToolResult::fail("Failed to create watchlist");
                    watchlist_id = created.value().id;
                } else {
                    watchlist_id = lists.value().first().id;
                }
            }

            auto r = repo.add_stock(watchlist_id, symbol, name);
            if (r.is_err())
                return ToolResult::fail("Failed to add symbol: " + QString::fromStdString(r.error()));

            LOG_INFO(TAG, "Added " + symbol + " to watchlist " + watchlist_id);

            EventBus::instance().publish(
                "watchlist.updated",
                QVariantMap{{"watchlist_id", watchlist_id}, {"action", "add"}, {"symbol", symbol}});

            return ToolResult::ok("Added " + symbol + " to watchlist",
                                  QJsonObject{{"symbol", symbol}, {"watchlist_id", watchlist_id}});
        };
        tools.push_back(std::move(t));
    }

    // ── remove_from_watchlist ───────────────────────────────────────────
    {
        ToolDef t;
        t.name = "remove_from_watchlist";
        t.description = "Remove a symbol from a watchlist.";
        t.category = "market-data";
        t.input_schema.properties = QJsonObject{
            {"symbol", QJsonObject{{"type", "string"}, {"description", "Ticker symbol to remove"}}},
            {"watchlist_id",
             QJsonObject{{"type", "string"}, {"description", "Watchlist ID (optional — searches all if omitted)"}}}};
        t.input_schema.required = {"symbol"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString symbol = args["symbol"].toString().trimmed().toUpper();
            if (symbol.isEmpty())
                return ToolResult::fail("Missing 'symbol'");

            QString watchlist_id = args["watchlist_id"].toString();
            auto& repo = WatchlistRepository::instance();

            if (watchlist_id.isEmpty()) {
                auto lists = repo.list_all();
                if (lists.is_ok()) {
                    for (const auto& wl : lists.value())
                        repo.remove_stock(wl.id, symbol);
                }
            } else {
                repo.remove_stock(watchlist_id, symbol);
            }

            EventBus::instance().publish("watchlist.updated", QVariantMap{{"action", "remove"}, {"symbol", symbol}});

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
        t.handler = [](const QJsonObject&) -> ToolResult {
            auto& repo = WatchlistRepository::instance();
            auto lists = repo.list_all();
            if (lists.is_err())
                return ToolResult::fail("Failed to load watchlists: " + QString::fromStdString(lists.error()));

            QJsonArray result;
            for (const auto& wl : lists.value()) {
                auto stocks = repo.get_stocks(wl.id);
                QJsonArray symbols;
                if (stocks.is_ok()) {
                    for (const auto& s : stocks.value()) {
                        symbols.append(QJsonObject{{"symbol", s.symbol}, {"name", s.name}, {"added_at", s.added_at}});
                    }
                }
                result.append(QJsonObject{{"id", wl.id},
                                          {"name", wl.name},
                                          {"color", wl.color},
                                          {"description", wl.description},
                                          {"symbols", symbols}});
            }
            return ToolResult::ok_data(result);
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
