// Exchange MCP Tools — live market data from crypto exchanges

#include "exchange_tools.h"
#include "../../trading/exchange_service.h"
#include "../../core/logger.h"

namespace fincept::mcp::tools {

std::vector<ToolDef> get_exchange_tools() {
    std::vector<ToolDef> tools;

    // ── get_ticker ─────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_ticker";
        t.description = "Get the latest price, volume, and change for a symbol from the configured exchange.";
        t.category = "exchange";
        t.input_schema.properties = {
            {"symbol", {{"type", "string"}, {"description", "Trading pair (e.g. BTC/USDT, ETH/USDT)"}}}
        };
        t.input_schema.required = {"symbol"};
        t.handler = [](const json& args) -> ToolResult {
            std::string symbol = args.value("symbol", "");
            if (symbol.empty()) return ToolResult::fail("Missing 'symbol'");

            auto& svc = trading::ExchangeService::instance();
            if (svc.get_exchange().empty())
                return ToolResult::fail("No exchange configured — set one in Crypto Trading first");

            try {
                auto ticker = svc.fetch_ticker(symbol);
                return ToolResult::ok_data({
                    {"symbol", ticker.symbol}, {"last", ticker.last},
                    {"bid", ticker.bid}, {"ask", ticker.ask},
                    {"high", ticker.high}, {"low", ticker.low},
                    {"open", ticker.open}, {"close", ticker.close},
                    {"change", ticker.change}, {"change_pct", ticker.percentage},
                    {"volume", ticker.base_volume},
                    {"quote_volume", ticker.quote_volume},
                    {"timestamp", ticker.timestamp}
                });
            } catch (const std::exception& e) {
                return ToolResult::fail(e.what());
            }
        };
        tools.push_back(std::move(t));
    }

    // ── get_order_book ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_order_book";
        t.description = "Get the order book (bids and asks) for a symbol.";
        t.category = "exchange";
        t.input_schema.properties = {
            {"symbol", {{"type", "string"}, {"description", "Trading pair"}}},
            {"limit", {{"type", "integer"}, {"description", "Number of levels (default: 20)"}}}
        };
        t.input_schema.required = {"symbol"};
        t.handler = [](const json& args) -> ToolResult {
            std::string symbol = args.value("symbol", "");
            int limit = args.value("limit", 20);
            if (symbol.empty()) return ToolResult::fail("Missing 'symbol'");

            auto& svc = trading::ExchangeService::instance();
            if (svc.get_exchange().empty())
                return ToolResult::fail("No exchange configured");

            try {
                auto ob = svc.fetch_orderbook(symbol, limit);
                json bids = json::array();
                for (const auto& [price, amount] : ob.bids) {
                    bids.push_back({{"price", price}, {"amount", amount}});
                }
                json asks = json::array();
                for (const auto& [price, amount] : ob.asks) {
                    asks.push_back({{"price", price}, {"amount", amount}});
                }
                return ToolResult::ok_data({
                    {"symbol", ob.symbol},
                    {"best_bid", ob.best_bid}, {"best_ask", ob.best_ask},
                    {"spread", ob.spread}, {"spread_pct", ob.spread_pct},
                    {"bids", bids}, {"asks", asks}
                });
            } catch (const std::exception& e) {
                return ToolResult::fail(e.what());
            }
        };
        tools.push_back(std::move(t));
    }

    // ── get_candles ────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_candles";
        t.description = "Get OHLCV candle data for a symbol. Timeframes: 1m, 5m, 15m, 1h, 4h, 1d.";
        t.category = "exchange";
        t.input_schema.properties = {
            {"symbol", {{"type", "string"}, {"description", "Trading pair"}}},
            {"timeframe", {{"type", "string"}, {"description", "Candle interval (default: 1h)"}}},
            {"limit", {{"type", "integer"}, {"description", "Number of candles (default: 100)"}}}
        };
        t.input_schema.required = {"symbol"};
        t.handler = [](const json& args) -> ToolResult {
            std::string symbol = args.value("symbol", "");
            std::string timeframe = args.value("timeframe", "1h");
            int limit = args.value("limit", 100);
            if (symbol.empty()) return ToolResult::fail("Missing 'symbol'");

            auto& svc = trading::ExchangeService::instance();
            if (svc.get_exchange().empty())
                return ToolResult::fail("No exchange configured");

            try {
                auto candles = svc.fetch_ohlcv(symbol, timeframe, limit);
                json result = json::array();
                for (const auto& c : candles) {
                    result.push_back({
                        {"timestamp", c.timestamp},
                        {"open", c.open}, {"high", c.high},
                        {"low", c.low}, {"close", c.close},
                        {"volume", c.volume}
                    });
                }
                return ToolResult::ok_data(result);
            } catch (const std::exception& e) {
                return ToolResult::fail(e.what());
            }
        };
        tools.push_back(std::move(t));
    }

    // ── get_exchange_info ──────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_exchange_info";
        t.description = "Get the currently configured exchange name and list all available exchange IDs.";
        t.category = "exchange";
        t.handler = [](const json&) -> ToolResult {
            auto& svc = trading::ExchangeService::instance();
            std::string current = svc.get_exchange();

            try {
                auto ids = svc.list_exchange_ids();
                json id_arr = json::array();
                for (const auto& id : ids) id_arr.push_back(id);

                return ToolResult::ok_data({
                    {"current_exchange", current.empty() ? "none" : current},
                    {"available_exchanges", id_arr}
                });
            } catch (const std::exception& e) {
                // If listing fails, at least return the current one
                return ToolResult::ok_data({
                    {"current_exchange", current.empty() ? "none" : current},
                    {"available_exchanges", json::array()}
                });
            }
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
