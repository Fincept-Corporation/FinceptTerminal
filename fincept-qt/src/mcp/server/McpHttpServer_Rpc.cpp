// McpHttpServer_Rpc.cpp — JSON-RPC 2.0 / MCP dispatch for the external server.
//
// Stateless Streamable HTTP: no Mcp-Session-Id is issued (one sent by a client
// is ignored) and every POST is answered with application/json.
//   request               → 200 + response object
//   notifications / client responses only → 202, empty body
//   batch (JSON array)    → array of responses for the requests in it
//   malformed JSON        → 400 + error -32700
// Methods: initialize, ping, tools/list (cursor pages of 100), tools/call,
// resources/list, resources/templates/list, prompts/list, logging/setLevel.
// Everything is answered inline on the GUI thread except tools/call, which runs
// on the server's own pool (McpHttpServer_Tools.cpp) and is posted back here.

#include "core/logging/Logger.h"
#include "mcp/server/McpHttpServer_p.h"

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
    static const QStringList k = {QStringLiteral("2025-06-18"), QStringLiteral("2025-03-26"),
                                  QStringLiteral("2024-11-05")};
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

QString server_instructions(bool has_tool_invoke) {
    QStringList lines;
    lines << QStringLiteral("Fincept Terminal is the user's running financial terminal: live quotes, price history, "
                            "fundamentals, news, SEC filings, macro data, portfolios, paper trading, analytics and "
                            "workflows.");
    lines << QStringLiteral("- Discover: tool_list(query=\"what you need\") searches the whole catalogue (~900 tools); "
                            "tool_describe(name) returns one tool's input schema.");
    lines << (has_tool_invoke
                  ? QStringLiteral("- Call: use a listed tool directly, or call any tool found through tool_list with "
                                   "tool_invoke(tool, arguments).")
                  : QStringLiteral("- Call: use the listed tools directly."));
    lines << QStringLiteral("- Long-running tools answer {\"job_id\", \"status\":\"running\"} instead of a result: poll "
                            "job_status(job_id, wait_ms=20000) until it finishes, then job_result(job_id). Do not "
                            "start the tool again.");
    lines << QStringLiteral("- Results over the response budget come back truncated with a result_id; page the rest "
                            "with result_fetch(result_id, offset, limit).");
    lines << QStringLiteral("- State-changing tools (orders, files, settings) run only if the user allowed actions for "
                            "external agents; live-trading tools also need the user's approval in the terminal for "
                            "every call. If a call is refused, tell the user what you wanted to do instead of "
                            "retrying.");
    lines << QStringLiteral("- Data comes live from the user's terminal session: quote tool results, never guess "
                            "prices or figures.");
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
        if (method == QLatin1String("notifications/initialized"))
            LOG_DEBUG(kTag, QStringLiteral("Client reported initialized"));
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
        return rpc_result(id, QJsonObject{{QStringLiteral("resources"), QJsonArray{}}});
    if (method == QLatin1String("resources/templates/list"))
        return rpc_result(id, QJsonObject{{QStringLiteral("resourceTemplates"), QJsonArray{}}});
    if (method == QLatin1String("prompts/list"))
        return rpc_result(id, QJsonObject{{QStringLiteral("prompts"), QJsonArray{}}});
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
    if (!distinct_clients.contains(key) && distinct_clients.size() < kMaxTrackedClients) {
        distinct_clients.insert(key, client);
        // Queued: the UI reacting to this must not re-enter request handling.
        QMetaObject::invokeMethod(q, [server = q]() { emit server->status_changed(); }, Qt::QueuedConnection);
    }
    LOG_INFO(kTag, QStringLiteral("initialize from %1 %2 (protocol %3)").arg(client, client_version, negotiated));

    const CataloguePtr cat = current_catalogue(*shared);
    const QJsonObject result{
        {QStringLiteral("protocolVersion"), negotiated},
        {QStringLiteral("capabilities"),
         QJsonObject{{QStringLiteral("tools"), QJsonObject{{QStringLiteral("listChanged"), false}}}}},
        {QStringLiteral("serverInfo"), QJsonObject{{QStringLiteral("name"), QStringLiteral("fincept-terminal")},
                                                   {QStringLiteral("title"), QStringLiteral("Fincept Terminal")},
                                                   {QStringLiteral("version"), QCoreApplication::applicationVersion()}}},
        {QStringLiteral("instructions"), server_instructions(cat->has_tool_invoke)},
    };
    return rpc_result(id, result);
}

QJsonObject McpHttpServer::Impl::handle_tools_list(const QJsonValue& id, const QJsonObject& params) {
    using namespace mcp_http_detail;
    const CataloguePtr cat = current_catalogue(*shared);
    const QJsonArray& all = toolset == ExternalToolset::Full ? cat->full_entries : cat->essential_entries;
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
    // Never log arguments or result text: tool errors can echo them.
    const QString logged = outcome.success || outcome.denied ? outcome.summary
                                                             : (outcome.rpc_error ? QStringLiteral("rejected")
                                                                                  : QStringLiteral("failed"));
    LOG_INFO(kTag, QStringLiteral("tools/call %1 by %2 — %3 (%4 ms)")
                       .arg(job.called_name, job.client, logged)
                       .arg(duration_ms));

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

QString McpHttpServer::Impl::client_for(const mcp_http_detail::Conn& conn, const QByteArray& user_agent) const {
    if (!conn.client_name.isEmpty())
        return conn.client_name;
    if (!user_agent.isEmpty()) {
        const auto it = client_by_agent.constFind(user_agent);
        if (it != client_by_agent.constEnd())
            return it.value();
    }
    QSet<QString> names;
    for (const QString& name : distinct_clients)
        names.insert(name);
    if (names.size() == 1)
        return *names.cbegin();
    return QStringLiteral("unknown");
}

} // namespace fincept::mcp
