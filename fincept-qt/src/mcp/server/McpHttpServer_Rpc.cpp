// McpHttpServer_Rpc.cpp — JSON-RPC 2.0 / MCP dispatch for the external server.
//
// Stateless Streamable HTTP: no Mcp-Session-Id is issued (one sent by a client
// is ignored) and every POST is answered with application/json.
//   request               → 200 + response object
//   notifications / client responses only → 202, empty body
//   batch (JSON array)    → array of responses for the requests in it
//   malformed JSON        → 400 + error -32700
// Methods: initialize, ping, tools/list (cursor pages of 100), tools/call,
// prompts/list, prompts/get, resources/list, resources/read,
// resources/templates/list, logging/setLevel.
// Everything is answered inline on the GUI thread except tools/call, which runs
// on the server's own pool (McpHttpServer_Tools.cpp) and is posted back here.

#include "app/DockScreenRouter.h"
#include "core/components/ComponentCatalog.h"
#include "core/logging/Logger.h"
#include "mcp/server/McpHttpServer_p.h"
#include "mcp/server/ToolApprovalDialog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>
#include <QThreadPool>

#include <algorithm>
#include <exception>

namespace fincept::mcp {

namespace mcp_http_detail {

namespace {

const QStringList& supported_protocol_versions() {
    static const QStringList k = {QStringLiteral("2025-11-25"), QStringLiteral("2025-06-18"),
                                  QStringLiteral("2025-03-26"), QStringLiteral("2024-11-05")};
    return k;
}

QString sanitize_client_text(const QString& raw, int max_chars) {
    QString out;
    out.reserve(raw.size());
    for (const QChar c : raw) {
        if (c.isPrint())
            out.append(c);
    }
    return out.simplified().left(max_chars);
}

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
    lines << QStringLiteral("- If you give up on a call, send notifications/cancelled (or close the request): the "
                            "terminal stops waiting for it.");
    lines << QStringLiteral("- Quote figures from what the terminal shows (ui_read / tool results); never guess prices "
                            "or numbers. Results over the size budget end with a res_… id: ui_read(ref) pages it.");
    return lines.join(QLatin1Char('\n'));
}

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

QString screens_text() {
    QStringList lines{QStringLiteral("Screens (id — title — category):")};
    for (const ComponentMeta& m : ComponentCatalog::instance().all()) {
        lines << QStringLiteral("%1 — %2 — %3%4")
                     .arg(m.id, m.title, m.category,
                          m.description.isEmpty() ? QString() : QStringLiteral(": ") + m.description.left(90));
    }
    lines << QStringLiteral("ui_open() lists every screen the running terminal can open, including any not above.");
    return lines.join(QLatin1Char('\n'));
}

} // namespace

QJsonObject rpc_result(const QJsonValue& id, const QJsonValue& result) {
    return QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                       {QStringLiteral("id"), id},
                       {QStringLiteral("result"), result}};
}

QJsonObject rpc_error_body(int code, const QString& message) {
    return QJsonObject{{QStringLiteral("code"), code}, {QStringLiteral("message"), message}};
}

QJsonObject rpc_error(const QJsonValue& id, int code, const QString& message) {
    return QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                       {QStringLiteral("id"), id},
                       {QStringLiteral("error"), rpc_error_body(code, message)}};
}

} // namespace mcp_http_detail

// ── POST /mcp ────────────────────────────────────────────────────────────────

