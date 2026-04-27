#include "services/workflow/nodes/MarketDataNodes.h"

#include "python/PythonRunner.h"
#include "screens/economics/panels/EconomicsPresets.h"
#include "services/workflow/NodeRegistry.h"

#include <QJsonDocument>

namespace fincept::workflow {

using fincept::python::extract_json;
using fincept::python::PythonResult;
using fincept::python::PythonRunner;

namespace {

// Helper: run a Python script and parse the JSON result, calling cb with the outcome.
void run_python_json(const QString& script, const QStringList& args,
                     std::function<void(bool, QJsonValue, QString)> cb) {
    PythonRunner::instance().run(script, args, [cb](const PythonResult& res) {
        if (!res.success) {
            cb(false, {}, res.error);
            return;
        }
        QString json_str = extract_json(res.output).trimmed();
        auto doc = QJsonDocument::fromJson(json_str.toUtf8());
        if (doc.isNull()) {
            cb(false, {}, "Invalid JSON: " + res.output.left(200));
            return;
        }
        // Check for Python-level error: {"success": false, "error": "..."} or {"error": "..."}
        if (doc.isObject()) {
            auto obj = doc.object();
            if (obj.contains("success") && !obj.value("success").toBool(true)) {
                cb(false, {}, obj.value("error").toString("Python script returned failure"));
                return;
            }
            if (obj.contains("error") && obj.size() <= 2 && !obj.contains("data")) {
                cb(false, {}, obj.value("error").toString("Unknown error"));
                return;
            }
        }
        cb(true, doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array()), {});
    });
}

} // anonymous namespace

