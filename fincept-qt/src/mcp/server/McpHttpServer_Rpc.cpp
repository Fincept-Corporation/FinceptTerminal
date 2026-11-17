// McpHttpServer_Rpc.cpp — what the terminal's MCP server SAYS besides tools:
// the `initialize` instructions, the workflow prompts and the two resources
// (the operating guide and the screen list). The JSON-RPC dispatch itself is
// McpServerEngine; these are the backend's answers for prompts/* and
// resources/* (see McpHttpServer_p.h).
//
// Token note: the instructions ride once per session (in the initialize
// result), not on every turn, so they stay a short operating manual.

#include "core/components/ComponentCatalog.h"
#include "core/logging/Logger.h"
#include "mcp/server/McpHttpServer_p.h"
#include "mcp/server/ToolApprovalDialog.h"

#include <QJsonArray>
#include <QMutexLocker>

namespace fincept::mcp {

namespace mcp_http_detail {

namespace {

struct PromptDef {
    const char* name;
    const char* title;
    const char* description;
    const char* arg; // nullptr = none
    const char* arg_description;
    bool arg_required;
    const char* body; // %1 = the argument
};

const QList<PromptDef>& prompt_defs() {
    static const QList<PromptDef> k = {
        {"research_company", "Research a company",
         "Open the company in Equity Research and build a sourced brief: price, fundamentals, news, peers.", "symbol",
         "Ticker, e.g. AAPL or RELIANCE.NS", true,
         "Research %1 using Fincept Terminal. ui_open(screen=\"equity_research\", symbol=\"%1\"), wait until it "
         "is idle, then read its tabs (overview, financials, news, peers, technicals) with ui_snapshot / ui_read. "
         "Add anything useful from the News screen. Write a concise brief with the figures exactly as the "
         "terminal shows them and say which screen each one came from."},
        {"market_briefing", "Market briefing", "Summarise what markets, news and the economic calendar show right now.",
         "focus", "Optional region or theme, e.g. India, rates, crypto", false,
         "Give me a market briefing from Fincept Terminal (focus: %1). Open the Markets screen and the News "
         "screen, wait for them to load, read the movers, indices and top headlines, and check Economics for "
         "upcoming data. Summarise in a few bullet points with numbers quoted from the terminal."},
        {"portfolio_review", "Portfolio review",
         "Review a portfolio's holdings, performance and risk as the Portfolio screen shows them.", "portfolio",
         "Optional portfolio name", false,
         "Review my portfolio in Fincept Terminal (%1). ui_open(screen=\"portfolio\"), select that portfolio if "
         "one is named, and read holdings, allocation, P&L and risk figures with ui_read. Point out "
         "concentration, the biggest movers and anything that needs attention. Do not place or change any "
         "orders."},
        {"run_analytics", "Run an analytics script",
         "Find the right bundled Python analytics script for a task and run it.", "goal",
         "What to compute, e.g. 'black-scholes price for a call' or 'FRED GDP series'", true,
         "Use Fincept Terminal's bundled analytics for this: %1. python(action=\"search\", query=…) to find "
         "candidate scripts, python(action=\"info\", script=…) to learn the command and arguments, then "
         "python(action=\"run\", …). If it returns a job_id, wait with python(action=\"status\"). Report the "
         "result and the script you used."},
        {"operate_terminal", "Do something in the terminal",
         "Carry out a task in Fincept Terminal by operating its screens.", "task",
         "What to do, e.g. 'add NVDA to my watchlist'", true,
         "In Fincept Terminal: %1. Start with ui_overview, open the screen that does this with ui_open, find the "
         "controls with ui_snapshot (use query=…), and work them with ui_act. Check the result before you say "
         "it is done. If the terminal asks you to approve something, tell me what it is."},
    };
    return k;
}

QString guide_markdown() {
    return QStringLiteral("# Operating Fincept Terminal over MCP\n\n") + server_instructions() +
           QStringLiteral("\n\n## Refs\n"
                          "w1 window · p3 panel (an open screen) · d1 dialog · m1 popup menu · e12 element · "
                          "e12:r5 row 5 of table e12. Refs stay valid while the element lives; a panel that is "
                          "closed and reopened gets new ones.\n\n"
                          "## Typical flow\n"
                          "1. ui_overview\n2. ui_open(screen=\"equity_research\", symbol=\"MSFT\")\n"
                          "3. ui_wait(until=\"idle\")\n4. ui_snapshot(target=\"p2\", query=\"revenue\")\n"
                          "5. ui_read(ref=\"e40\") for the full table\n"
                          "6. ui_act(steps=[{action:\"click\", name:\"Peers\", role:\"tab\"}], snapshot=true)\n");
}

QJsonObject prompts_list() {
    QJsonArray prompts;
    for (const PromptDef& d : prompt_defs()) {
        QJsonObject p{{QStringLiteral("name"), QString::fromLatin1(d.name)},
                      {QStringLiteral("title"), QString::fromUtf8(d.title)},
                      {QStringLiteral("description"), QString::fromUtf8(d.description)}};
        if (d.arg) {
            p.insert(QStringLiteral("arguments"),
                     QJsonArray{QJsonObject{{QStringLiteral("name"), QString::fromLatin1(d.arg)},
                                            {QStringLiteral("description"), QString::fromUtf8(d.arg_description)},
                                            {QStringLiteral("required"), d.arg_required}}});
        }
        prompts.append(p);
    }
    return QJsonObject{{QStringLiteral("prompts"), prompts}};
}

bool prompts_get(const QJsonObject& params, QJsonObject* result, int* error_code, QString* error_message) {
    const QString name = params.value(QStringLiteral("name")).toString();
    for (const PromptDef& d : prompt_defs()) {
        if (name != QLatin1String(d.name))
            continue;
        QString value;
        if (d.arg)
            value = params.value(QStringLiteral("arguments"))
                        .toObject()
                        .value(QString::fromLatin1(d.arg))
                        .toString()
                        .trimmed();
        if (d.arg && d.arg_required && value.isEmpty()) {
            *error_code = -32602;
            *error_message =
                QStringLiteral("Invalid params: prompt %1 needs argument \"%2\"").arg(name, QString::fromLatin1(d.arg));
            return true;
        }
        if (value.isEmpty())
            value = QStringLiteral("none given");
        const QString text = QString::fromUtf8(d.body).arg(value.left(200));
        *result = QJsonObject{{QStringLiteral("description"), QString::fromUtf8(d.description)},
                              {QStringLiteral("messages"),
                               QJsonArray{QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                                      {QStringLiteral("content"),
                                                       QJsonObject{{QStringLiteral("type"), QStringLiteral("text")},
                                                                   {QStringLiteral("text"), text}}}}}}};
        return true;
    }
    *error_code = -32602;
    *error_message = QStringLiteral("Invalid params: unknown prompt %1").arg(clip_text(name, 80));
    return true;
}

QJsonObject resources_list() {
    const QJsonArray resources{
        QJsonObject{{QStringLiteral("uri"), QStringLiteral("fincept://guide")},
                    {QStringLiteral("name"), QStringLiteral("guide")},
                    {QStringLiteral("title"), QStringLiteral("How to operate Fincept Terminal")},
                    {QStringLiteral("description"), QStringLiteral("The tools, refs and a typical flow.")},
                    {QStringLiteral("mimeType"), QStringLiteral("text/markdown")}},
        QJsonObject{{QStringLiteral("uri"), QStringLiteral("fincept://screens")},
                    {QStringLiteral("name"), QStringLiteral("screens")},
                    {QStringLiteral("title"), QStringLiteral("Fincept Terminal screens")},
                    {QStringLiteral("description"), QStringLiteral("Every screen ui_open can open, with a line each.")},
                    {QStringLiteral("mimeType"), QStringLiteral("text/plain")}},
    };
    return QJsonObject{{QStringLiteral("resources"), resources}};
}

} // namespace

QString server_instructions() {
    QStringList lines;
    lines << QStringLiteral("You are connected to the user's running Fincept Terminal (a financial terminal: markets, "
                            "research, news, filings, macro, portfolios, trading, analytics). You operate it the way "
                            "a person does.");
    lines << QStringLiteral("- Start with ui_overview: windows, open panels (refs like p3), dialogs, sign-in state.");
    lines << QStringLiteral("- ui_open(screen, symbol?) opens any screen (no screen = the list); it answers the "
                            "panel's controls. ui_snapshot(target) lists a panel's elements with refs (e12; table "
                            "rows e12:r5); pass query=\"words\" to find one thing in a busy panel.");
    lines << QStringLiteral("- ui_act works controls by ref or by name: click, type (submit=true presses Enter), "
                            "select, check, set_value, press, scroll … Batch related steps in `steps`; the answer "
                            "says what changed, so do not snapshot after every step.");
    lines << QStringLiteral("- Data loads asynchronously: after opening a screen or changing a symbol, "
                            "ui_wait(until=\"idle\") (or until=\"text\") before reading. ui_read(ref) returns a "
                            "table's rows, a chart's points, a dropdown's options or a long text in full, paged.");
    lines << QStringLiteral("- python(action=\"search\") finds the bundled analytics scripts (data connectors, "
                            "valuation, portfolio, derivatives, backtests); info shows how to call one; run starts "
                            "it. A run longer than wait_s returns a job_id: call python(action=\"status\", job_id) "
                            "until it finishes — never start it twice.");
    if (ToolApprovalDialog::auto_approve())
        lines << QStringLiteral("- AUTO MODE IS ON: orders, money movements, credit spends, deletes and sign-out run "
                                "WITHOUT asking the user. Act only on what the user asked for, check symbol, side, "
                                "quantity and price before any order, and never retry a failed order blindly. Secret "
                                "fields (passwords, API keys) are never read or typed — ask the user to type them.");
    else
        lines << QStringLiteral("- Orders, money movements, credit spends, deletes and sign-out ask the user in the "
                                "terminal first; if a step is declined, tell the user instead of retrying. Secret "
                                "fields (passwords, API keys) are never read or typed — ask the user to type them.");
    lines << QStringLiteral("- A call that times out or is cancelled may still finish on screen: check with "
                            "ui_overview before you retry anything that changes state.");
    lines << QStringLiteral("- Quote figures from what the terminal shows (ui_read / tool results); never guess prices "
                            "or numbers. Results over the size budget end with a res_… id: ui_read(ref) pages it.");
    return lines.join(QLatin1Char('\n'));
}

QString build_screens_text() {
    QStringList lines{QStringLiteral("Screens (id — title — category):")};
    for (const ComponentMeta& m : ComponentCatalog::instance().all()) {
        lines << QStringLiteral("%1 — %2 — %3%4")
                     .arg(m.id, m.title, m.category,
                          m.description.isEmpty() ? QString() : QStringLiteral(": ") + m.description.left(90));
    }
    lines << QStringLiteral("ui_open() lists every screen the running terminal can open, including any not above.");
    return lines.join(QLatin1Char('\n'));
}

bool serve_content(const BackendShared& shared, const QString& method, const QJsonObject& params, QJsonObject* result,
                   int* error_code, QString* error_message) {
    if (method == QLatin1String("prompts/list")) {
        *result = prompts_list();
        return true;
    }
    if (method == QLatin1String("prompts/get"))
        return prompts_get(params, result, error_code, error_message);
    if (method == QLatin1String("resources/list")) {
        *result = resources_list();
        return true;
    }
    if (method == QLatin1String("resources/templates/list")) {
        *result = QJsonObject{{QStringLiteral("resourceTemplates"), QJsonArray{}}};
        return true;
    }
    if (method == QLatin1String("resources/read")) {
        const QString uri = params.value(QStringLiteral("uri")).toString();
        QString text;
        QString mime;
        if (uri == QLatin1String("fincept://guide")) {
            text = guide_markdown();
            mime = QStringLiteral("text/markdown");
        } else if (uri == QLatin1String("fincept://screens")) {
            QMutexLocker lock(&shared.facts_mutex);
            text = shared.screens_text;
            mime = QStringLiteral("text/plain");
        } else {
            *error_code = -32002;
            *error_message = QStringLiteral("Resource not found: %1").arg(clip_text(uri, 120));
            return true;
        }
        *result = QJsonObject{{QStringLiteral("contents"), QJsonArray{QJsonObject{{QStringLiteral("uri"), uri},
                                                                                  {QStringLiteral("mimeType"), mime},
                                                                                  {QStringLiteral("text"), text}}}}};
        return true;
    }
    return false;
}

} // namespace mcp_http_detail

} // namespace fincept::mcp
