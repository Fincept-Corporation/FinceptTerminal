// EdgarTools.cpp — SEC EDGAR MCP tools
// Calls Analytics/sec_edgar/edgar_fetcher.py directly via PythonRunner.
// Completely separate from M&A Analytics.

#include "mcp/tools/EdgarTools.h"

#include "core/logging/Logger.h"
#include "mcp/tools/ThreadHelper.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"

#include <QJsonDocument>

namespace fincept::mcp::tools {

static constexpr const char* TAG = "EdgarTools";
static constexpr const char* SCRIPT = "mcp/edgar/main.py";
[[maybe_unused]] static constexpr int kTimeoutMs = 30000;
static constexpr int kEdgarTtlSec = 30 * 60; // 30 min — SEC filings rarely change intra-day

// ── Helper: run mcp/edgar/main.py synchronously and return parsed JSON ───────

static ToolResult run_edgar(const QStringList& args) {
    // Cache key: join all args
    const QString cache_key = "edgar:" + args.join(":");
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        auto doc = QJsonDocument::fromJson(cached.toString().toUtf8());
        if (!doc.isNull() && doc.isObject())
            return ToolResult::ok_data(doc.object());
    }

    QJsonObject result;
    QString error;

    // Marshal the PythonRunner::run() call onto the runner's thread (main).
    // The previous worker-thread QEventLoop pattern caused QObject parentage
    // violations since QProcess signals fire on the main thread and never
    // wake the worker's loop. See mcp/tools/ThreadHelper.h.
    auto* runner = &fincept::python::PythonRunner::instance();
    detail::run_async_wait(runner, [&](auto signal_done) {
        runner->run(SCRIPT, args, [&, signal_done](const fincept::python::PythonResult& r) {
            if (!r.success) {
                error = r.error.isEmpty() ? r.output : r.error;
            } else {
                QString text = fincept::python::extract_json(r.output);
                QJsonParseError pe;
                auto doc = QJsonDocument::fromJson(text.toUtf8(), &pe);
                if (pe.error != QJsonParseError::NoError) {
                    error = QString("JSON parse error: %1").arg(pe.errorString());
                } else {
                    QJsonObject obj = doc.object();
                    if (obj.value("success").toBool(true) == false)
                        error = obj.value("error").toString("unknown error");
                    else
                        result = obj;
                }
            }
            signal_done();
        });
    });

    if (!error.isEmpty()) {
        LOG_WARN(TAG, QString("edgar error [%1]: %2").arg(args.value(0), error));
        return ToolResult::fail(error);
    }

    fincept::CacheManager::instance().put(
        cache_key, QVariant(QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact))), kEdgarTtlSec,
        "edgar");

    return ToolResult::ok_data(result);
}

// ── Tool definitions ─────────────────────────────────────────────────────────