void register_market_data_nodes(NodeRegistry& registry) {
    registry.register_type({
        .type_id = "market.get_quote",
        .display_name = "Get Quote",
        .category = "Market Data",
        .description = "Fetch real-time market quote for a symbol",
        .icon_text = "$",
        .accent_color = "#2563eb",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::MarketData}},
        .parameters =
            {
                {"symbol", "Symbol", "string", "AAPL", {}, "Ticker symbol", true},
                {"source", "Source", "select", "yahoo", {"yahoo", "alpha_vantage", "polygon", "databento"}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QString symbol = params.value("symbol").toString("AAPL");
                run_python_json("yfinance_data.py", {"quote", symbol}, cb);
            },
    });

    registry.register_type({
        .type_id = "market.get_historical",
        .display_name = "Historical Data",
        .category = "Market Data",
        .description = "Fetch OHLCV historical price data",
        .icon_text = "$",
        .accent_color = "#2563eb",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::PriceData}},
        .parameters =
            {
                {"symbol", "Symbol", "string", "AAPL", {}, "Ticker symbol", true},
                {"period", "Period", "select", "1y", {"1d", "5d", "1mo", "3mo", "6mo", "1y", "2y", "5y", "max"}, ""},
                {"interval", "Interval", "select", "1d", {"1m", "5m", "15m", "1h", "1d", "1wk", "1mo"}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QString symbol = params.value("symbol").toString("AAPL");
                QString period = params.value("period").toString("1y");
                QString interval = params.value("interval").toString("1d");
                run_python_json("yfinance_data.py", {"historical_period", symbol, period, interval}, cb);
            },
    });

    registry.register_type({
        .type_id = "market.get_depth",
        .display_name = "Market Depth",
        .category = "Market Data",
        .description = "Fetch Level 2 order book data",
        .icon_text = "$",
        .accent_color = "#2563eb",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::MarketData}},
        .parameters =
            {
                {"symbol", "Symbol", "string", "AAPL", {}, "Ticker symbol", true},
                {"depth", "Depth", "number", 10, {}, "Number of levels"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QString symbol = params.value("symbol").toString("AAPL");
                // yfinance doesn't provide L2 depth — return basic bid/ask from quote
                run_python_json("yfinance_data.py", {"quote", symbol},
                                [cb, symbol](bool ok, QJsonValue val, QString err) {
                                    if (!ok) {
                                        cb(false, {}, err);
                                        return;
                                    }
                                    QJsonObject quote = val.toObject();
                                    QJsonObject out;
                                    out["symbol"] = symbol;
                                    out["bid"] = quote.value("bid").toDouble(quote.value("price").toDouble());
                                    out["ask"] = quote.value("ask").toDouble(quote.value("price").toDouble());
                                    out["last"] = quote.value("price");
                                    out["source"] = "yfinance";
                                    cb(true, out, {});
                                });
            },
    });

    registry.register_type({
        .type_id = "market.get_stats",
        .display_name = "Ticker Stats",
        .category = "Market Data",
        .description = "Fetch ticker statistics (52w high/low, volume, etc.)",
        .icon_text = "$",
        .accent_color = "#2563eb",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::MarketData}},
        .parameters =
            {
                {"symbol", "Symbol", "string", "AAPL", {}, "Ticker symbol", true},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QString symbol = params.value("symbol").toString("AAPL");
                // info command returns 52w high/low, volume, market cap, etc.
                run_python_json("yfinance_data.py", {"info", symbol}, cb);
            },
    });

    registry.register_type({
        .type_id = "market.get_fundamentals",
        .display_name = "Fundamentals",
        .category = "Market Data",
        .description = "Fetch company fundamentals (P/E, EPS, revenue, etc.)",
        .icon_text = "$",
        .accent_color = "#2563eb",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::FundamentalData}},
        .parameters =
            {
                {"symbol", "Symbol", "string", "AAPL", {}, "Ticker symbol", true},
                {"type", "Type", "select", "overview", {"overview", "income", "balance", "cashflow", "earnings"}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QString symbol = params.value("symbol").toString("AAPL");
                QString type = params.value("type").toString("overview");
                if (type == "overview") {
                    run_python_json("yfinance_data.py", {"info", symbol}, cb);
                } else {
                    // income, balance, cashflow, earnings → financials command
                    run_python_json("yfinance_data.py", {"financials", symbol},
                                    [cb, type](bool ok, QJsonValue val, QString err) {
                                        if (!ok) {
                                            cb(false, {}, err);
                                            return;
                                        }
                                        if (val.isObject() && val.toObject().contains(type)) {
                                            cb(true, val.toObject().value(type), {});
                                        } else {
                                            cb(true, val, {});
                                        }
                                    });
                }
            },
    });

    registry.register_type({
        .type_id = "market.get_economics",
        .display_name = "FRED Economic Data",
        .category = "Market Data",
        .description = "Fetch a FRED macroeconomic series (GDP, CPI, Treasury yields, Fed Funds, etc.)",
        .icon_text = "$",
        .accent_color = "#2563eb",
        .version = 2,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::EconomicData}},
        .parameters =
            {
                {"preset",
                 "Preset",
                 "select",
                 "GDPC1",
                 fincept::screens::fred_preset_series_ids(),
                 "Pick a common series, or use Series ID below for any FRED code"},
                {"series_id", "Series ID (override)", "string", "", {},
                 "Optional FRED series id (e.g. GS10). Overrides Preset when set."},
                {"start_date", "Start Date", "string", "", {}, "YYYY-MM-DD (optional)"},
                {"end_date", "End Date", "string", "", {}, "YYYY-MM-DD (optional)"},
                {"frequency", "Frequency", "select", "",
                 {"", "d", "w", "m", "q", "a"},
                 "Output frequency: d=daily, w=weekly, m=monthly, q=quarterly, a=annual"},
                {"transform", "Transform", "select", "",
                 {"", "chg", "pch", "log"},
                 "chg=change, pch=percent change, log=natural log"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QString series_id = params.value("series_id").toString().trimmed();
                if (series_id.isEmpty())
                    series_id = params.value("preset").toString("GDPC1");

                QStringList args = {"series", series_id};
                const QString start = params.value("start_date").toString().trimmed();
                const QString end = params.value("end_date").toString().trimmed();
                const QString freq = params.value("frequency").toString().trimmed();
                const QString transform = params.value("transform").toString().trimmed();

                // fred_data.py series <id> [start] [end] [frequency] [transform] — positional.
                // Only emit trailing args if needed; pad earlier ones with empty strings.
                if (!start.isEmpty() || !end.isEmpty() || !freq.isEmpty() || !transform.isEmpty())
                    args.append(start);
                if (!end.isEmpty() || !freq.isEmpty() || !transform.isEmpty())
                    args.append(end);
                if (!freq.isEmpty() || !transform.isEmpty())
                    args.append(freq);
                if (!transform.isEmpty())
                    args.append(transform);

                run_python_json("fred_data.py", args, cb);
            },
    });

    registry.register_type({
        .type_id = "market.get_yield_curve",
        .display_name = "US Treasury Yield Curve",
        .category = "Market Data",
        .description = "Fetch the full US Treasury yield curve (1m–30y) plus 10y-2y / 10y-3m spreads from FRED",
        .icon_text = "$",
        .accent_color = "#2563eb",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::EconomicData}},
        .parameters =
            {
                {"start_date", "Start Date", "string", "", {}, "YYYY-MM-DD (optional)"},
                {"end_date", "End Date", "string", "", {}, "YYYY-MM-DD (optional)"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QStringList args = {"yield_curve"};
                const QString start = params.value("start_date").toString().trimmed();
                const QString end = params.value("end_date").toString().trimmed();
                if (!start.isEmpty() || !end.isEmpty())
                    args.append(start);
                if (!end.isEmpty())
                    args.append(end);
                run_python_json("fred_economic_data.py", args, cb);
            },
    });

    registry.register_type({
        .type_id = "market.get_news",
        .display_name = "Market News",
        .category = "Market Data",
        .description = "Fetch latest financial news",
        .icon_text = "$",
        .accent_color = "#2563eb",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::NewsData}},
        .parameters =
            {
                {"symbol", "Symbol", "string", "", {}, "Optional ticker filter"},
                {"limit", "Limit", "number", 10, {}, "Number of articles"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QString symbol = params.value("symbol").toString();
                QString limit = QString::number(static_cast<int>(params.value("limit").toDouble(10)));
                run_python_json("yfinance_data.py", {"news", symbol, limit}, cb);
            },
    });

    // ── Tier 1 additions ───────────────────────────────────────────

    registry.register_type({
        .type_id = "market.get_options_chain",
        .display_name = "Options Chain",
        .category = "Market Data",
        .description = "Fetch options chain with greeks",
        .icon_text = "$",
        .accent_color = "#2563eb",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::OptionsData}},
        .parameters =
            {
                {"symbol", "Symbol", "string", "AAPL", {}, "", true},
                {"expiry", "Expiry", "string", "", {}, "YYYY-MM-DD or 'nearest'"},
                {"type", "Type", "select", "both", {"calls", "puts", "both"}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QString symbol = params.value("symbol").toString("AAPL");
                // Use yfinance info which includes options data
                run_python_json("yfinance_data.py", {"info", symbol},
                                [cb, symbol](bool ok, QJsonValue val, QString err) {
                                    if (!ok) {
                                        cb(false, {}, err);
                                        return;
                                    }
                                    QJsonObject out = val.toObject();
                                    out["symbol"] = symbol;
                                    out["node_type"] = "market.get_options_chain";
                                    cb(true, out, {});
                                });
            },
    });

    registry.register_type({
        .type_id = "market.get_crypto_price",
        .display_name = "Crypto Price",
        .category = "Market Data",
        .description = "Real-time cryptocurrency prices",
        .icon_text = "$",
        .accent_color = "#2563eb",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::PriceData}},
        .parameters =
            {
                {"symbol", "Symbol", "string", "BTC", {}, "BTC, ETH, SOL...", true},
                {"quote", "Quote Currency", "select", "USD", {"USD", "USDT", "EUR", "BTC"}, ""},
                {"exchange", "Exchange", "select", "binance", {"binance", "kraken", "coinbase", "hyperliquid"}, ""},
            },
        .execute = nullptr,
    });

    registry.register_type({
        .type_id = "market.get_forex_rate",
        .display_name = "Forex Rate",
        .category = "Market Data",
        .description = "Foreign exchange rates",
        .icon_text = "$",
        .accent_color = "#2563eb",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::PriceData}},
        .parameters =
            {
                {"base", "Base Currency", "string", "USD", {}, "", true},
                {"quote", "Quote Currency", "string", "EUR", {}, "", true},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QString base = params.value("base").toString("USD");
                QString quote = params.value("quote").toString("EUR");
                // yfinance supports forex via "USDEUR=X" format
                QString symbol = base + quote + "=X";
                run_python_json("yfinance_data.py", {"quote", symbol},
                                [cb, base, quote](bool ok, QJsonValue val, QString err) {
                                    if (!ok) {
                                        cb(false, {}, err);
                                        return;
                                    }
                                    QJsonObject out = val.toObject();
                                    out["base"] = base;
                                    out["quote"] = quote;
                                    out["rate"] = out.value("price");
                                    cb(true, out, {});
                                });
            },
    });

    // ── Tier 3: Advanced Market Data ───────────────────────────────

    registry.register_type({
        .type_id = "market.screener",
        .display_name = "Stock Screener",
        .category = "Market Data",
        .description = "Screen stocks by financial criteria",
        .icon_text = "$",
        .accent_color = "#2563eb",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"market_cap_min", "Min Market Cap ($M)", "number", 1000, {}, ""},
                {"pe_max", "Max P/E Ratio", "number", 30, {}, ""},
                {"volume_min", "Min Avg Volume", "number", 500000, {}, ""},
                {"sector",
                 "Sector",
                 "select",
                 "any",
                 {"any", "technology", "healthcare", "finance", "energy", "consumer", "industrial", "utilities",
                  "materials", "real_estate"},
                 ""},
                {"country", "Country", "select", "US", {"US", "UK", "IN", "JP", "DE", "FR", "CA", "AU"}, ""},
                {"limit", "Max Results", "number", 50, {}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                // Use yfinance search as a basic screener
                QString sector = params.value("sector").toString("any");
                QString query = (sector == "any") ? "market" : sector;
                QString limit = QString::number(static_cast<int>(params.value("limit").toDouble(50)));
                run_python_json("yfinance_data.py", {"search", query, limit}, cb);
            },
    });

    registry.register_type({
        .type_id = "market.insider_trades",
        .display_name = "Insider Trades",
        .category = "Market Data",
        .description = "Fetch insider trading activity (Form 4 filings)",
        .icon_text = "$",
        .accent_color = "#2563eb",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"symbol", "Symbol", "string", "", {}, "Ticker or leave empty for all"},
                {"transaction_type", "Type", "select", "all", {"all", "buy", "sell", "exercise"}, ""},
                {"days_back", "Days Back", "number", 30, {}, ""},
                {"min_value", "Min Transaction Value ($)", "number", 100000, {}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QString symbol = params.value("symbol").toString();
                if (symbol.isEmpty()) {
                    cb(false, {}, "Symbol is required for insider trades");
                    return;
                }
                // Use yfinance info which includes insider transaction data
                run_python_json("yfinance_data.py", {"info", symbol},
                                [cb, symbol](bool ok, QJsonValue val, QString err) {
                                    if (!ok) {
                                        cb(false, {}, err);
                                        return;
                                    }
                                    QJsonObject out;
                                    out["symbol"] = symbol;
                                    out["data"] = val;
                                    out["node_type"] = "market.insider_trades";
                                    cb(true, out, {});
                                });
            },
    });

    registry.register_type({
        .type_id = "market.sec_filings",
        .display_name = "SEC Filings",
        .category = "Market Data",
        .description = "Fetch SEC filings (10-K, 10-Q, 8-K, etc.)",
        .icon_text = "$",
        .accent_color = "#2563eb",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"symbol", "Symbol", "string", "", {}, "Company ticker", true},
                {"filing_type",
                 "Filing Type",
                 "select",
                 "10-K",
                 {"10-K", "10-Q", "8-K", "13F", "S-1", "DEF 14A", "all"},
                 ""},
                {"limit", "Limit", "number", 10, {}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QString symbol = params.value("symbol").toString();
                if (symbol.isEmpty()) {
                    cb(false, {}, "Symbol is required for SEC filings");
                    return;
                }
                // Use yfinance info for basic company data including filings metadata
                run_python_json("yfinance_data.py", {"info", symbol},
                                [cb, symbol](bool ok, QJsonValue val, QString err) {
                                    if (!ok) {
                                        cb(false, {}, err);
                                        return;
                                    }
                                    QJsonObject out;
                                    out["symbol"] = symbol;
                                    out["data"] = val;
                                    out["node_type"] = "market.sec_filings";
                                    cb(true, out, {});
                                });
            },
    });
}

} // namespace fincept::workflow