void McpHttpServer::Impl::handle_mcp_post(QTcpSocket* sock) {
    using namespace mcp_http_detail;
    auto it = conns.find(sock);
    if (it == conns.end())
        return;
    Conn& c = it.value();

    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(c.body, &perr);
    if (perr.error != QJsonParseError::NoError || (!doc.isObject() && !doc.isArray())) {
        const QString why = perr.error != QJsonParseError::NoError ? perr.errorString()
                                                                   : QStringLiteral("expected a JSON object or array");
        respond_json(sock, 400, rpc_error(QJsonValue(), -32700, QStringLiteral("Parse error: %1").arg(why)));
        return;
    }

    const QByteArray user_agent = c.headers.value("user-agent");
    PendingHttp ph;
    ph.sock = sock;
    ph.batch = doc.isArray();
    const quint64 http_id = next_http_id++;
    if (ph.batch) {
        const QJsonArray messages = doc.array();
        if (messages.isEmpty()) {
            respond_json(sock, 400, rpc_error(QJsonValue(), -32600, QStringLiteral("Invalid Request: empty batch")));
            return;
        }
        for (const QJsonValue& m : messages)
            handle_message(m, http_id, ph, c, user_agent);
    } else {
        handle_message(QJsonValue(doc.object()), http_id, ph, c, user_agent);
    }

    if (ph.responses.isEmpty()) {
        respond(sock, 202, QByteArray()); // notifications / client responses only
        return;
    }
    if (ph.outstanding == 0) {
        write_rpc_response(sock, ph);
        return;
    }
    pending.insert(http_id, ph); // completed by on_call_finished()
}

void McpHttpServer::Impl::handle_message(const QJsonValue& message, quint64 http_id, mcp_http_detail::PendingHttp& ph,
                                         mcp_http_detail::Conn& conn, const QByteArray& user_agent) {
    using namespace mcp_http_detail;
    if (!message.isObject()) {
        ph.responses.append(rpc_error(QJsonValue(), -32600, QStringLiteral("Invalid Request: expected an object")));
        return;
    }
    const QJsonObject m = message.toObject();
    const bool has_id = m.contains(QStringLiteral("id"));
    const QJsonValue id = has_id ? m.value(QStringLiteral("id")) : QJsonValue();
    const QJsonValue method_v = m.value(QStringLiteral("method"));

    if (m.value(QStringLiteral("jsonrpc")).toString() != QLatin1String("2.0")) {
        if (has_id || method_v.isUndefined())
            ph.responses.append(rpc_error(id, -32600, QStringLiteral("Invalid Request: \"jsonrpc\" must be \"2.0\"")));
        return;
    }
    if (method_v.isUndefined()) {
        // A response to a server→client request. This server never sends one,
        // so there is nothing to route it to: accept and ignore.
        if (m.contains(QStringLiteral("result")) || m.contains(QStringLiteral("error")))
            return;
        ph.responses.append(rpc_error(id, -32600, QStringLiteral("Invalid Request: missing \"method\"")));
        return;
    }
    if (!method_v.isString() || method_v.toString().isEmpty()) {
        if (has_id)
            ph.responses.append(rpc_error(id, -32600, QStringLiteral("Invalid Request: \"method\" must be a string")));
        return;
    }
    const QString method = method_v.toString();
    if (!has_id) { // notification — never answered
        if (method == QLatin1String("notifications/initialized")) {
            LOG_DEBUG(kTag, QStringLiteral("Client reported initialized"));
        } else if (method == QLatin1String("notifications/cancelled")) {
            const QJsonObject p = m.value(QStringLiteral("params")).toObject();
            cancel_request(user_agent, p.value(QStringLiteral("requestId")),
                           p.value(QStringLiteral("reason")).toString());
        }
        return;
    }
    if (!id.isString() && !id.isDouble()) {
        ph.responses.append(rpc_error(QJsonValue(), -32600, QStringLiteral("Invalid Request: \"id\" must be a string "
                                                                           "or a number")));
        return;
    }
    const QJsonValue params_v = m.value(QStringLiteral("params"));
    if (!params_v.isUndefined() && !params_v.isNull() && !params_v.isObject()) {
        ph.responses.append(rpc_error(id, -32602, QStringLiteral("Invalid params: \"params\" must be an object")));
        return;
    }
    const QJsonObject params = params_v.toObject();

    if (method != QLatin1String("tools/call")) {
        ph.responses.append(handle_method(method, id, params, conn, user_agent));
        return;
    }

    const QString client = client_for(conn, user_agent);
    const QJsonValue name_v = params.value(QStringLiteral("name"));
    const QJsonValue args_v = params.value(QStringLiteral("arguments"));
    QString invalid;
    if (!name_v.isString() || name_v.toString().trimmed().isEmpty())
        invalid = QStringLiteral("Invalid params: \"name\" (the tool name) is required");
    else if (!args_v.isUndefined() && !args_v.isNull() && !args_v.isObject())
        invalid = QStringLiteral("Invalid params: \"arguments\" must be an object");
    if (!invalid.isEmpty()) {
        ph.responses.append(rpc_error(id, -32602, invalid));
        ExternalCallRecord rec;
        rec.at = QDateTime::currentDateTime();
        rec.client = client;
        rec.tool = name_v.toString().trimmed();
        rec.summary = QStringLiteral("rejected — invalid params");
        record_activity(rec);
        return;
    }

    CallJob job;
    job.http_id = http_id;
    job.slot = static_cast<int>(ph.responses.size());
    job.epoch = epoch;
    job.rpc_id = id;
    job.called_name = name_v.toString().trimmed();
    job.args = args_v.toObject();
    job.client = client;
    job.cancel = std::make_shared<std::atomic<bool>>(false);
    job.cancel_key = QString::fromLatin1(user_agent) + QLatin1Char('|') +
                     (id.isString() ? id.toString() : QString::number(id.toDouble(), 'g', 17));
    // A client that reuses an id while the first call still runs: the newer call
    // owns the key (a cancel names the request it just sent).
    cancel_by_request.insert(job.cancel_key, job.cancel);
    ph.cancels.append(job.cancel);
    ph.responses.append(QJsonValue()); // filled by on_call_finished()
    ++ph.outstanding;
    start_call(job);
}

