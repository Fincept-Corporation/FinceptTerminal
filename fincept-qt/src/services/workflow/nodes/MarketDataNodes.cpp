#include "services/workflow/nodes/MarketDataNodes.h"

#include "python/PythonRunner.h"
#include "screens/economics/panels/EconomicsPresets.h"
#include "services/workflow/NodeRegistry.h"

#include <QDate>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

#include <cmath>

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

// sec_data.py (shared with the MCP data tools, so it is not edited here) prints Python's bare
// NaN literal for missing cells — e.g. "isXBRLNumeric": NaN — which is not JSON, so the
// runner's strict JSON check rejects every filing list. Run it unchecked and map NaN/Infinity
// to null before parsing.
void mkt_run_sec_json(const QStringList& args, std::function<void(bool, QJsonValue, QString)> cb) {
    PythonRunner::RunOptions opts;
    opts.expect_json = false;
    PythonRunner::instance().run_with_options("sec_data.py", args, opts, [cb](const PythonResult& res) {
        if (!res.success) {
            cb(false, {}, res.error.isEmpty() ? QStringLiteral("sec_data.py failed") : res.error);
            return;
        }
        QString json_str = extract_json(res.output).trimmed();
        static const QRegularExpression non_json_re(QStringLiteral(R"((?<=[:\[,])\s*-?(?:NaN|Infinity)(?=\s*[,\]}]))"));
        json_str.replace(non_json_re, QStringLiteral(" null"));
        const auto doc = QJsonDocument::fromJson(json_str.toUtf8());
        if (!doc.isObject()) {
            cb(false, {}, "Invalid JSON from sec_data.py: " + res.output.left(200));
            return;
        }
        const QJsonObject obj = doc.object();
        if (obj.contains("error") || !obj.value("success").toBool(true)) {
            const QJsonValue e = obj.value("error");
            const QString msg = e.isObject() ? e.toObject().value("error").toString() : e.toString();
            cb(false, {}, msg.isEmpty() ? QStringLiteral("SEC request failed") : msg);
            return;
        }
        cb(true, obj, {});
    });
}

