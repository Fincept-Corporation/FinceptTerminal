// MarketsTools.cpp — Markets tab MCP tools (quote lookup, symbol search)

#include "mcp/tools/MarketsTools.h"

#include "core/logging/Logger.h"
#include "mcp/tools/ThreadHelper.h"
#include "python/PythonRunner.h"
#include "services/markets/MarketDataService.h"
#include "storage/cache/CacheManager.h"

#include <QDateTime>
#include <QTimeZone>
#include <QJsonArray>
#include <QJsonDocument>

namespace fincept::mcp::tools {

static constexpr const char* TAG = "MarketsTools";

// Symbol search cache TTL — 1 hour. Yahoo's symbol catalog rarely changes;
// IPOs/delistings settle within a day, so an hour is a generous floor.
static constexpr int kSymbolSearchTtlSec = 60 * 60;

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

    // ── lookup_symbol ───────────────────────────────────────────────────
    // Resolve a free-form company name (or partial ticker) to its actual
    // exchange-suffixed Yahoo ticker via Yahoo's /v1/finance/search endpoint.
    //
    // Why this exists: without this tool, the model used to *guess* tickers
    // from training data and silently store nonsense in the watchlist
    // (e.g. "RITES Limited" → wrong ticker). Cross-exchange names like
    // RITES.NS / TCS.NS / 7203.T are especially prone to that. The model
    // should call lookup_symbol BEFORE add_to_watchlist whenever the user
    // names a company by name rather than ticker.
    {
        ToolDef t;
        t.name = "lookup_symbol";
        t.description =
            "Resolve a company name or partial ticker to its exchange-suffixed Yahoo ticker. "
            "Returns the top candidates as {symbol, name, exchange, type, currency}. "
            "ALWAYS call this BEFORE add_to_watchlist / get_quote when the user names a company "
            "(e.g. 'RITES Limited', 'Apple', 'Tata Consultancy') rather than giving a ticker — "
            "guessing the ticker from prior knowledge frequently gets the suffix wrong "
            "(.NS for NSE, .BO for BSE, .T for Tokyo, .L for London, etc.) and stores a "
            "broken symbol. Backed by Yahoo Finance's search API.";
        t.category = "markets";
        t.input_schema.properties = QJsonObject{
            {"query", QJsonObject{{"type", "string"},
                                  {"description", "Company name or partial ticker (e.g. 'RITES Limited', 'Apple', 'TCS')"}}},
            {"limit", QJsonObject{{"type", "integer"},
                                  {"description", "Max results to return (1-20, default 10)"}}}};
        t.input_schema.required = {"query"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString query = args["query"].toString().trimmed();
            if (query.isEmpty())
                return ToolResult::fail("Missing 'query'");
            int limit = args["limit"].toInt(10);
            if (limit < 1) limit = 1;
            if (limit > 20) limit = 20;

            const QString cache_key = QString("symsearch:%1:%2").arg(query.toLower()).arg(limit);
            const QVariant cached = fincept::CacheManager::instance().get(cache_key);
            if (!cached.isNull()) {
                auto doc = QJsonDocument::fromJson(cached.toString().toUtf8());
                if (!doc.isNull() && doc.isObject())
                    return ToolResult::ok_data(doc.object());
            }

            QJsonObject result_obj;
            QString error;

            auto* runner = &fincept::python::PythonRunner::instance();
            const QStringList py_args = {"search", query, QString::number(limit)};
            detail::run_async_wait(runner, [&](auto signal_done) {
                runner->run("yfinance_data.py", py_args,
                            [&, signal_done](const fincept::python::PythonResult& r) {
                    if (!r.success) {
                        error = r.error.isEmpty() ? r.output : r.error;
                    } else {
                        const QString text = fincept::python::extract_json(r.output);
                        QJsonParseError pe;
                        auto doc = QJsonDocument::fromJson(text.toUtf8(), &pe);
                        if (pe.error != QJsonParseError::NoError) {
                            error = QString("JSON parse error: %1").arg(pe.errorString());
                        } else if (!doc.isObject()) {
                            error = "Unexpected response shape";
                        } else {
                            QJsonObject o = doc.object();
                            if (o.contains("error") && !o.value("error").toString().isEmpty()) {
                                error = o.value("error").toString();
                            } else {
                                // Trim each row to the fields the model needs to disambiguate.
                                // Keeping the payload compact reduces follow-up token cost.
                                QJsonArray trimmed;
                                for (const auto& v : o.value("results").toArray()) {
                                    QJsonObject row = v.toObject();
                                    QJsonObject keep;
                                    keep["symbol"]   = row.value("symbol").toString();
                                    keep["name"]     = row.value("name").toString();
                                    keep["exchange"] = row.value("exchange").toString();
                                    keep["type"]     = row.value("type").toString();
                                    keep["currency"] = row.value("currency").toString();
                                    trimmed.append(keep);
                                }
                                result_obj["query"] = query;
                                result_obj["count"] = trimmed.size();
                                result_obj["results"] = trimmed;
                            }
                        }
                    }
                    signal_done();
                });
            });

            if (!error.isEmpty()) {
                LOG_WARN(TAG, QString("lookup_symbol error [%1]: %2").arg(query, error));
                return ToolResult::fail(error);
            }

            if (result_obj.value("count").toInt() == 0) {
                LOG_INFO(TAG, "lookup_symbol no matches for " + query);
                return ToolResult::ok("No matches — refine the query or pass a known ticker directly.",
                                      result_obj);
            }

            fincept::CacheManager::instance().put(
                cache_key,
                QVariant(QString::fromUtf8(QJsonDocument(result_obj).toJson(QJsonDocument::Compact))),
                kSymbolSearchTtlSec, "markets");
            return ToolResult::ok_data(result_obj);
        };
        tools.push_back(std::move(t));
    }