QJsonObject McpHttpServer::Impl::handle_method(const QString& method, const QJsonValue& id, const QJsonObject& params,
                                               mcp_http_detail::Conn& conn, const QByteArray& user_agent) {
    using namespace mcp_http_detail;
    if (method == QLatin1String("initialize"))
        return handle_initialize(id, params, conn, user_agent);
    if (method == QLatin1String("ping"))
        return rpc_result(id, QJsonObject{});
    if (method == QLatin1String("tools/list"))
        return handle_tools_list(id, params);
    if (method == QLatin1String("resources/list"))
        return handle_resources_list(id);
    if (method == QLatin1String("resources/read"))
        return handle_resources_read(id, params);
    if (method == QLatin1String("resources/templates/list"))
        return rpc_result(id, QJsonObject{{QStringLiteral("resourceTemplates"), QJsonArray{}}});
    if (method == QLatin1String("prompts/list"))
        return handle_prompts_list(id);
    if (method == QLatin1String("prompts/get"))
        return handle_prompts_get(id, params);
    if (method == QLatin1String("logging/setLevel"))
        return rpc_result(id, QJsonObject{});
    return rpc_error(id, -32601, QStringLiteral("Method not found: %1").arg(method));
}

QJsonObject McpHttpServer::Impl::handle_initialize(const QJsonValue& id, const QJsonObject& params,
                                                   mcp_http_detail::Conn& conn, const QByteArray& user_agent) {
    using namespace mcp_http_detail;
    const QString requested = params.value(QStringLiteral("protocolVersion")).toString();
    const QString negotiated =
        supported_protocol_versions().contains(requested) ? requested : QString::fromLatin1(kLatestProtocol);

    const QJsonObject info = params.value(QStringLiteral("clientInfo")).toObject();
    QString client = sanitize_client_text(info.value(QStringLiteral("name")).toString(), kClientNameMaxChars);
    if (client.isEmpty())
        client = QStringLiteral("unknown");
    const QString client_version = sanitize_client_text(info.value(QStringLiteral("version")).toString(), 32);

    conn.client_name = client;
    if (!user_agent.isEmpty()) {
        if (client_by_agent.size() >= kMaxTrackedClients && !client_by_agent.contains(user_agent))
            client_by_agent.clear();
        client_by_agent.insert(user_agent, client);
    }
    const QString key = client + QLatin1Char('|') + QString::fromLatin1(user_agent);
    const QDateTime now = QDateTime::currentDateTime();
    auto known = distinct_clients.find(key);
    if (known != distinct_clients.end()) {
        known->last_seen = now;
    } else if (distinct_clients.size() < kMaxTrackedClients) {
        ExternalClientInfo info;
        info.name = client;
        info.user_agent = sanitize_client_text(QString::fromLatin1(user_agent), 120);
        info.first_seen = now;
        info.last_seen = now;
        distinct_clients.insert(key, info);
        // Queued: the UI reacting to this must not re-enter request handling.
        QMetaObject::invokeMethod(q, [server = q]() { emit server->status_changed(); }, Qt::QueuedConnection);
    }
    LOG_INFO(kTag, QStringLiteral("initialize from %1 %2 (protocol %3)").arg(client, client_version, negotiated));

    const QJsonObject result{
        {QStringLiteral("protocolVersion"), negotiated},
        {QStringLiteral("capabilities"),
         QJsonObject{{QStringLiteral("tools"), QJsonObject{{QStringLiteral("listChanged"), false}}},
                     {QStringLiteral("prompts"), QJsonObject{{QStringLiteral("listChanged"), false}}},
                     {QStringLiteral("resources"),
                      QJsonObject{{QStringLiteral("listChanged"), false}, {QStringLiteral("subscribe"), false}}}}},
        {QStringLiteral("serverInfo"),
         QJsonObject{{QStringLiteral("name"), QStringLiteral("fincept-terminal")},
                     {QStringLiteral("title"), QStringLiteral("Fincept Terminal")},
                     {QStringLiteral("version"), QCoreApplication::applicationVersion()}}},
        {QStringLiteral("instructions"), server_instructions()},
    };
    return rpc_result(id, result);
}

