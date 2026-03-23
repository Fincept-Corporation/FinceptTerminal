// ExchangeTools.cpp — Live crypto exchange data tools (Qt port)

#include "mcp/tools/ExchangeTools.h"

#include "core/logging/Logger.h"
#include "trading/ExchangeService.h"

namespace fincept::mcp::tools {

std::vector<ToolDef> get_exchange_tools() {
    std::vector<ToolDef> tools;

    // ── get_ticker ─────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_ticker";
        t.description = "Get the latest price, volume, and change for a symbol from the configured exchange.";
        t.category = "exchange";
        t.input_schema.properties = QJsonObject{
            {"symbol", QJsonObject{{"type", "string"}, {"description", "Trading pair (e.g. BTC/USDT, ETH/USDT)"}}}};
        t.input_schema.required = {"symbol"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString symbol = args["symbol"].toString().trimmed();
            if (symbol.isEmpty())
                return ToolResult::fail("Missing 'symbol'");

            auto& svc = trading::ExchangeService::instance();
            if (svc.get_exchange().isEmpty())
                return ToolResult::fail("No exchange configured — set one in Crypto Trading first");

            try {
                auto ticker = svc.fetch_ticker(symbol);
                return ToolResult::ok_data(QJsonObject{{"symbol", ticker.symbol},
                                                       {"last", ticker.last},
                                                       {"bid", ticker.bid},
                                                       {"ask", ticker.ask},
                                                       {"high", ticker.high},
                                                       {"low", ticker.low},
                                                       {"open", ticker.open},
                                                       {"close", ticker.close},
                                                       {"change", ticker.change},
                                                       {"change_pct", ticker.percentage},
                                                       {"volume", ticker.base_volume},
                                                       {"quote_volume", ticker.quote_volume},
                                                       {"timestamp", static_cast<double>(ticker.timestamp)}});
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
        t.input_schema.properties =
            QJsonObject{{"symbol", QJsonObject{{"type", "string"}, {"description", "Trading pair"}}},
                        {"limit", QJsonObject{{"type", "integer"}, {"description", "Number of levels (default: 20)"}}}};
        t.input_schema.required = {"symbol"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString symbol = args["symbol"].toString().trimmed();
            int limit = args["limit"].toInt(20);
            if (symbol.isEmpty())
                return ToolResult::fail("Missing 'symbol'");

            auto& svc = trading::ExchangeService::instance();
            if (svc.get_exchange().isEmpty())
                return ToolResult::fail("No exchange configured");

            try {
                auto ob = svc.fetch_orderbook(symbol, limit);
                QJsonArray bids, asks;
                for (const auto& level : ob.bids)
                    bids.append(QJsonObject{{"price", level.first}, {"amount", level.second}});
                for (const auto& level : ob.asks)
                    asks.append(QJsonObject{{"price", level.first}, {"amount", level.second}});

                return ToolResult::ok_data(QJsonObject{{"symbol", ob.symbol},
                                                       {"best_bid", ob.best_bid},
                                                       {"best_ask", ob.best_ask},
                                                       {"spread", ob.spread},
                                                       {"spread_pct", ob.spread_pct},
                                                       {"bids", bids},
                                                       {"asks", asks}});
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
        t.input_schema.properties = QJsonObject{
            {"symbol", QJsonObject{{"type", "string"}, {"description", "Trading pair"}}},
            {"timeframe", QJsonObject{{"type", "string"}, {"description", "Candle interval (default: 1h)"}}},
            {"limit", QJsonObject{{"type", "integer"}, {"description", "Number of candles (default: 100)"}}}};
        t.input_schema.required = {"symbol"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString symbol = args["symbol"].toString().trimmed();
            QString timeframe = args["timeframe"].toString("1h");
            int limit = args["limit"].toInt(100);
            if (symbol.isEmpty())
                return ToolResult::fail("Missing 'symbol'");

            auto& svc = trading::ExchangeService::instance();
            if (svc.get_exchange().isEmpty())
                return ToolResult::fail("No exchange configured");

            try {
                auto candles = svc.fetch_ohlcv(symbol, timeframe, limit);
                QJsonArray result;
                for (const auto& c : candles) {
                    result.append(QJsonObject{{"timestamp", static_cast<double>(c.timestamp)},
                                              {"open", c.open},
                                              {"high", c.high},
                                              {"low", c.low},
                                              {"close", c.close},
                                              {"volume", c.volume}});
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
        t.handler = [](const QJsonObject&) -> ToolResult {
            auto& svc = trading::ExchangeService::instance();
            QString current = svc.get_exchange();

            try {
                auto ids = svc.list_exchange_ids();
                QJsonArray id_arr;
                for (const auto& id : ids)
                    id_arr.append(id);

                return ToolResult::ok_data(QJsonObject{{"current_exchange", current.isEmpty() ? "none" : current},
                                                       {"available_exchanges", id_arr}});
            } catch (...) {
                return ToolResult::ok_data(QJsonObject{{"current_exchange", current.isEmpty() ? "none" : current},
                                                       {"available_exchanges", QJsonArray{}}});
            }
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