// Best bid / ask of one delayed quote. A missing or zero side means "not quoted", never a
// price: the depth node used to emit the last trade as both bid and ask whenever the quote had
// none (yfinance_data.py's quote never has), so every spread downstream was silently zero.
// Returns an empty object when neither side is quoted.
QJsonObject mkt_top_of_book(const QString& symbol, int requested_depth, const QJsonObject& quote,
                            const QString& last_key, const QString& source) {
    const auto side = [&quote](const char* key) {
        const QJsonValue v = quote.value(QLatin1String(key));
        return v.isDouble() && v.toDouble() > 0.0 ? v : QJsonValue();
    };
    const QJsonValue bid = side("bid");
    const QJsonValue ask = side("ask");
    if (bid.isNull() && ask.isNull())
        return {};
    const bool two_sided = !bid.isNull() && !ask.isNull();
    const auto price = [](double v) { return QJsonValue(std::round(v * 1e8) / 1e8); };
    QJsonObject out;
    out["symbol"] = symbol;
    out["bid"] = bid;
    out["ask"] = ask;
    out["bid_size"] = bid.isNull() ? QJsonValue() : quote.value("bid_size");
    out["ask_size"] = ask.isNull() ? QJsonValue() : quote.value("ask_size");
    out["spread"] = two_sided ? price(ask.toDouble() - bid.toDouble()) : QJsonValue();
    out["mid"] = two_sided ? price((ask.toDouble() + bid.toDouble()) / 2.0) : QJsonValue();
    out["last"] = quote.value(last_key);
    out["levels"] = 1;
    out["requested_depth"] = requested_depth;
    out["source"] = source;
    out["note"] = "Top of book only, from a delayed quote. Level 2 depth is not published by this source.";
    out["node_type"] = "market.get_depth";
    return out;
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
        .description = "Best bid / ask with sizes from a delayed quote (Level 2 depth is not available)",
        .icon_text = "$",
        .accent_color = "#2563eb",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::MarketData}},
        .parameters =
            {
                {"symbol", "Symbol", "string", "AAPL", {}, "Ticker symbol", true},
                {"depth", "Depth", "number", 10, {}, "Levels wanted (only the top level is available)"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                const QString symbol = params.value("symbol").toString("AAPL").trimmed().toUpper();
                if (symbol.isEmpty()) {
                    cb(false, {}, "Symbol is required for market depth");
                    return;
                }
                if (symbol.startsWith(QLatin1Char('^'))) {
                    cb(false, {}, QString("%1 is an index: indices have no bid / ask").arg(symbol));
                    return;
                }
                const int depth = qMax(1, static_cast<int>(params.value("depth").toDouble(10)));

                // Yahoo path: yfinance_data.py's quote carries a bid / ask only once the script
                // exposes them; until then this ends in a clear "not quoted" error.
                auto from_yahoo = [cb, symbol, depth](const QString& cboe_note) {
                    run_python_json(
                        "yfinance_data.py", {"quote", symbol},
                        [cb, symbol, depth, cboe_note](bool ok, QJsonValue val, QString err) {
                            if (!ok) {
                                cb(false, {}, QString("No quote for %1%2: %3").arg(symbol, cboe_note, err));
                                return;
                            }
                            const QJsonObject quote = val.toObject();
                            if (quote.value("quote_type").toString().compare("INDEX", Qt::CaseInsensitive) == 0) {
                                cb(false, {}, QString("%1 is an index: indices have no bid / ask").arg(symbol));
                                return;
                            }
                            const QJsonObject out = mkt_top_of_book(symbol, depth, quote, "price", "Yahoo Finance");
                            if (out.isEmpty()) {
                                cb(false, {},
                                   QString("No bid / ask is quoted for %1 by Yahoo Finance%2. Level 2 depth is "
                                           "not available from these sources.")
                                       .arg(symbol, cboe_note));
                                return;
                            }
                            cb(true, out, {});
                        });
                };

                // Cboe's delayed quote has a real bid / ask with sizes for US-listed stocks and ETFs.
                static const QRegularExpression us_ticker_re(QStringLiteral(R"(^[A-Z]{1,5}([.-][A-Z]{1,2})?$)"));
                if (!us_ticker_re.match(symbol).hasMatch()) {
                    from_yahoo(QString());
                    return;
                }
                QString cboe_symbol = symbol;
                cboe_symbol.replace(QLatin1Char('-'), QLatin1Char('.')); // Yahoo BRK-B is Cboe BRK.B
                run_python_json("cboe_data.py", {"equity_quote", cboe_symbol},
                                [cb, symbol, depth, from_yahoo](bool ok, QJsonValue val, QString err) {
                                    if (ok) {
                                        const QJsonObject quote = val.toObject().value("data").toObject();
                                        if (quote.value("security_type").toString() == QLatin1String("index")) {
                                            cb(false, {},
                                               QString("%1 is an index: indices have no bid / ask").arg(symbol));
                                            return;
                                        }
                                        const QJsonObject out =
                                            mkt_top_of_book(symbol, depth, quote, "current_price", "Cboe (delayed)");
                                        if (!out.isEmpty()) {
                                            cb(true, out, {});
                                            return;
                                        }
                                        err = QStringLiteral("no bid / ask in the Cboe quote");
                                    }
                                    from_yahoo(QString(" (Cboe: %1)").arg(err.left(160)));
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
                {"type",
                 "Type",
                 "select",
                 "overview",
                 {"overview", "income", "balance", "cashflow", "earnings"},
                 "income / balance / cashflow: annual statements; earnings: revenue, net income and EPS"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                const QString symbol = params.value("symbol").toString("AAPL");
                const QString type = params.value("type").toString("overview");
                if (type == "overview") {
                    run_python_json("yfinance_data.py", {"info", symbol}, cb);
                    return;
                }
                // financials -> {income_statement | balance_sheet | cash_flow: {period: {line: value}}}.
                // The node looked for "income" / "balance" / "cashflow" / "earnings" keys, which never
                // exist, so every type silently returned all three statements.
                static const QHash<QString, QString> statement_keys = {{"income", "income_statement"},
                                                                       {"balance", "balance_sheet"},
                                                                       {"cashflow", "cash_flow"},
                                                                       {"earnings", "income_statement"}};
                const QString key = statement_keys.value(type);
                if (key.isEmpty()) {
                    cb(false, {}, QString("Unknown fundamentals type '%1'").arg(type));
                    return;
                }
                run_python_json("yfinance_data.py", {"financials", symbol},
                                [cb, symbol, type, key](bool ok, QJsonValue val, QString err) {
                                    if (!ok) {
                                        cb(false, {}, err);
                                        return;
                                    }
                                    // Yahoo has no separate earnings table any more (yfinance: look for "Net Income"
                                    // in the income statement), so "earnings" keeps those lines of each period.
                                    static const QStringList earnings_lines = {"Total Revenue", "Net Income",
                                                                               "Basic EPS", "Diluted EPS"};
                                    const bool earnings = type == QLatin1String("earnings");
                                    const QJsonObject statement = val.toObject().value(key).toObject();
                                    QJsonObject periods;
                                    for (auto it = statement.constBegin(); it != statement.constEnd(); ++it) {
                                        QJsonObject lines = it.value().toObject();
                                        if (earnings) {
                                            QJsonObject picked;
                                            for (const QString& line : earnings_lines) {
                                                if (lines.contains(line))
                                                    picked[line] = lines.value(line);
                                            }
                                            lines = picked;
                                        }
                                        if (!lines.isEmpty())
                                            periods[it.key()] = lines;
                                    }
                                    if (periods.isEmpty()) {
                                        cb(false, {}, QString("Yahoo Finance has no %1 data for %2").arg(type, symbol));
                                        return;
                                    }
                                    QJsonObject out;
                                    out["symbol"] = symbol;
                                    out["type"] = type;
                                    out["frequency"] = "annual";
                                    out["statement"] = key;
                                    out[earnings ? QStringLiteral("earnings") : key] = periods;
                                    out["source"] = "Yahoo Finance";
                                    out["node_type"] = "market.get_fundamentals";
                                    cb(true, out, {});
                                });
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
                {"preset", "Preset", "select", "GDPC1", fincept::screens::fred_preset_series_ids(),
                 "Pick a common series, or use Series ID below for any FRED code"},
                {"series_id",
                 "Series ID (override)",
                 "string",
                 "",
                 {},
                 "Optional FRED series id (e.g. GS10). Overrides Preset when set."},
                {"start_date", "Start Date", "string", "", {}, "YYYY-MM-DD (optional)"},
                {"end_date", "End Date", "string", "", {}, "YYYY-MM-DD (optional)"},
                {"frequency",
                 "Frequency",
                 "select",
                 "",
                 {"", "d", "w", "m", "q", "a"},
                 "Output frequency: d=daily, w=weekly, m=monthly, q=quarterly, a=annual"},
                {"transform",
                 "Transform",
                 "select",
                 "",
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
                {"symbol", "Symbol", "string", "", {}, "Ticker; empty = broad US market news (the ^GSPC feed)"},
                {"limit", "Limit", "number", 10, {}, "Number of articles"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                const QString symbol = params.value("symbol").toString().trimmed();
                // Yahoo news is per ticker: an empty symbol made yfinance raise "Empty ticker name",
                // and that three-key error object passed run_python_json's check as a success with
                // no articles. Market-wide news comes from the S&P 500's feed instead.
                const QString query = symbol.isEmpty() ? QStringLiteral("^GSPC") : symbol;
                const QString limit =
                    QString::number(qBound(1, static_cast<int>(params.value("limit").toDouble(10)), 100));
                run_python_json("yfinance_data.py", {"news", query, limit},
                                [cb, symbol, query](bool ok, QJsonValue val, QString err) {
                                    const QJsonObject obj = val.toObject();
                                    const QString script_error = obj.value("error").toString();
                                    if (!ok || !script_error.isEmpty()) {
                                        cb(false, {},
                                           QString("News for %1 failed: %2").arg(query, ok ? script_error : err));
                                        return;
                                    }
                                    const QJsonArray articles = obj.value("articles").toArray();
                                    QJsonObject out;
                                    out["symbol"] = symbol;
                                    out["query_symbol"] = query;
                                    out["scope"] = symbol.isEmpty() ? "market" : "symbol";
                                    out["count"] = articles.size();
                                    out["articles"] = articles;
                                    out["source"] = "Yahoo Finance";
                                    out["node_type"] = "market.get_news";
                                    cb(true, out, {});
                                });
            },
    });

    // ── Tier 1 additions ───────────────────────────────────────────

    registry.register_type({
        .type_id = "market.get_options_chain",
        .display_name = "Options Chain",
        .category = "Market Data",
        .description = "Fetch an options chain with greeks (Cboe delayed quotes: US-listed stocks, ETFs, indices)",
        .icon_text = "$",
        .accent_color = "#2563eb",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::OptionsData}},
        .parameters =
            {
                {"symbol", "Symbol", "string", "AAPL", {}, "US-listed stock, ETF or index (^SPX, ^VIX)", true},
                {"expiry", "Expiry", "string", "nearest", {}, "YYYY-MM-DD, 'nearest' (default) or 'all'"},
                {"type", "Type", "select", "both", {"calls", "puts", "both"}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                const QString symbol = params.value("symbol").toString("AAPL").trimmed().toUpper();
                if (symbol.isEmpty()) {
                    cb(false, {}, "Symbol is required for an options chain");
                    return;
                }
                QString expiry = params.value("expiry").toString().trimmed().toLower();
                if (expiry.isEmpty())
                    expiry = QStringLiteral("nearest");
                const QString type = params.value("type").toString("both");
                // This node returned yfinance company info (no options at all) and ignored expiry and
                // type. cboe_data.py serves the chain (bid/ask, IV, greeks, OI) filtered by both.
                run_python_json("cboe_data.py", {"options_chains", symbol, expiry, type},
                                [cb, symbol](bool ok, QJsonValue val, QString err) {
                                    if (!ok) {
                                        cb(false, {}, QString("Options chain for %1 failed: %2").arg(symbol, err));
                                        return;
                                    }
                                    const QJsonObject data = val.toObject().value("data").toObject();
                                    QJsonArray calls;
                                    QJsonArray puts;
                                    for (const QJsonValue& o : data.value("options").toArray())
                                        (o.toObject().value("option_type").toString() == QLatin1String("call") ? calls
                                                                                                               : puts)
                                            .append(o);
                                    QJsonObject out;
                                    out["symbol"] = symbol;
                                    out["underlying"] = data.value("metadata");
                                    out["expirations"] = data.value("expirations");
                                    out["expiry"] = data.value("expiration_filter");
                                    out["type"] = data.value("option_type_filter");
                                    out["count"] = calls.size() + puts.size();
                                    out["calls"] = calls;
                                    out["puts"] = puts;
                                    out["units"] = data.value("units");
                                    out["source"] = "Cboe (delayed)";
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
                {"exchange", "Exchange", "select", "kraken", {"kraken", "binance", "coinbase", "hyperliquid"}, ""},
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
                {"market_cap_min",
                 "Min Market Cap (millions)",
                 "number",
                 1000,
                 {},
                 "In the listing's quote currency (USD for US). 0 = no limit"},
                {"pe_max", "Max P/E Ratio", "number", 30, {}, "Trailing P/E between 0 and this. 0 = no limit"},
                {"volume_min", "Min Avg Volume", "number", 500000, {}, "3-month average daily volume. 0 = no limit"},
                {"sector",
                 "Sector",
                 "select",
                 "any",
                 {"any", "technology", "healthcare", "finance", "energy", "consumer", "industrial", "utilities",
                  "materials", "real_estate"},
                 ""},
                {"country", "Country", "select", "US", {"US", "UK", "IN", "JP", "DE", "FR", "CA", "AU"}, ""},
                {"limit", "Max Results", "number", 50, {}, "Up to 250"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                // Yahoo Finance screener through scripts/screener_data.py (the Screener screen's engine).
                // This node used to run a Yahoo text search for the word "market" (or the sector name)
                // and ignored every criterion.
                static const QHash<QString, QStringList> sectors = {
                    {"technology", {"Technology"}},
                    {"healthcare", {"Healthcare"}},
                    {"finance", {"Financial Services"}},
                    {"energy", {"Energy"}},
                    {"consumer", {"Consumer Cyclical", "Consumer Defensive"}},
                    {"industrial", {"Industrials"}},
                    {"utilities", {"Utilities"}},
                    {"materials", {"Basic Materials"}},
                    {"real_estate", {"Real Estate"}}};
                static const QHash<QString, QString> regions = {{"US", "us"}, {"UK", "gb"}, {"IN", "in"}, {"JP", "jp"},
                                                                {"DE", "de"}, {"FR", "fr"}, {"CA", "ca"}, {"AU", "au"}};
                const QString country = params.value("country").toString("US");
                const QString region = regions.value(country);
                if (region.isEmpty()) {
                    cb(false, {}, QString("Unsupported country '%1'").arg(country));
                    return;
                }
                QJsonObject criteria;
                criteria["region"] = QJsonArray{region};
                const QString sector = params.value("sector").toString("any");
                if (sector != QLatin1String("any")) {
                    if (!sectors.contains(sector)) {
                        cb(false, {}, QString("Unsupported sector '%1'").arg(sector));
                        return;
                    }
                    criteria["sector"] = QJsonArray::fromStringList(sectors.value(sector));
                }
                QJsonArray filters;
                const double market_cap_min = params.value("market_cap_min").toDouble(1000);
                if (market_cap_min > 0)
                    filters.append(
                        QJsonObject{{"field", "market_cap"}, {"op", "gte"}, {"value", market_cap_min * 1e6}});
                const double pe_max = params.value("pe_max").toDouble(30);
                if (pe_max > 0)
                    filters.append(QJsonObject{{"field", "pe"}, {"op", "btwn"}, {"value", QJsonArray{0, pe_max}}});
                const double volume_min = params.value("volume_min").toDouble(500000);
                if (volume_min > 0)
                    filters.append(QJsonObject{{"field", "avg_volume_3m"}, {"op", "gte"}, {"value", volume_min}});
                criteria["filters"] = filters;
                const int limit = qBound(1, static_cast<int>(params.value("limit").toDouble(50)), 250);
                const QString criteria_json = QString::fromUtf8(QJsonDocument(criteria).toJson(QJsonDocument::Compact));
                run_python_json("screener_data.py",
                                {"custom", criteria_json, QString::number(limit), "market_cap", "desc"},
                                [cb](bool ok, QJsonValue val, QString err) {
                                    if (!ok) {
                                        cb(false, {}, "Screener failed: " + err);
                                        return;
                                    }
                                    QJsonObject out = val.toObject();
                                    out["results"] = out.value("rows"); // the key this node's old output used
                                    out["node_type"] = "market.screener";
                                    cb(true, out, {});
                                });
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
                // This node returned yfinance company info (nothing to do with insiders).
                // It now lists the company's real Form 4 filings from SEC EDGAR, newest first.
                // sec_data.py's own insider_trading command is a stub (fixed placeholder rows),
                // and its date filters corrupt the result, so the Days Back cut-off is applied here.
                const int days_back = qBound(1, static_cast<int>(params.value("days_back").toDouble(30)), 3650);
                const QString cutoff = QDate::currentDate().addDays(-days_back).toString(Qt::ISODate);
                mkt_run_sec_json({"company_filings", symbol, "", "4", "", "", "100"},
                                 [cb, symbol, days_back, cutoff](bool ok, QJsonValue val, QString err) {
                                     if (!ok) {
                                         cb(false, {}, err);
                                         return;
                                     }
                                     QJsonArray recent;
                                     for (const QJsonValue& row : val.toObject().value("data").toArray()) {
                                         if (row.toObject().value("filingDate").toString() >= cutoff)
                                             recent.append(row);
                                     }
                                     QJsonObject out;
                                     out["symbol"] = symbol;
                                     out["form"] = "4";
                                     out["days_back"] = days_back;
                                     out["count"] = recent.size();
                                     out["filings"] = recent;
                                     out["data"] = recent;
                                     out["source"] = "SEC EDGAR";
                                     out["note"] = "Form 4 filing index. Transaction details (buy/sell, shares, value) "
                                                   "are not parsed, so the Type and Min Value filters do not apply.";
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
                // Filing Type and Limit were ignored and the node returned yfinance company info.
                // sec_data.py lists the real EDGAR filings (form, filing date, document URLs).
                const QString filing_type = params.value("filing_type").toString("10-K");
                const int limit = qBound(1, static_cast<int>(params.value("limit").toDouble(10)), 100);
                const QString form = filing_type == "all" ? QString() : (filing_type == "13F" ? "13F-HR" : filing_type);
                mkt_run_sec_json({"company_filings", symbol, "", form, "", "", QString::number(limit)},
                                 [cb, symbol, filing_type](bool ok, QJsonValue val, QString err) {
                                     if (!ok) {
                                         cb(false, {}, err);
                                         return;
                                     }
                                     const QJsonArray rows = val.toObject().value("data").toArray();
                                     QJsonObject out;
                                     out["symbol"] = symbol;
                                     out["filing_type"] = filing_type;
                                     out["count"] = rows.size();
                                     out["filings"] = rows;
                                     out["data"] = rows;
                                     out["source"] = "SEC EDGAR";
                                     out["node_type"] = "market.sec_filings";
                                     cb(true, out, {});
                                 });
            },
    });
}

} // namespace fincept::workflow