QJsonObject McpHttpServer::Impl::handle_tools_list(const QJsonValue& id, const QJsonObject& params) {
    using namespace mcp_http_detail;
    const CataloguePtr cat = current_catalogue(*shared);
    const QJsonArray& all = cat->entries;
    const int total = static_cast<int>(all.size());

    int offset = 0;
    const QJsonValue cursor = params.value(QStringLiteral("cursor"));
    if (!cursor.isUndefined() && !cursor.isNull()) {
        const QString s = cursor.toString();
        bool ok = false;
        if (s.startsWith(QLatin1Char('p')))
            offset = s.mid(1).toInt(&ok);
        if (!ok || offset < 0 || offset > total)
            return rpc_error(id, -32602, QStringLiteral("Invalid params: unknown cursor"));
    }

    const int end = std::min(offset + kToolsPageSize, total);
    QJsonArray page;
    for (int i = offset; i < end; ++i)
        page.append(all.at(i));
    QJsonObject result{{QStringLiteral("tools"), page}};
    if (end < total)
        result.insert(QStringLiteral("nextCursor"), QStringLiteral("p%1").arg(end));
    return rpc_result(id, result);
}

QJsonObject McpHttpServer::Impl::handle_prompts_list(const QJsonValue& id) {
    using namespace mcp_http_detail;
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
    return rpc_result(id, QJsonObject{{QStringLiteral("prompts"), prompts}});
}

QJsonObject McpHttpServer::Impl::handle_prompts_get(const QJsonValue& id, const QJsonObject& params) {
    using namespace mcp_http_detail;
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
        if (d.arg && d.arg_required && value.isEmpty())
            return rpc_error(id, -32602,
                             QStringLiteral("Invalid params: prompt %1 needs argument \"%2\"")
                                 .arg(name, QString::fromLatin1(d.arg)));
        if (value.isEmpty())
            value = QStringLiteral("none given");
        const QString text = QString::fromUtf8(d.body).arg(value.left(200));
        return rpc_result(
            id, QJsonObject{{QStringLiteral("description"), QString::fromUtf8(d.description)},
                            {QStringLiteral("messages"),
                             QJsonArray{QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                                    {QStringLiteral("content"),
                                                     QJsonObject{{QStringLiteral("type"), QStringLiteral("text")},
                                                                 {QStringLiteral("text"), text}}}}}}});
    }
    return rpc_error(id, -32602, QStringLiteral("Invalid params: unknown prompt %1").arg(name));
}