std::vector<ToolDef> get_edgar_tools() {
    std::vector<ToolDef> tools;

    // ── edgar_resolve_cik ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_resolve_cik";
        t.description = "Resolve a company name or ticker symbol to its SEC EDGAR CIK number.";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"company", QJsonObject{{"type", "string"},
                                                {"description", "Company name or ticker (e.g. AAPL, Apple Inc)"}}}};
        t.input_schema.required = {"company"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString company = args["company"].toString().trimmed();
            if (company.isEmpty())
                return ToolResult::fail("Missing 'company'");
            return run_edgar({"resolve_cik", company}); // → mcp/edgar/main.py resolve_cik
        };
        tools.push_back(std::move(t));
    }

    // ── edgar_get_metadata ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_get_metadata";
        t.description =
            "Fetch SEC EDGAR company metadata: name, ticker, SIC code, state of incorporation, fiscal year end.";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"cik", QJsonObject{{"type", "string"}, {"description", "10-digit SEC CIK number"}}}};
        t.input_schema.required = {"cik"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString cik = args["cik"].toString().trimmed();
            if (cik.isEmpty())
                return ToolResult::fail("Missing 'cik'");
            return run_edgar({"get_company", cik});
        };
        tools.push_back(std::move(t));
    }

    // ── edgar_get_financials ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_get_financials";
        t.description = "Fetch XBRL financials from SEC EDGAR: revenue, EBITDA, net income, debt, cash, shares. "
                        "Returns annual and TTM figures.";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"cik", QJsonObject{{"type", "string"}, {"description", "10-digit SEC CIK number"}}},
                        {"periods", QJsonObject{{"type", "integer"},
                                                {"description", "Number of annual periods to fetch (default: 4)"}}}};
        t.input_schema.required = {"cik"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString cik = args["cik"].toString().trimmed();
            if (cik.isEmpty())
                return ToolResult::fail("Missing 'cik'");
            int periods = args["periods"].toInt(4);
            return run_edgar({"get_financials", cik, QString::number(periods), "true"});
        };
        tools.push_back(std::move(t));
    }

    // ── edgar_get_filing_text ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_get_filing_text";
        t.description =
            "Fetch the full text of the latest SEC filing (10-K, 10-Q, 8-K, etc.) for a company by ticker or CIK.";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol or CIK number"}}},
                        {"form", QJsonObject{{"type", "string"},
                                             {"description", "Filing form type: 10-K, 10-Q, 8-K (default: 10-K)"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            QString form = args["form"].toString().trimmed();
            if (form.isEmpty())
                form = "10-K";
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            return run_edgar({"get_filing_text", ticker, form});
        };
        tools.push_back(std::move(t));
    }

    // ── edgar_search_filings ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_search_filings";
        t.description = "Search recent SEC EDGAR filings for a company by ticker. Specify form type (8-K, 10-K, S-4, "
                        "etc.) and date range.";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol or CIK"}}},
                        {"form", QJsonObject{{"type", "string"}, {"description", "Filing form type (default: 8-K)"}}},
                        {"months_back", QJsonObject{{"type", "integer"},
                                                    {"description", "How many months back to search (default: 12)"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            QString form = args["form"].toString().trimmed();
            if (form.isEmpty())
                form = "8-K";
            int months = args["months_back"].toInt(12);
            return run_edgar({"search_filings", ticker, form, QString::number(months)});
        };
        tools.push_back(std::move(t));
    }

    // ── edgar_calc_multiples ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_calc_multiples";
        t.description = "Calculate valuation multiples (EV/Revenue, EV/EBITDA, implied P/E, price per share) given a "
                        "deal value and target CIK.";
        t.category = "sec-edgar";
        t.input_schema.properties = QJsonObject{
            {"deal_value", QJsonObject{{"type", "number"}, {"description", "Total deal / enterprise value in USD"}}},
            {"cik", QJsonObject{{"type", "string"}, {"description", "10-digit SEC CIK of the target company"}}}};
        t.input_schema.required = {"deal_value", "cik"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            double deal_value = args["deal_value"].toDouble(0);
            QString cik = args["cik"].toString().trimmed();
            if (cik.isEmpty() || deal_value <= 0)
                return ToolResult::fail("Missing or invalid 'deal_value' / 'cik'");
            return run_edgar({"calc_multiples", cik, QString::number(deal_value, 'f', 2)});
        };
        tools.push_back(std::move(t));
    }

    // ── edgar_get_filing_documents ───────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_get_filing_documents";
        t.description = "List all documents (exhibits, attachments) in the latest SEC filing for a company by ticker "
                        "and form type.";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol or CIK"}}},
                        {"form", QJsonObject{{"type", "string"}, {"description", "Filing form type (default: 10-K)"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            QString form = args["form"].toString().trimmed();
            if (form.isEmpty())
                form = "10-K";
            return run_edgar({"get_filing_documents", ticker, form});
        };
        tools.push_back(std::move(t));
    }

    // ── edgar_find_merger_agreement ──────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_find_merger_agreement";
        t.description =
            "Find merger agreement documents (EX-2.1, S-4, DEFM14A) in SEC filings for a company by ticker.";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol or CIK"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            return run_edgar({"get_filing_documents", ticker, "S-4"});
        };
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // 10-K ANNUAL REPORTS
    // ════════════════════════════════════════════════════════════════════

    // ── edgar_10k_sections ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_10k_sections";
        t.description = "Extract specific sections from a company's latest 10-K annual report (business, risk_factors, "
                        "mda, financials, etc.).";
        t.category = "sec-edgar";
        t.input_schema.properties = QJsonObject{
            {"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol (e.g. AAPL)"}}},
            {"sections", QJsonObject{{"type", "string"},
                                     {"description", "Comma-separated sections: business, risk_factors, mda, "
                                                     "financials, controls, compensation, governance, all_items"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            QString sections = args["sections"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            if (sections.isEmpty())
                sections = "business";
            QStringList cmd = {"10k_sections", ticker};
            for (const auto& s : sections.split(","))
                cmd.append(s.trimmed());
            return run_edgar(cmd);
        };
        tools.push_back(std::move(t));
    }

    // ── edgar_10k_full_text ──────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_10k_full_text";
        t.description = "Get the full text of a company's latest 10-K annual filing.";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}},
                        {"max_length", QJsonObject{{"type", "integer"},
                                                   {"description", "Max characters to return (default: 20000)"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            int max_len = args["max_length"].toInt(20000);
            return run_edgar({"10k_full_text", ticker, QString::number(max_len)});
        };
        tools.push_back(std::move(t));
    }

    // ── edgar_10k_search ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_10k_search";
        t.description = "Search within a company's 10-K filing for a specific topic or keyword.";
        t.category = "sec-edgar";
        t.input_schema.properties = QJsonObject{
            {"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}},
            {"query",
             QJsonObject{{"type", "string"}, {"description", "Search query (e.g. revenue recognition, litigation)"}}},
            {"max_results", QJsonObject{{"type", "integer"}, {"description", "Max results (default: 10)"}}}};
        t.input_schema.required = {"ticker", "query"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            QString query = args["query"].toString().trimmed();
            if (ticker.isEmpty() || query.isEmpty())
                return ToolResult::fail("Missing 'ticker' or 'query'");
            int max_results = args["max_results"].toInt(10);
            return run_edgar({"10k_search", ticker, query, QString::number(max_results)});
        };
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // 10-Q QUARTERLY REPORTS
    // ════════════════════════════════════════════════════════════════════

    // ── edgar_10q_sections ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_10q_sections";
        t.description = "Extract specific sections from a company's latest 10-Q quarterly report.";
        t.category = "sec-edgar";
        t.input_schema.properties = QJsonObject{
            {"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}},
            {"sections", QJsonObject{{"type", "string"},
                                     {"description", "Comma-separated sections: part_i_item_1, part_i_item_2 (MD&A), "
                                                     "part_i_item_3, part_ii_item_1, all_items"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            QString sections = args["sections"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            if (sections.isEmpty())
                sections = "part_i_item_2";
            QStringList cmd = {"10q_sections", ticker};
            for (const auto& s : sections.split(","))
                cmd.append(s.trimmed());
            return run_edgar(cmd);
        };
        tools.push_back(std::move(t));
    }

    // ── edgar_10q_full_text ──────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_10q_full_text";
        t.description = "Get the full text of a company's latest 10-Q quarterly filing.";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}},
                        {"max_length", QJsonObject{{"type", "integer"},
                                                   {"description", "Max characters to return (default: 15000)"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            int max_len = args["max_length"].toInt(15000);
            return run_edgar({"10q_full_text", ticker, QString::number(max_len)});
        };
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // 8-K CORPORATE EVENTS
    // ════════════════════════════════════════════════════════════════════

    // ── edgar_8k_events ──────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_8k_events";
        t.description =
            "Get recent 8-K corporate event filings for a company (earnings, M&A, leadership changes, etc.).";
        t.category = "sec-edgar";
        t.input_schema.properties = QJsonObject{
            {"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}},
            {"limit", QJsonObject{{"type", "integer"}, {"description", "Number of events to return (default: 20)"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            int limit = args["limit"].toInt(20);
            return run_edgar({"8k_events", ticker, QString::number(limit)});
        };
        tools.push_back(std::move(t));
    }

    // ── edgar_8k_full_text ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_8k_full_text";
        t.description = "Get the full text of a company's latest 8-K filing.";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}},
                        {"max_length", QJsonObject{{"type", "integer"},
                                                   {"description", "Max characters to return (default: 10000)"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            int max_len = args["max_length"].toInt(10000);
            return run_edgar({"8k_full_text", ticker, QString::number(max_len)});
        };
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // INSIDER TRANSACTIONS (FORM 4)
    // ════════════════════════════════════════════════════════════════════

    // ── edgar_insider_transactions ───────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_insider_transactions";
        t.description = "Get recent insider transactions (Form 4) for a company — buys/sells by executives, directors, "
                        "major shareholders.";
        t.category = "sec-edgar";
        t.input_schema.properties = QJsonObject{
            {"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}},
            {"limit", QJsonObject{{"type", "integer"}, {"description", "Number of transactions (default: 25)"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            int limit = args["limit"].toInt(25);
            return run_edgar({"insider_transactions", ticker, QString::number(limit)});
        };
        tools.push_back(std::move(t));
    }

    // ── edgar_insider_summary ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_insider_summary";
        t.description = "Get a summary of insider buying vs selling activity for a company.";
        t.category = "sec-edgar";
        t.input_schema.properties = QJsonObject{
            {"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}},
            {"limit", QJsonObject{{"type", "integer"}, {"description", "Transactions to summarize (default: 50)"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            int limit = args["limit"].toInt(50);
            return run_edgar({"insider_summary", ticker, QString::number(limit)});
        };
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // 13F INSTITUTIONAL HOLDINGS
    // ════════════════════════════════════════════════════════════════════

    // ── edgar_13f_holdings ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_13f_holdings";
        t.description =
            "Get 13F institutional holdings for a company across recent quarters (hedge funds, mutual funds, etc.).";
        t.category = "sec-edgar";
        t.input_schema.properties = QJsonObject{
            {"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}},
            {"quarters", QJsonObject{{"type", "integer"}, {"description", "Number of recent quarters (default: 2)"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            int quarters = args["quarters"].toInt(2);
            return run_edgar({"13f_holdings", ticker, QString::number(quarters)});
        };
        tools.push_back(std::move(t));
    }

    // ── edgar_13f_top_holdings ───────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_13f_top_holdings";
        t.description = "Get the top institutional holders of a company's stock from 13F filings.";
        t.category = "sec-edgar";
        t.input_schema.properties = QJsonObject{
            {"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}},
            {"top_n", QJsonObject{{"type", "integer"}, {"description", "Number of top holders (default: 20)"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            int top_n = args["top_n"].toInt(20);
            return run_edgar({"13f_top_holdings", ticker, QString::number(top_n)});
        };
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // 10-K EXTRAS
    // ════════════════════════════════════════════════════════════════════

    // ── edgar_10k_exhibits ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_10k_exhibits";
        t.description = "List all exhibits attached to a company's latest 10-K filing.";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            return run_edgar({"10k_exhibits", ticker});
        };
        tools.push_back(std::move(t));
    }

    // ── edgar_10k_metadata ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_10k_metadata";
        t.description =
            "Get metadata for a company's latest 10-K filing: accession number, filing date, period of report.";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            return run_edgar({"10k_metadata", ticker});
        };
        tools.push_back(std::move(t));
    }

    // ── edgar_10k_markdown ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_10k_markdown";
        t.description = "Get the latest 10-K filing rendered as markdown text.";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            return run_edgar({"10k_markdown", ticker});
        };
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // 10-Q EXTRAS
    // ════════════════════════════════════════════════════════════════════

    // ── edgar_10q_metadata ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_10q_metadata";
        t.description =
            "Get metadata for a company's latest 10-Q filing: accession number, filing date, period of report.";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            return run_edgar({"10q_metadata", ticker});
        };
        tools.push_back(std::move(t));
    }

    // ── edgar_10q_markdown ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_10q_markdown";
        t.description = "Get the latest 10-Q filing rendered as markdown text.";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            return run_edgar({"10q_markdown", ticker});
        };
        tools.push_back(std::move(t));
    }

    // ── edgar_10q_search ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_10q_search";
        t.description = "Search within a company's latest 10-Q filing for a specific topic or keyword.";
        t.category = "sec-edgar";
        t.input_schema.properties = QJsonObject{
            {"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}},
            {"query", QJsonObject{{"type", "string"}, {"description", "Search query"}}},
            {"max_results", QJsonObject{{"type", "integer"}, {"description", "Max results (default: 10)"}}}};
        t.input_schema.required = {"ticker", "query"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            QString query = args["query"].toString().trimmed();
            if (ticker.isEmpty() || query.isEmpty())
                return ToolResult::fail("Missing 'ticker' or 'query'");
            int max_results = args["max_results"].toInt(10);
            return run_edgar({"10q_search", ticker, query, QString::number(max_results)});
        };
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // 8-K EXTRAS
    // ════════════════════════════════════════════════════════════════════

    // ── edgar_8k_events_categorized ──────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_8k_events_categorized";
        t.description =
            "Get recent 8-K corporate events categorized by type (M&A, earnings, leadership, bankruptcy, etc.).";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}},
                        {"limit", QJsonObject{{"type", "integer"}, {"description", "Number of events (default: 30)"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            int limit = args["limit"].toInt(30);
            return run_edgar({"8k_events_categorized", ticker, QString::number(limit)});
        };
        tools.push_back(std::move(t));
    }

    // ── edgar_8k_search ──────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_8k_search";
        t.description = "Search within a company's recent 8-K filings for a specific topic or keyword.";
        t.category = "sec-edgar";
        t.input_schema.properties = QJsonObject{
            {"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}},
            {"query", QJsonObject{{"type", "string"}, {"description", "Search query"}}},
            {"max_results", QJsonObject{{"type", "integer"}, {"description", "Max results (default: 10)"}}}};
        t.input_schema.required = {"ticker", "query"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            QString query = args["query"].toString().trimmed();
            if (ticker.isEmpty() || query.isEmpty())
                return ToolResult::fail("Missing 'ticker' or 'query'");
            int max_results = args["max_results"].toInt(10);
            return run_edgar({"8k_search", ticker, query, QString::number(max_results)});
        };
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // INSIDER EXTRAS
    // ════════════════════════════════════════════════════════════════════

    // ── edgar_insider_transactions_detailed ──────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_insider_transactions_detailed";
        t.description = "Get detailed insider transactions (Form 4) with full transaction breakdowns per insider.";
        t.category = "sec-edgar";
        t.input_schema.properties = QJsonObject{
            {"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}},
            {"limit", QJsonObject{{"type", "integer"}, {"description", "Number of transactions (default: 25)"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            int limit = args["limit"].toInt(25);
            return run_edgar({"insider_transactions_detailed", ticker, QString::number(limit)});
        };
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // 13F EXTRAS
    // ════════════════════════════════════════════════════════════════════

    // ── edgar_13f_manager_info ───────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_13f_manager_info";
        t.description = "Get fund manager information from 13F filings for a company.";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            return run_edgar({"13f_manager_info", ticker});
        };
        tools.push_back(std::move(t));
    }

    // ── edgar_13f_summary ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_13f_summary";
        t.description = "Get a summary of 13F institutional holdings for a company.";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            return run_edgar({"13f_summary", ticker});
        };
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // FINANCIALS EXTRAS
    // ════════════════════════════════════════════════════════════════════

    // ── edgar_get_financial_metrics ──────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_get_financial_metrics";
        t.description =
            "Get key financial metrics for a company: revenue, net income, EPS, profit margin from SEC XBRL data.";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            return run_edgar({"get_financial_metrics", ticker});
        };
        tools.push_back(std::move(t));
    }

    // ── edgar_get_xbrl_statements ────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_get_xbrl_statements";
        t.description = "Get raw XBRL financial statements for a company by form type (10-K or 10-Q).";
        t.category = "sec-edgar";
        t.input_schema.properties = QJsonObject{
            {"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}},
            {"form", QJsonObject{{"type", "string"}, {"description", "Form type: 10-K or 10-Q (default: 10-K)"}}}};
        t.input_schema.required = {"ticker"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return ToolResult::fail("Missing 'ticker'");
            QString form = args["form"].toString().trimmed();
            if (form.isEmpty())
                form = "10-K";
            return run_edgar({"get_xbrl_statements", ticker, form});
        };
        tools.push_back(std::move(t));
    }

    // ── edgar_get_company_facts ──────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_get_company_facts";
        t.description = "Get all XBRL company facts from SEC EDGAR for a company by CIK.";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"cik", QJsonObject{{"type", "string"}, {"description", "10-digit SEC CIK number"}}}};
        t.input_schema.required = {"cik"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString cik = args["cik"].toString().trimmed();
            if (cik.isEmpty())
                return ToolResult::fail("Missing 'cik'");
            return run_edgar({"get_company_facts", cik});
        };
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // COMPANY SEARCH
    // ════════════════════════════════════════════════════════════════════

    // ── edgar_find_company ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_find_company";
        t.description =
            "Search SEC EDGAR for companies by name with fuzzy matching. Returns list of matches with ticker and CIK.";
        t.category = "sec-edgar";
        t.input_schema.properties = QJsonObject{
            {"query",
             QJsonObject{{"type", "string"}, {"description", "Company name or partial name (e.g. Apple, Tesla)"}}},
            {"top_n", QJsonObject{{"type", "integer"}, {"description", "Number of results (default: 10)"}}}};
        t.input_schema.required = {"query"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString query = args["query"].toString().trimmed();
            if (query.isEmpty())
                return ToolResult::fail("Missing 'query'");
            int top_n = args["top_n"].toInt(10);
            return run_edgar({"find_company", query, QString::number(top_n)});
        };
        tools.push_back(std::move(t));
    }

    // ── edgar_resolve_ticker ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_resolve_ticker";
        t.description = "Resolve a company name to its ticker symbol.";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"query", QJsonObject{{"type", "string"}, {"description", "Company name (e.g. Apple Inc)"}}}};
        t.input_schema.required = {"query"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString query = args["query"].toString().trimmed();
            if (query.isEmpty())
                return ToolResult::fail("Missing 'query'");
            return run_edgar({"resolve_ticker", query});
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
