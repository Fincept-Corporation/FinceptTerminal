// EdgarTools.cpp — SEC EDGAR MCP tools
// Calls mcp/edgar/main.py directly via PythonRunner.
// Completely separate from M&A Analytics.

#include "mcp/tools/EdgarTools.h"

#include "mcp/AsyncDispatch.h"
#include "services/equity/SecFilingsService.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>
#include <cstring>
#include <memory>

namespace fincept::mcp::tools {

namespace edgar_tools_detail {

constexpr int kEdgarTtlSec = 30 * 60; // 30 min — SEC filings rarely change intra-day
// Full-text / Form 4 / 13F pulls download and parse many documents; with SecFilingsService
// queueing behind PythonRunner's 3-process cap a call can take a minute. Declaring it
// (> kMcpAsyncJobThresholdMs) makes every EDGAR tool backgroundable (job receipt + cancel)
// instead of holding the provider's HTTP turn open.
constexpr int kEdgarToolTimeoutMs = 120000;

// Integer tool args are interpolated straight into the script's argv, so a negative or
// absurd value (limit=-1, max_length=999999999) went to the Python side unchecked. Clamp
// each to a sane window; the bounds are per-argument-name because they are the same
// concept in every tool that uses them.
int edgar_int_arg(const QJsonObject& args, const char* key, int fallback) {
    struct Bounds {
        const char* name;
        int lo;
        int hi;
    };
    static const Bounds kBounds[] = {
        {"periods", 1, 20},      {"max_length", 500, 200000}, {"max_results", 1, 50}, {"limit", 1, 200},
        {"quarters", 1, 8},      {"top_n", 1, 100},           {"months_back", 1, 120},
    };
    const int v = args[QLatin1String(key)].toInt(fallback);
    for (const auto& b : kBounds) {
        if (std::strcmp(b.name, key) == 0)
            return std::clamp(v, b.lo, b.hi);
    }
    return v;
}

/// Builds the mcp/edgar/main.py argv from the tool arguments; returns an error message
/// (and leaves argv empty) when they are invalid.
using EdgarArgv = std::function<QString(const QJsonObject& args, QStringList& argv)>;

/// Async handler shared by every EDGAR tool: SecFilingsService runs the script through
/// PythonRunner (never blocking a worker thread), caches the result and coalesces an
/// identical request already in flight (e.g. from the Equity Research FILINGS tab).
void set_edgar_handler(ToolDef& t, EdgarArgv build) {
    t.default_timeout_ms = kEdgarToolTimeoutMs;
    t.async_handler = [build](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
        QStringList argv;
        const QString err = build(args, argv);
        auto* svc = &services::equity::SecFilingsService::instance();
        AsyncDispatch::callback_to_promise(svc, std::move(ctx), promise, [svc, argv, err](auto resolve) {
            if (!err.isEmpty() || argv.isEmpty()) {
                resolve(ToolResult::fail(err.isEmpty() ? QStringLiteral("Invalid arguments") : err));
                return;
            }
            svc->run_command(argv, kEdgarTtlSec, [resolve](bool ok, const QJsonObject& payload, const QString& error) {
                resolve(ok ? ToolResult::ok_data(payload) : ToolResult::fail(error));
            });
        });
    };
}

} // namespace edgar_tools_detail

using namespace edgar_tools_detail;

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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString company = args["company"].toString().trimmed();
            if (company.isEmpty())
                return QStringLiteral("Missing 'company'");
            argv = QStringList{"resolve_cik", company};
            return {}; // → mcp/edgar/main.py resolve_cik
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString cik = args["cik"].toString().trimmed();
            if (cik.isEmpty())
                return QStringLiteral("Missing 'cik'");
            argv = QStringList{"get_company", cik};
            return {};
        });
        tools.push_back(std::move(t));
    }

    // ── edgar_get_financials ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_get_financials";
        t.description = "Fetch the latest XBRL financial statements (balance sheet, income statement, cash flow) "
                        "from SEC EDGAR as full statement tables. For a compact latest-10-K/10-Q metric summary "
                        "with YoY use edgar_key_financials.";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"cik", QJsonObject{{"type", "string"}, {"description", "10-digit SEC CIK number"}}},
                        {"periods", QJsonObject{{"type", "integer"},
                                                {"description", "Number of annual periods to fetch (default: 4)"}}}};
        t.input_schema.required = {"cik"};
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString cik = args["cik"].toString().trimmed();
            if (cik.isEmpty())
                return QStringLiteral("Missing 'cik'");
            int periods = edgar_int_arg(args, "periods", 4);
            argv = QStringList{"get_financials", cik, QString::number(periods), "true"};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            QString form = args["form"].toString().trimmed();
            if (form.isEmpty())
                form = "10-K";
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            argv = QStringList{"get_filing_text", ticker, form};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            QString form = args["form"].toString().trimmed();
            if (form.isEmpty())
                form = "8-K";
            int months = edgar_int_arg(args, "months_back", 12);
            argv = QStringList{"search_filings", ticker, form, QString::number(months)};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            double deal_value = args["deal_value"].toDouble(0);
            QString cik = args["cik"].toString().trimmed();
            if (cik.isEmpty() || deal_value <= 0)
                return QStringLiteral("Missing or invalid 'deal_value' / 'cik'");
            argv = QStringList{"calc_multiples", cik, QString::number(deal_value, 'f', 2)};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            QString form = args["form"].toString().trimmed();
            if (form.isEmpty())
                form = "10-K";
            argv = QStringList{"get_filing_documents", ticker, form};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            argv = QStringList{"get_filing_documents", ticker, "S-4"};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            QString sections = args["sections"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            if (sections.isEmpty())
                sections = "business";
            QStringList cmd = {"10k_sections", ticker};
            for (const auto& s : sections.split(","))
                cmd.append(s.trimmed());
            argv = cmd;
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            int max_len = edgar_int_arg(args, "max_length", 20000);
            argv = QStringList{"10k_full_text", ticker, QString::number(max_len)};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            QString query = args["query"].toString().trimmed();
            if (ticker.isEmpty() || query.isEmpty())
                return QStringLiteral("Missing 'ticker' or 'query'");
            int max_results = edgar_int_arg(args, "max_results", 10);
            argv = QStringList{"10k_search", ticker, query, QString::number(max_results)};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            QString sections = args["sections"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            if (sections.isEmpty())
                sections = "part_i_item_2";
            QStringList cmd = {"10q_sections", ticker};
            for (const auto& s : sections.split(","))
                cmd.append(s.trimmed());
            argv = cmd;
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            int max_len = edgar_int_arg(args, "max_length", 15000);
            argv = QStringList{"10q_full_text", ticker, QString::number(max_len)};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            int limit = edgar_int_arg(args, "limit", 20);
            argv = QStringList{"8k_events", ticker, QString::number(limit)};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            int max_len = edgar_int_arg(args, "max_length", 10000);
            argv = QStringList{"8k_full_text", ticker, QString::number(max_len)};
            return {};
        });
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // INSIDER TRANSACTIONS (FORM 4)
    // ════════════════════════════════════════════════════════════════════

    // ── edgar_insider_transactions ───────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_insider_transactions";
        t.description = "List recent Form 4 filings for a company with the insider's name, role and the "
                        "transaction codes filed (P = open-market buy, S = sale, A grant, M option exercise, "
                        "F tax withholding, G gift).";
        t.category = "sec-edgar";
        t.input_schema.properties = QJsonObject{
            {"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}},
            {"limit", QJsonObject{{"type", "integer"}, {"description", "Number of transactions (default: 25)"}}}};
        t.input_schema.required = {"ticker"};
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            int limit = edgar_int_arg(args, "limit", 25);
            argv = QStringList{"insider_transactions", ticker, QString::number(limit)};
            return {};
        });
        tools.push_back(std::move(t));
    }

    // ── edgar_insider_summary ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_insider_summary";
        t.description = "Summarise open-market insider buying (code P) vs selling (code S) — counts, shares and "
                        "USD value — over the most recent Form 4 filings. Grants, option exercises, tax "
                        "withholding and gifts are reported separately, not as buys/sells.";
        t.category = "sec-edgar";
        t.input_schema.properties = QJsonObject{
            {"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}},
            {"limit", QJsonObject{{"type", "integer"}, {"description", "Transactions to summarize (default: 50)"}}}};
        t.input_schema.required = {"ticker"};
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            int limit = edgar_int_arg(args, "limit", 50);
            argv = QStringList{"insider_summary", ticker, QString::number(limit)};
            return {};
        });
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // 13F INSTITUTIONAL HOLDINGS
    // ════════════════════════════════════════════════════════════════════

    // ── edgar_13f_holdings ───────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_13f_holdings";
        t.description = "Holdings reported in an INSTITUTIONAL MANAGER's own 13F-HR filings over recent "
                        "quarters. The ticker/CIK must be a 13F filer (e.g. BRK-B, BLK) — this is NOT who owns "
                        "a stock; use edgar_institutional_holders for that.";
        t.category = "sec-edgar";
        t.input_schema.properties = QJsonObject{
            {"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}},
            {"quarters", QJsonObject{{"type", "integer"}, {"description", "Number of recent quarters (default: 2)"}}}};
        t.input_schema.required = {"ticker"};
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            int quarters = edgar_int_arg(args, "quarters", 2);
            argv = QStringList{"13f_holdings", ticker, QString::number(quarters)};
            return {};
        });
        tools.push_back(std::move(t));
    }

    // ── edgar_13f_top_holdings ───────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_13f_top_holdings";
        t.description = "Top positions (by value) in an INSTITUTIONAL MANAGER's latest 13F-HR portfolio. The "
                        "ticker/CIK must be a 13F filer (e.g. BRK-B, BLK) — for the top holders OF a stock use "
                        "edgar_institutional_holders.";
        t.category = "sec-edgar";
        t.input_schema.properties = QJsonObject{
            {"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}},
            {"top_n", QJsonObject{{"type", "integer"}, {"description", "Number of top holders (default: 20)"}}}};
        t.input_schema.required = {"ticker"};
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            int top_n = edgar_int_arg(args, "top_n", 20);
            argv = QStringList{"13f_top_holdings", ticker, QString::number(top_n)};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            argv = QStringList{"10k_exhibits", ticker};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            argv = QStringList{"10k_metadata", ticker};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            argv = QStringList{"10k_markdown", ticker};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            argv = QStringList{"10q_metadata", ticker};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            argv = QStringList{"10q_markdown", ticker};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            QString query = args["query"].toString().trimmed();
            if (ticker.isEmpty() || query.isEmpty())
                return QStringLiteral("Missing 'ticker' or 'query'");
            int max_results = edgar_int_arg(args, "max_results", 10);
            argv = QStringList{"10q_search", ticker, query, QString::number(max_results)};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            int limit = edgar_int_arg(args, "limit", 30);
            argv = QStringList{"8k_events_categorized", ticker, QString::number(limit)};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            QString query = args["query"].toString().trimmed();
            if (ticker.isEmpty() || query.isEmpty())
                return QStringLiteral("Missing 'ticker' or 'query'");
            int max_results = edgar_int_arg(args, "max_results", 10);
            argv = QStringList{"8k_search", ticker, query, QString::number(max_results)};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            int limit = edgar_int_arg(args, "limit", 25);
            argv = QStringList{"insider_transactions_detailed", ticker, QString::number(limit)};
            return {};
        });
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // 13F EXTRAS
    // ════════════════════════════════════════════════════════════════════

    // ── edgar_13f_manager_info ───────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_13f_manager_info";
        t.description = "Manager details from an institutional manager's latest 13F-HR (the ticker/CIK must "
                        "be a 13F filer).";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}}};
        t.input_schema.required = {"ticker"};
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            argv = QStringList{"13f_manager_info", ticker};
            return {};
        });
        tools.push_back(std::move(t));
    }

    // ── edgar_13f_summary ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_13f_summary";
        t.description = "Portfolio summary (total value, positions, top-10 concentration) of an institutional "
                        "manager's latest 13F-HR (the ticker/CIK must be a 13F filer).";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"ticker", QJsonObject{{"type", "string"}, {"description", "Ticker symbol"}}}};
        t.input_schema.required = {"ticker"};
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            argv = QStringList{"13f_summary", ticker};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            argv = QStringList{"get_financial_metrics", ticker};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            QString form = args["form"].toString().trimmed();
            if (form.isEmpty())
                form = "10-K";
            argv = QStringList{"get_xbrl_statements", ticker, form};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString cik = args["cik"].toString().trimmed();
            if (cik.isEmpty())
                return QStringLiteral("Missing 'cik'");
            argv = QStringList{"get_company_facts", cik};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString query = args["query"].toString().trimmed();
            if (query.isEmpty())
                return QStringLiteral("Missing 'query'");
            int top_n = edgar_int_arg(args, "top_n", 10);
            argv = QStringList{"find_company", query, QString::number(top_n)};
            return {};
        });
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
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            QString query = args["query"].toString().trimmed();
            if (query.isEmpty())
                return QStringLiteral("Missing 'query'");
            argv = QStringList{"resolve_ticker", query};
            return {};
        });
        tools.push_back(std::move(t));
    }

    // ════════════════════════════════════════════════════════════════════
    // RESEARCH VIEWS — compact, display-ready (same data as the Equity
    // Research FILINGS tab)
    // ════════════════════════════════════════════════════════════════════

    // ── edgar_filings_overview ───────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_filings_overview";
        t.description = "Issuer profile (CIK, SIC, fiscal year end) plus its most recent SEC filings with document "
                        "links (insider Forms 3/4/5/144 excluded) and recent 8-K events with plain-English item "
                        "labels.";
        t.category = "sec-edgar";
        t.input_schema.properties = QJsonObject{
            {"ticker", QJsonObject{{"type", "string"}, {"description", "US ticker (e.g. AAPL, BRK-B)"}}},
            {"limit", QJsonObject{{"type", "integer"}, {"description", "Filings to list (default: 20, max 200)"}}}};
        t.input_schema.required = {"ticker"};
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            const QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            argv = QStringList{"research_filings", ticker, QString::number(edgar_int_arg(args, "limit", 20))};
            return {};
        });
        tools.push_back(std::move(t));
    }

    // ── edgar_key_financials ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_key_financials";
        t.description = "Key metrics from the latest annual (10-K/20-F) and quarterly (10-Q) XBRL filings — "
                        "revenue, operating & net income, diluted EPS, operating cash flow, capex, FCF, assets, "
                        "liabilities, equity, cash, long-term debt — with the same period a year earlier and YoY. "
                        "10-Q cash-flow lines are year-to-date (see 'months').";
        t.category = "sec-edgar";
        t.input_schema.properties =
            QJsonObject{{"ticker", QJsonObject{{"type", "string"}, {"description", "US ticker (e.g. AAPL)"}}}};
        t.input_schema.required = {"ticker"};
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            const QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            argv = QStringList{"research_financials", ticker};
            return {};
        });
        tools.push_back(std::move(t));
    }

    // ── edgar_insider_activity ───────────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_insider_activity";
        t.description = "Open-market insider buys (P) vs sells (S) with counts, shares and USD value, Rule 10b5-1 "
                        "share of sales, and the individual non-derivative Form 4 transactions (newest first).";
        t.category = "sec-edgar";
        t.input_schema.properties = QJsonObject{
            {"ticker", QJsonObject{{"type", "string"}, {"description", "US ticker"}}},
            {"limit", QJsonObject{{"type", "integer"}, {"description", "Form 4 filings to parse (default: 25, max 60)"}}}};
        t.input_schema.required = {"ticker"};
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            const QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            argv = QStringList{"research_insiders", ticker,
                               QString::number(std::min(edgar_int_arg(args, "limit", 25), 60))};
            return {};
        });
        tools.push_back(std::move(t));
    }

    // ── edgar_institutional_holders ──────────────────────────────────────
    {
        ToolDef t;
        t.name = "edgar_institutional_holders";
        t.description = "Who owns a stock: top institutional holders from 13F filings (as aggregated by Yahoo "
                        "Finance — shares, % outstanding, value, quarter-over-quarter change) and the institutional / "
                        "insider ownership split; plus the company's own 13F-HR portfolio when it is a 13F filer.";
        t.category = "sec-edgar";
        t.input_schema.properties = QJsonObject{
            {"ticker", QJsonObject{{"type", "string"}, {"description", "US ticker"}}},
            {"top_n", QJsonObject{{"type", "integer"}, {"description", "Holders to return (default: 10, max 50)"}}}};
        t.input_schema.required = {"ticker"};
        set_edgar_handler(t, [](const QJsonObject& args, QStringList& argv) -> QString {
            const QString ticker = args["ticker"].toString().trimmed();
            if (ticker.isEmpty())
                return QStringLiteral("Missing 'ticker'");
            argv = QStringList{"research_holders", ticker,
                               QString::number(std::min(edgar_int_arg(args, "top_n", 10), 50))};
            return {};
        });
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
