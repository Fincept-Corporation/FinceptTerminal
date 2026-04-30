// MarketsTools.cpp — Markets tab MCP tools (quote lookup, symbol search)

#include "mcp/tools/MarketsTools.h"

#include "core/logging/Logger.h"
#include "mcp/tools/ThreadHelper.h"
#include "services/markets/MarketDataService.h"

namespace fincept::mcp::tools {

static constexpr const char* TAG = "MarketsTools";

std::vector<ToolDef> get_markets_tools() {
    std::vector<ToolDef> tools;

    // ── get_quote ───────────────────────────────────────────────────────
    // Synchronous fetch via MarketDataService. The service batches/dedups
    // (100 ms window) and publishes results to the DataHub `market:quote:<sym>`
    // topic as a side effect, so the same call warms the hub for any
    // streaming subscribers. Worker-thread blocking is bridged via
    // detail::run_async_wait — the service lives on the main thread.
    {
        ToolDef t;
        t.name = "get_quote";
        t.description = "Fetch the latest stock/ETF/crypto quote (price, change, "
                        "high/low, volume). Backed by yfinance. Symbols accept "
                        "AAPL, BTC-USD, ^GSPC, GBPUSD=X, GC=F, etc.";
        t.category = "markets";
        t.input_schema.properties = QJsonObject{
            {"symbol", QJsonObject{{"type", "string"}, {"description", "Ticker symbol (e.g. AAPL, BTC-USD)"}}}};
        t.input_schema.required = {"symbol"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString symbol = args["symbol"].toString().trimmed().toUpper();
            if (symbol.isEmpty())
                return ToolResult::fail("Missing 'symbol'");

            auto* svc = &services::MarketDataService::instance();
            bool ok = false;
            services::QuoteData q;
            detail::run_async_wait(svc, [svc, symbol, &ok, &q](auto signal_done) {
                svc->fetch_quotes({symbol}, [&ok, &q, symbol, signal_done](bool success, QVector<services::QuoteData> quotes) {
                    if (success) {
                        for (const auto& candidate : quotes) {
                            if (candidate.symbol.compare(symbol, Qt::CaseInsensitive) == 0) {
                                q = candidate;
                                ok = true;
                                break;
                            }
                        }
                    }
                    signal_done();
                });
            });

            if (!ok) {
                LOG_WARN(TAG, "fetch_quotes returned no data for " + symbol);
                return ToolResult::fail("No quote data available for " + symbol);
            }

            return ToolResult::ok_data(QJsonObject{{"symbol", q.symbol},
                                                   {"name", q.name},
                                                   {"price", q.price},
                                                   {"change", q.change},
                                                   {"change_pct", q.change_pct},
                                                   {"high", q.high},
                                                   {"low", q.low},
                                                   {"volume", q.volume}});
        };
        tools.push_back(std::move(t));
    }

    // ── search_symbol ───────────────────────────────────────────────────
    // Removed in Phase 2: no backend exists for symbol search. The previous
    // implementation published a `market.search_symbol` event with no
    // subscriber and returned a fake-OK ToolResult to the LLM. Per the
    // Phase 2 plan we delete the tool rather than leave it broken.
    // Future option: wire to an OpenFIGI / Yahoo /v1/finance/search
    // endpoint via a Python script, then re-register here.

    return tools;
}

} // namespace fincept::mcp::tools