QJsonObject McpHttpServer::Impl::handle_resources_list(const QJsonValue& id) {
    using namespace mcp_http_detail;
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
    return rpc_result(id, QJsonObject{{QStringLiteral("resources"), resources}});
}

QJsonObject McpHttpServer::Impl::handle_resources_read(const QJsonValue& id, const QJsonObject& params) {
    using namespace mcp_http_detail;
    const QString uri = params.value(QStringLiteral("uri")).toString();
    QString text;
    QString mime;
    if (uri == QLatin1String("fincept://guide")) {
        text = guide_markdown();
        mime = QStringLiteral("text/markdown");
    } else if (uri == QLatin1String("fincept://screens")) {
        text = screens_text();
        mime = QStringLiteral("text/plain");
    } else {
        return rpc_error(id, -32002, QStringLiteral("Resource not found: %1").arg(uri));
    }
    return rpc_result(
        id, QJsonObject{{QStringLiteral("contents"), QJsonArray{QJsonObject{{QStringLiteral("uri"), uri},
                                                                            {QStringLiteral("mimeType"), mime},
                                                                            {QStringLiteral("text"), text}}}}});
}

void McpHttpServer::Impl::note_client_call(const QString& client) {
    for (auto it = distinct_clients.begin(); it != distinct_clients.end(); ++it) {
        if (it->name == client) {
            ++it->calls;
            it->last_seen = QDateTime::currentDateTime();
            return;
        }
    }
}

void McpHttpServer::Impl::write_rpc_response(QTcpSocket* sock, const mcp_http_detail::PendingHttp& ph) {
    using namespace mcp_http_detail;
    if (ph.batch) {
        QJsonArray responses;
        for (const QJsonValue& r : ph.responses)
            responses.append(r);
        respond(sock, 200, compact_json(responses));
    } else {
        respond(sock, 200, compact_json(ph.responses.value(0).toObject()));
    }
}

// ── tools/call hand-off ──────────────────────────────────────────────────────

void McpHttpServer::Impl::start_call(const mcp_http_detail::CallJob& job) {
    using namespace mcp_http_detail;
    ensure_pool();
    std::shared_ptr<WorkerShared> state = shared; // outlives Impl if a call is still running at exit
    McpHttpServer* server_obj = q;
    pool->start([state, server_obj, job]() {
        if (!state->accepting.load() || !state->alive.load())
            return; // stopped before this call started — its connection is gone
        QElapsedTimer timer;
        timer.start();
        CallOutcome outcome;
        try {
            outcome = run_tool_call(*state, job);
        } catch (const std::exception& e) {
            outcome = CallOutcome{};
            outcome.payload = error_call_result(
                QStringLiteral("Internal error while running the tool: %1").arg(QString::fromUtf8(e.what())));
            outcome.summary = QStringLiteral("internal error");
        } catch (...) {
            outcome = CallOutcome{};
            outcome.payload = error_call_result(QStringLiteral("Internal error while running the tool."));
            outcome.summary = QStringLiteral("internal error");
        }
        const qint64 duration_ms = timer.elapsed();
        if (!state->alive.load())
            return;
        QMetaObject::invokeMethod(
            server_obj, [server_obj, job, outcome, duration_ms]() {
                server_obj->d_->on_call_finished(job, outcome, duration_ms);
            },
            Qt::QueuedConnection);
    });
}