    // ── get_history ─────────────────────────────────────────────────────
    // Historical OHLCV bars via MarketDataService::fetch_history. The
    // service publishes to DataHub topic `market:history:<sym>:<period>:<interval>`
    // as a side effect, warming the hub for streaming chart subscribers.
    {
        ToolDef t;
        t.name = "get_history";
        t.description = "Fetch historical OHLCV bars for a stock/ETF/crypto symbol. "
                        "Returns an array of {date, timestamp, open, high, low, close, volume}. "
                        "Backed by yfinance.";
        t.category = "markets";
        t.input_schema.properties = QJsonObject{
            {"symbol", QJsonObject{{"type", "string"},
                                   {"description", "Ticker symbol (e.g. AAPL, BTC-USD)"}}},
            {"period", QJsonObject{{"type", "string"},
                                   {"description", "1d, 5d, 1mo, 3mo, 6mo, 1y, 2y, 5y, 10y, ytd, max (default 1mo)"}}},
            {"interval", QJsonObject{{"type", "string"},
                                     {"description", "1m, 5m, 15m, 30m, 1h, 1d, 1wk, 1mo (default 1d). "
                                                     "Intraday intervals (<1d) limit period to ~60d."}}}};
        t.input_schema.required = {"symbol"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString symbol = args["symbol"].toString().trimmed().toUpper();
            if (symbol.isEmpty())
                return ToolResult::fail("Missing 'symbol'");

            QString period = args["period"].toString().trimmed();
            if (period.isEmpty()) period = "1mo";
            QString interval = args["interval"].toString().trimmed();
            if (interval.isEmpty()) interval = "1d";

            auto* svc = &services::MarketDataService::instance();
            bool ok = false;
            QVector<services::HistoryPoint> points;
            detail::run_async_wait(svc, [svc, symbol, period, interval, &ok, &points](auto signal_done) {
                svc->fetch_history(symbol, period, interval,
                                   [&ok, &points, signal_done](bool success, QVector<services::HistoryPoint> result) {
                    ok = success;
                    points = std::move(result);
                    signal_done();
                });
            });

            if (!ok) {
                LOG_WARN(TAG, QString("fetch_history failed for %1 (%2/%3)").arg(symbol, period, interval));
                return ToolResult::fail("No history data available for " + symbol);
            }

            QJsonArray bars;
            for (const auto& p : points) {
                bars.append(QJsonObject{
                    {"timestamp", p.timestamp},
                    {"date", QDateTime::fromSecsSinceEpoch(p.timestamp, QTimeZone::UTC).toString(Qt::ISODate)},
                    {"open", p.open},
                    {"high", p.high},
                    {"low", p.low},
                    {"close", p.close},
                    {"volume", static_cast<double>(p.volume)}});
            }

            return ToolResult::ok_data(QJsonObject{
                {"symbol", symbol},
                {"period", period},
                {"interval", interval},
                {"count", bars.size()},
                {"bars", bars}});
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