void McpHttpServer::Impl::on_call_finished(const mcp_http_detail::CallJob& job,
                                           const mcp_http_detail::CallOutcome& outcome, qint64 duration_ms) {
    using namespace mcp_http_detail;
    ExternalCallRecord rec;
    rec.at = QDateTime::currentDateTime();
    rec.client = job.client;
    rec.tool = job.called_name;
    rec.success = outcome.success;
    rec.denied = outcome.denied;
    rec.duration_ms = duration_ms;
    rec.summary = outcome.summary;
    record_activity(rec);
    note_client_call(job.client);
    // Never log arguments or result text: tool errors can echo them.
    const QString logged = outcome.success || outcome.denied ? outcome.summary
                                                             : (outcome.rpc_error ? QStringLiteral("rejected")
                                                                                  : QStringLiteral("failed"));
    LOG_INFO(kTag, QStringLiteral("tools/call %1 by %2 — %3 (%4 ms)")
                       .arg(job.called_name, job.client, logged)
                       .arg(duration_ms));

    {
        const auto ck = cancel_by_request.find(job.cancel_key);
        if (ck != cancel_by_request.end() && ck.value() == job.cancel)
            cancel_by_request.erase(ck);
    }

    if (job.epoch != epoch)
        return; // the server restarted while the tool ran
    auto it = pending.find(job.http_id);
    if (it == pending.end())
        return;
    PendingHttp& ph = it.value();
    if (job.slot >= 0 && job.slot < ph.responses.size()) {
        ph.responses[job.slot] =
            outcome.rpc_error ? QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                                            {QStringLiteral("id"), job.rpc_id},
                                            {QStringLiteral("error"), outcome.payload}}
                              : rpc_result(job.rpc_id, outcome.payload);
    }
    if (--ph.outstanding > 0)
        return;
    const PendingHttp done = ph;
    pending.erase(it);
    QTcpSocket* sock = done.sock.data();
    if (!sock)
        return; // the client hung up while the tool ran
    write_rpc_response(sock, done);
    // Serve whatever the client pipelined behind this request.
    QPointer<QTcpSocket> guard(sock);
    QMetaObject::invokeMethod(
        q, [this, guard]() {
            if (guard)
                process(guard.data());
        },
        Qt::QueuedConnection);
}

void McpHttpServer::Impl::cancel_request(const QByteArray& user_agent, const QJsonValue& request_id,
                                         const QString& reason) {
    using namespace mcp_http_detail;
    if (!request_id.isString() && !request_id.isDouble())
        return;
    const QString key =
        QString::fromLatin1(user_agent) + QLatin1Char('|') +
        (request_id.isString() ? request_id.toString() : QString::number(request_id.toDouble(), 'g', 17));
    const auto it = cancel_by_request.constFind(key);
    if (it == cancel_by_request.constEnd()) {
        LOG_DEBUG(kTag, QStringLiteral("notifications/cancelled for a request that is not running"));
        return;
    }
    it.value()->store(true);
    LOG_INFO(kTag, QStringLiteral("Client cancelled a running tools/call%1")
                       .arg(reason.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(clip_text(reason, 80))));
}

void McpHttpServer::Impl::cancel_calls_on(QTcpSocket* sock) {
    for (auto it = pending.begin(); it != pending.end(); ++it) {
        if (it->sock.data() != sock)
            continue;
        for (const auto& flag : it->cancels)
            flag->store(true);
    }
}

void McpHttpServer::Impl::cancel_all_calls() {
    for (auto it = pending.begin(); it != pending.end(); ++it) {
        for (const auto& flag : it->cancels)
            flag->store(true);
    }
    for (auto it = cancel_by_request.cbegin(); it != cancel_by_request.cend(); ++it)
        it.value()->store(true);
    cancel_by_request.clear();
}

QString McpHttpServer::Impl::client_for(const mcp_http_detail::Conn& conn, const QByteArray& user_agent) const {
    if (!conn.client_name.isEmpty())
        return conn.client_name;
    if (!user_agent.isEmpty()) {
        const auto it = client_by_agent.constFind(user_agent);
        if (it != client_by_agent.constEnd())
            return it.value();
    }
    QSet<QString> names;
    for (const ExternalClientInfo& info : distinct_clients)
        names.insert(info.name);
    if (names.size() == 1)
        return *names.cbegin();
    return QStringLiteral("unknown");
}

} // namespace fincept::mcp
