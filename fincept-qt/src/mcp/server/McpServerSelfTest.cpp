// McpServerSelfTest.cpp — see McpServerSelfTest.h.

#include "mcp/server/McpServerSelfTest.h"

#include "mcp/McpInit.h"
#include "mcp/McpProvider.h"
#include "mcp/core/CoreTools.h"
#include "mcp/server/McpHttpServer.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <QTcpSocket>
#include <QTimer>

#include <algorithm>
#include <cstdio>
#include <functional>

namespace fincept::mcp {

namespace mcp_server_selftest_detail {

using HeaderList = QList<QPair<QByteArray, QByteArray>>;

void out(const QString& line) {
    std::fprintf(stdout, "%s\n", line.toUtf8().constData());
    std::fflush(stdout);
}

QString clip(const QString& s, int max_chars = 90) {
    const QString flat = s.simplified();
    return flat.size() <= max_chars ? flat : flat.left(max_chars) + QStringLiteral("…");
}

struct Report {
    int pass = 0;
    int fail = 0;
    void check(bool ok, const QString& what, const QString& detail = {}) {
        if (ok)
            ++pass;
        else
            ++fail;
        out(QStringLiteral("  [%1] %2%3")
                .arg(ok ? QStringLiteral("PASS") : QStringLiteral("FAIL"), what,
                     detail.isEmpty() ? QString() : QStringLiteral("  (") + detail + QLatin1Char(')')));
    }
};

/// Run the event loop until `done()` or the timeout. The self-test runs before
/// QApplication::exec(), so this is the only loop serving the server's sockets.
bool pump_until(const std::function<bool()>& done, int timeout_ms) {
    QElapsedTimer timer;
    timer.start();
    while (!done()) {
        if (timer.elapsed() > timeout_ms)
            return false;
        QEventLoop loop;
        QTimer::singleShot(5, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return true;
}

struct HttpReply {
    bool ok = false; // a complete response arrived
    int status = 0;
    QHash<QByteArray, QByteArray> headers; // lower-case names
    QByteArray body;
};

QByteArray to_json_bytes(const QJsonObject& o) {
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

bool connect_socket(QTcpSocket& sock, quint16 port) {
    sock.connectToHost(QHostAddress::LocalHost, port);
    return pump_until([&sock]() { return sock.state() == QAbstractSocket::ConnectedState; }, 5000);
}

/// Send `raw` on an open socket and read exactly one Content-Length-framed response.
HttpReply exchange(QTcpSocket& sock, const QByteArray& raw, int timeout_ms = 15000) {
    HttpReply r;
    QByteArray buf;
    qsizetype body_start = -1;
    qint64 need = -1;
    sock.write(raw);
    const bool done = pump_until(
        [&]() {
            buf += sock.readAll();
            if (body_start < 0) {
                const qsizetype end = buf.indexOf("\r\n\r\n");
                if (end < 0)
                    return false;
                body_start = end + 4;
                const QList<QByteArray> lines = buf.left(end).split('\n');
                r.status = lines.value(0).trimmed().split(' ').value(1).toInt();
                for (qsizetype i = 1; i < lines.size(); ++i) {
                    const QByteArray line = lines.at(i).trimmed();
                    const qsizetype colon = line.indexOf(':');
                    if (colon > 0)
                        r.headers.insert(line.left(colon).trimmed().toLower(), line.mid(colon + 1).trimmed());
                }
                need = r.headers.value("content-length", "-1").toLongLong();
            }
            if (need >= 0)
                return buf.size() - body_start >= need;
            return sock.state() == QAbstractSocket::UnconnectedState;
        },
        timeout_ms);
    if (body_start >= 0)
        r.body = need >= 0 ? buf.mid(body_start, need) : buf.mid(body_start);
    r.ok = done && body_start >= 0;
    return r;
}

QByteArray build_request(const QByteArray& method, const QByteArray& path, quint16 port, const HeaderList& headers,
                         const QByteArray& body, bool keep_alive = false) {
    QByteArray raw = method + ' ' + path + " HTTP/1.1\r\n";
    bool has_host = false;
    for (const auto& h : headers) {
        if (h.first.toLower() == "host")
            has_host = true;
        raw += h.first + ": " + h.second + "\r\n";
    }
    if (!has_host)
        raw += "Host: 127.0.0.1:" + QByteArray::number(port) + "\r\n";
    if (!body.isEmpty() || method == "POST")
        raw += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    raw += keep_alive ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
    raw += "\r\n";
    raw += body;
    return raw;
}

HttpReply http_request(quint16 port, const QByteArray& method, const QByteArray& path, const HeaderList& headers,
                       const QByteArray& body = {}) {
    QTcpSocket sock;
    if (!connect_socket(sock, port))
        return {};
    HttpReply r = exchange(sock, build_request(method, path, port, headers, body));
    sock.abort();
    return r;
}

HeaderList auth_headers(const QString& token) {
    return {{QByteArrayLiteral("Authorization"), QByteArrayLiteral("Bearer ") + token.toUtf8()},
            {QByteArrayLiteral("Content-Type"), QByteArrayLiteral("application/json")},
            {QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json, text/event-stream")},
            {QByteArrayLiteral("MCP-Protocol-Version"), QByteArrayLiteral("2025-06-18")}};
}

struct RpcReply {
    HttpReply http;
    QJsonDocument doc;
};

RpcReply rpc(quint16 port, const QString& token, const QJsonDocument& message) {
    RpcReply r;
    r.http = http_request(port, "POST", "/mcp", auth_headers(token), message.toJson(QJsonDocument::Compact));
    r.doc = QJsonDocument::fromJson(r.http.body);
    return r;
}

QJsonObject request(const QString& method, int id, const QJsonObject& params = {}) {
    QJsonObject m{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                  {QStringLiteral("id"), id},
                  {QStringLiteral("method"), method}};
    if (!params.isEmpty())
        m.insert(QStringLiteral("params"), params);
    return m;
}

QJsonObject call_tool(quint16 port, const QString& token, const QString& name, const QJsonObject& args, int id) {
    const QJsonObject params{{QStringLiteral("name"), name}, {QStringLiteral("arguments"), args}};
    return rpc(port, token, QJsonDocument(request(QStringLiteral("tools/call"), id, params))).doc.object();
}

QString first_text(const QJsonObject& call_result) {
    return call_result.value(QStringLiteral("content")).toArray().at(0).toObject().value(QStringLiteral("text")).toString();
}

int error_code(const QJsonDocument& doc) {
    return doc.object().value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toInt();
}

} // namespace mcp_server_selftest_detail

int run_mcp_server_selftest() {
    using namespace mcp_server_selftest_detail;
    QElapsedTimer total;
    total.start();
    out(QStringLiteral("\n══════════════════════════════════════════════════════════════"));
    out(QStringLiteral("  MCP SERVER SELF-TEST (external agents · Streamable HTTP)"));
    out(QStringLiteral("══════════════════════════════════════════════════════════════"));
    Report rep;
    static const QRegularExpression name_rx(QStringLiteral("^[a-zA-Z0-9_-]{1,50}$"));

    // main() defers initialize_all_tools() to the first event-loop turn; give it that turn.
    auto& provider = McpProvider::instance();
    if (provider.tool_count() == 0)
        pump_until([&provider]() { return provider.tool_count() > 0; }, 8000);
    if (provider.tool_count() == 0)
        initialize_all_tools();
    rep.check(provider.tool_count() == static_cast<std::size_t>(core::core_tool_names().size()),
              QStringLiteral("the ten core tools are registered"),
              QStringLiteral("%1 tools").arg(provider.tool_count()));

    auto& server = McpHttpServer::instance();
    server.stop(); // in case "start with the terminal" already started it on the real port
    const QString token = QString::number(QRandomGenerator::system()->generate64(), 16) +
                          QString::number(QRandomGenerator::system()->generate64(), 16);
    server.set_selftest_overrides(0, token, /*allow_ui_control=*/true, /*allow_scripts=*/true);
    const bool started = server.start();
    const quint16 port = server.port();
    rep.check(started && server.is_running() && port != 0, QStringLiteral("listens on an OS-assigned loopback port"),
              server.url());
    if (!started || port == 0) {
        server.clear_selftest_overrides();
        out(QStringLiteral("  ABORT: %1").arg(server.last_error()));
        return 1;
    }
    rep.check(server.url() == QStringLiteral("http://127.0.0.1:%1/mcp").arg(port),
              QStringLiteral("url() reports the bound port"));

    // ── /health ──
    {
        const HttpReply r = http_request(port, "GET", "/health", {});
        const QJsonObject o = QJsonDocument::fromJson(r.body).object();
        rep.check(r.ok && r.status == 200 && o.value(QStringLiteral("ok")).toBool() &&
                      o.value(QStringLiteral("name")).toString() == QLatin1String("fincept-terminal"),
                  QStringLiteral("GET /health answers without a token"), QString::number(r.status));
    }

    // ── initialize / notifications ──
    {
        const QJsonObject client{{QStringLiteral("name"), QStringLiteral("fincept-selftest")},
                                 {QStringLiteral("version"), QStringLiteral("1.0")}};
        const RpcReply r = rpc(port, token,
                               QJsonDocument(request(QStringLiteral("initialize"), 1,
                                                     {{QStringLiteral("protocolVersion"), QStringLiteral("2025-06-18")},
                                                      {QStringLiteral("capabilities"), QJsonObject{}},
                                                      {QStringLiteral("clientInfo"), client}})));
        const QJsonObject res = r.doc.object().value(QStringLiteral("result")).toObject();
        rep.check(r.http.status == 200 &&
                      res.value(QStringLiteral("protocolVersion")).toString() == QLatin1String("2025-06-18") &&
                      res.value(QStringLiteral("serverInfo")).toObject().value(QStringLiteral("name")).toString() ==
                          QLatin1String("fincept-terminal") &&
                      res.value(QStringLiteral("capabilities")).toObject().contains(QStringLiteral("tools")) &&
                      res.value(QStringLiteral("capabilities")).toObject().contains(QStringLiteral("prompts")) &&
                      res.value(QStringLiteral("capabilities")).toObject().contains(QStringLiteral("resources")) &&
                      !res.value(QStringLiteral("instructions")).toString().isEmpty(),
                  QStringLiteral("initialize negotiates 2025-06-18 and describes the server"));
        const RpcReply older = rpc(port, token,
                                   QJsonDocument(request(QStringLiteral("initialize"), 2,
                                                         {{QStringLiteral("protocolVersion"),
                                                           QStringLiteral("2024-11-05")},
                                                          {QStringLiteral("clientInfo"), client}})));
        rep.check(older.doc.object().value(QStringLiteral("result")).toObject().value(QStringLiteral("protocolVersion"))
                          .toString() == QLatin1String("2024-11-05"),
                  QStringLiteral("initialize echoes an older supported protocol version"));
        const RpcReply future = rpc(
            port, token,
            QJsonDocument(request(QStringLiteral("initialize"), 3,
                                  {{QStringLiteral("protocolVersion"), QStringLiteral("2099-01-01")}})));
        rep.check(future.doc.object()
                          .value(QStringLiteral("result"))
                          .toObject()
                          .value(QStringLiteral("protocolVersion"))
                          .toString() == QLatin1String("2025-11-25"),
                  QStringLiteral("initialize answers 2025-11-25 (the latest) to an unknown version"));
        rep.check(server.connected_client_count() >= 1, QStringLiteral("connected_client_count() counts the client"),
                  QString::number(server.connected_client_count()));
        const RpcReply note = rpc(port, token,
                                  QJsonDocument(QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                                                            {QStringLiteral("method"),
                                                             QStringLiteral("notifications/initialized")}}));
        rep.check(note.http.status == 202 && note.http.body.isEmpty(),
                  QStringLiteral("notification → 202 with an empty body"), QString::number(note.http.status));
    }

    // ── tools/list ──
    {
        const RpcReply r = rpc(port, token, QJsonDocument(request(QStringLiteral("tools/list"), 4)));
        const QJsonObject res = r.doc.object().value(QStringLiteral("result")).toObject();
        const QJsonArray tools = res.value(QStringLiteral("tools")).toArray();
        QStringList listed;
        bool names_ok = !tools.isEmpty();
        bool schemas_ok = true;
        bool read_hints_ok = true;
        for (const QJsonValue& v : tools) {
            const QJsonObject t = v.toObject();
            const QString name = t.value(QStringLiteral("name")).toString();
            listed << name;
            names_ok = names_ok && name_rx.match(name).hasMatch();
            const QJsonObject schema = t.value(QStringLiteral("inputSchema")).toObject();
            schemas_ok = schemas_ok && schema.value(QStringLiteral("type")).toString() == QLatin1String("object") &&
                         schema.value(QStringLiteral("properties")).isObject();
            const QJsonObject ann = t.value(QStringLiteral("annotations")).toObject();
            const bool read_only = ann.value(QStringLiteral("readOnlyHint")).toBool();
            const bool should_be = name == QLatin1String("ui_overview") || name == QLatin1String("ui_snapshot") ||
                                   name == QLatin1String("ui_read") || name == QLatin1String("ui_wait") ||
                                   name == QLatin1String("ui_screenshot");
            // Observing tools are never destructive; every tool that acts may be.
            read_hints_ok = read_hints_ok && read_only == should_be &&
                            ann.value(QStringLiteral("destructiveHint")).toBool() == !should_be;
        }
        rep.check(r.http.status == 200 && listed == core::core_tool_names(),
                  QStringLiteral("tools/list returns exactly the ten core tools, in order"),
                  listed.join(QLatin1Char(',')));
        rep.check(!res.contains(QStringLiteral("nextCursor")), QStringLiteral("the whole catalogue fits one page"));
        rep.check(names_ok, QStringLiteral("every advertised name matches ^[a-zA-Z0-9_-]{1,50}$"));
        rep.check(schemas_ok, QStringLiteral("every inputSchema is an object schema with properties"));
        rep.check(read_hints_ok,
                  QStringLiteral("readOnlyHint marks exactly the five observing tools; the rest are destructiveHint"));
    }

    // ── prompts & resources ──
    {
        const RpcReply list = rpc(port, token, QJsonDocument(request(QStringLiteral("prompts/list"), 50)));
        const QJsonArray prompts =
            list.doc.object().value(QStringLiteral("result")).toObject().value(QStringLiteral("prompts")).toArray();
        rep.check(prompts.size() >= 5, QStringLiteral("prompts/list offers the workflow prompts"),
                  QString::number(prompts.size()));
        const RpcReply got =
            rpc(port, token,
                QJsonDocument(request(
                    QStringLiteral("prompts/get"), 51,
                    {{QStringLiteral("name"), QStringLiteral("research_company")},
                     {QStringLiteral("arguments"), QJsonObject{{QStringLiteral("symbol"), QStringLiteral("MSFT")}}}})));
        const QJsonArray msgs =
            got.doc.object().value(QStringLiteral("result")).toObject().value(QStringLiteral("messages")).toArray();
        rep.check(msgs.size() == 1 && msgs.at(0)
                                          .toObject()
                                          .value(QStringLiteral("content"))
                                          .toObject()
                                          .value(QStringLiteral("text"))
                                          .toString()
                                          .contains(QStringLiteral("MSFT")),
                  QStringLiteral("prompts/get fills the argument into the prompt"));
        const RpcReply missing =
            rpc(port, token,
                QJsonDocument(request(QStringLiteral("prompts/get"), 52,
                                      {{QStringLiteral("name"), QStringLiteral("research_company")}})));
        rep.check(error_code(missing.doc) == -32602,
                  QStringLiteral("prompts/get without a required argument → -32602"));
        const RpcReply rl = rpc(port, token, QJsonDocument(request(QStringLiteral("resources/list"), 53)));
        rep.check(rl.doc.object()
                          .value(QStringLiteral("result"))
                          .toObject()
                          .value(QStringLiteral("resources"))
                          .toArray()
                          .size() == 2,
                  QStringLiteral("resources/list offers the guide and the screen list"));
        const RpcReply guide =
            rpc(port, token,
                QJsonDocument(request(QStringLiteral("resources/read"), 54,
                                      {{QStringLiteral("uri"), QStringLiteral("fincept://guide")}})));
        rep.check(guide.doc.object()
                      .value(QStringLiteral("result"))
                      .toObject()
                      .value(QStringLiteral("contents"))
                      .toArray()
                      .at(0)
                      .toObject()
                      .value(QStringLiteral("text"))
                      .toString()
                      .contains(QStringLiteral("ui_overview")),
                  QStringLiteral("resources/read fincept://guide returns the guide"));
        const RpcReply nores = rpc(port, token,
                                   QJsonDocument(request(QStringLiteral("resources/read"), 55,
                                                         {{QStringLiteral("uri"), QStringLiteral("fincept://nope")}})));
        rep.check(error_code(nores.doc) == -32002, QStringLiteral("unknown resource → -32002"));
    }

    // ── tools/call ──
    {
        // Headless: no terminal window exists, and the tools must say so rather
        // than hang or crash.
        const QJsonObject ov =
            call_tool(port, token, QStringLiteral("ui_overview"), {}, 5).value(QStringLiteral("result")).toObject();
        rep.check(!ov.value(QStringLiteral("isError")).toBool(true) &&
                      first_text(ov).contains(QStringLiteral("Fincept Terminal")),
                  QStringLiteral("ui_overview answers with no window open"), clip(first_text(ov)));
        QElapsedTimer t;
        t.start();
        const QJsonObject snap =
            call_tool(port, token, QStringLiteral("ui_snapshot"), {}, 6).value(QStringLiteral("result")).toObject();
        rep.check(snap.value(QStringLiteral("isError")).toBool() && t.elapsed() < 5000,
                  QStringLiteral("ui_snapshot fails fast and cleanly with no window"), clip(first_text(snap)));
        const QJsonObject py = call_tool(port, token, QStringLiteral("python"),
                                         {{QStringLiteral("action"), QStringLiteral("search")},
                                          {QStringLiteral("query"), QStringLiteral("fred economic data")}},
                                         7)
                                   .value(QStringLiteral("result"))
                                   .toObject();
        rep.check(py.contains(QStringLiteral("content")), QStringLiteral("python search answers"),
                  clip(first_text(py)));
    }
    {
        // Look-only mode refuses the UI-changing tools, and the refusal is logged.
        server.set_allow_ui_control(false);
        const QJsonObject res = call_tool(port, token, QStringLiteral("ui_open"),
                                          {{QStringLiteral("screen"), QStringLiteral("markets")}}, 8)
                                    .value(QStringLiteral("result"))
                                    .toObject();
        rep.check(res.value(QStringLiteral("isError")).toBool() &&
                      first_text(res).contains(QStringLiteral("look-only")),
                  QStringLiteral("look-only mode refuses ui_open"), clip(first_text(res)));
        const QJsonObject act = call_tool(port, token, QStringLiteral("ui_act"),
                                          {{QStringLiteral("action"), QStringLiteral("click")},
                                           {QStringLiteral("name"), QStringLiteral("Refresh")}},
                                          9)
                                    .value(QStringLiteral("result"))
                                    .toObject();
        rep.check(act.value(QStringLiteral("isError")).toBool() &&
                      first_text(act).contains(QStringLiteral("look-only")),
                  QStringLiteral("look-only mode refuses ui_act"));
        const auto activity = server.recent_activity();
        const bool logged = std::any_of(activity.cbegin(), activity.cend(), [](const ExternalCallRecord& r) {
            return r.denied && r.tool == QLatin1String("ui_open");
        });
        rep.check(logged, QStringLiteral("the refusal is recorded in recent_activity() as denied"));
        server.set_allow_ui_control(true);

        server.set_allow_scripts(false);
        const QJsonObject run = call_tool(port, token, QStringLiteral("python"),
                                          {{QStringLiteral("action"), QStringLiteral("run")},
                                           {QStringLiteral("script"), QStringLiteral("fred_data")}},
                                          10)
                                    .value(QStringLiteral("result"))
                                    .toObject();
        rep.check(run.value(QStringLiteral("isError")).toBool() &&
                      first_text(run).contains(QStringLiteral("not allowed external agents to run")),
                  QStringLiteral("scripts off refuses python run"), clip(first_text(run)));
        const QJsonObject info =
            call_tool(port, token, QStringLiteral("python"), {{QStringLiteral("action"), QStringLiteral("search")}}, 11)
                .value(QStringLiteral("result"))
                .toObject();
        rep.check(!first_text(info).contains(QStringLiteral("not allowed external agents")),
                  QStringLiteral("scripts off still allows python search"));
        server.set_allow_scripts(true);
    }
    {
        const QJsonObject resp = call_tool(port, token, QStringLiteral("no_such_tool_selftest"), {}, 12);
        rep.check(resp.value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toInt() == -32602,
                  QStringLiteral("unknown tool → JSON-RPC -32602"));
        const RpcReply bad_args = rpc(port, token,
                                      QJsonDocument(request(QStringLiteral("tools/call"), 13,
                                                            {{QStringLiteral("name"), QStringLiteral("ui_overview")},
                                                             {QStringLiteral("arguments"), QStringLiteral("x")}})));
        rep.check(error_code(bad_args.doc) == -32602, QStringLiteral("non-object arguments → -32602"));
    }
    {
        // notifications/cancelled ends a running call: a 20 s pause answers
        // within a few seconds once the client cancels it (from another POST —
        // the server is stateless, so the request id + User-Agent find it).
        QTcpSocket sock;
        QElapsedTimer t;
        t.start();
        bool answered_fast = false;
        bool says_cancelled = false;
        if (connect_socket(sock, port)) {
            const QJsonObject params{
                {QStringLiteral("name"), QStringLiteral("ui_wait")},
                {QStringLiteral("arguments"), QJsonObject{{QStringLiteral("until"), QStringLiteral("time")},
                                                          {QStringLiteral("timeout_ms"), 20000}}}};
            sock.write(build_request("POST", "/mcp", port, auth_headers(token),
                                     to_json_bytes(request(QStringLiteral("tools/call"), 31, params))));
            pump_until([]() { return false; }, 400); // let the call start
            const RpcReply cancel =
                rpc(port, token,
                    QJsonDocument(QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                                              {QStringLiteral("method"), QStringLiteral("notifications/cancelled")},
                                              {QStringLiteral("params"),
                                               QJsonObject{{QStringLiteral("requestId"), 31},
                                                           {QStringLiteral("reason"), QStringLiteral("selftest")}}}}));
            const HttpReply r = exchange(sock, QByteArray(), 10000);
            answered_fast = cancel.http.status == 202 && r.ok && t.elapsed() < 8000;
            const QJsonObject res = QJsonDocument::fromJson(r.body).object().value(QStringLiteral("result")).toObject();
            says_cancelled = res.value(QStringLiteral("isError")).toBool() &&
                             first_text(res).contains(QStringLiteral("Cancelled"), Qt::CaseInsensitive);
            sock.abort();
        }
        rep.check(answered_fast && says_cancelled, QStringLiteral("notifications/cancelled stops a running tools/call"),
                  QStringLiteral("%1 ms").arg(t.elapsed()));
    }

    // ── auth & origin ──
    {
        const QByteArray ping = to_json_bytes(request(QStringLiteral("ping"), 11));
        const HttpReply none = http_request(port, "POST", "/mcp", {{"Content-Type", "application/json"}}, ping);
        rep.check(none.status == 401 && none.headers.value("www-authenticate").startsWith("Bearer"),
                  QStringLiteral("missing token → 401 + WWW-Authenticate: Bearer"), QString::number(none.status));
        const HttpReply wrong = http_request(port, "POST", "/mcp", auth_headers(QStringLiteral("not-the-token")), ping);
        rep.check(wrong.status == 401, QStringLiteral("wrong token → 401"), QString::number(wrong.status));
        HeaderList evil_host = auth_headers(token);
        evil_host.append(qMakePair(QByteArrayLiteral("Host"),
                                   QByteArrayLiteral("evil.example.com:") + QByteArray::number(port)));
        const HttpReply host = http_request(port, "POST", "/mcp", evil_host, ping);
        rep.check(host.status == 403, QStringLiteral("non-loopback Host header → 403"), QString::number(host.status));
        HeaderList evil_origin = auth_headers(token);
        evil_origin.append(qMakePair(QByteArrayLiteral("Origin"), QByteArrayLiteral("http://evil.example.com")));
        const HttpReply origin = http_request(port, "POST", "/mcp", evil_origin, ping);
        rep.check(origin.status == 403, QStringLiteral("cross-site Origin → 403"), QString::number(origin.status));
    }

    // ── batch ──
    {
        const QJsonArray batch{request(QStringLiteral("ping"), 21),
                               QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                                           {QStringLiteral("method"), QStringLiteral("notifications/initialized")}},
                               request(QStringLiteral("tools/list"), 22)};
        const RpcReply r = rpc(port, token, QJsonDocument(batch));
        const QJsonArray arr = r.doc.array();
        rep.check(r.http.status == 200 && arr.size() == 2 &&
                      arr.at(0).toObject().value(QStringLiteral("id")).toInt() == 21 &&
                      arr.at(1).toObject().value(QStringLiteral("id")).toInt() == 22,
                  QStringLiteral("batch → one response per request, in order"));
        const QJsonArray mixed{request(QStringLiteral("ping"), 23),
                               request(QStringLiteral("tools/call"), 24,
                                       {{QStringLiteral("name"), QStringLiteral("ui_overview")},
                                        {QStringLiteral("arguments"), QJsonObject{}}})};
        const RpcReply m = rpc(port, token, QJsonDocument(mixed));
        rep.check(m.http.status == 200 && m.doc.array().size() == 2 &&
                      m.doc.array().at(1).toObject().contains(QStringLiteral("result")),
                  QStringLiteral("batch containing tools/call completes after the worker finishes"));
    }

    // ── protocol errors ──
    {
        const HttpReply garbage = http_request(port, "POST", "/mcp", auth_headers(token), "{not json");
        rep.check(garbage.status == 400 && error_code(QJsonDocument::fromJson(garbage.body)) == -32700,
                  QStringLiteral("malformed JSON → 400 + -32700"), QString::number(garbage.status));
        const RpcReply unknown = rpc(port, token, QJsonDocument(request(QStringLiteral("no/such/method"), 31)));
        rep.check(error_code(unknown.doc) == -32601, QStringLiteral("unknown method → -32601"));
        const HttpReply get = http_request(port, "GET", "/mcp", auth_headers(token));
        rep.check(get.status == 405 && get.headers.value("allow") == "POST", QStringLiteral("GET /mcp → 405 + Allow: POST"),
                  QString::number(get.status));
        const HttpReply missing = http_request(port, "GET", "/nope", {});
        rep.check(missing.status == 404, QStringLiteral("unknown path → 404"), QString::number(missing.status));
    }

    // ── chunked body + keep-alive on one connection ──
    {
        QTcpSocket sock;
        if (connect_socket(sock, port)) {
            const QByteArray body = to_json_bytes(request(QStringLiteral("tools/list"), 41));
            const qsizetype half = body.size() / 2;
            QByteArray raw = "POST /mcp HTTP/1.1\r\nHost: 127.0.0.1:" + QByteArray::number(port) +
                             "\r\nAuthorization: Bearer " + token.toUtf8() +
                             "\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\n\r\n";
            raw += QByteArray::number(half, 16) + "\r\n" + body.left(half) + "\r\n";
            raw += QByteArray::number(body.size() - half, 16) + "\r\n" + body.mid(half) + "\r\n0\r\n\r\n";
            const HttpReply first = exchange(sock, raw);
            rep.check(first.status == 200 &&
                          QJsonDocument::fromJson(first.body).object().value(QStringLiteral("id")).toInt() == 41,
                      QStringLiteral("chunked request body is decoded"), QString::number(first.status));
            const HttpReply second =
                exchange(sock, build_request("POST", "/mcp", port, auth_headers(token),
                                             to_json_bytes(request(QStringLiteral("ping"), 42)), /*keep_alive=*/true));
            rep.check(second.status == 200 &&
                          QJsonDocument::fromJson(second.body).object().value(QStringLiteral("id")).toInt() == 42,
                      QStringLiteral("keep-alive: a second request on the same connection"),
                      QString::number(second.status));
            sock.abort();
        } else {
            rep.check(false, QStringLiteral("connect for the chunked / keep-alive test"));
        }
    }

    // ── advertised-name aliasing ──
    {
        const QString long_name = QStringLiteral("x").repeated(70);
        const QString alias = McpHttpServer::advertised_name(long_name);
        rep.check(name_rx.match(alias).hasMatch() && alias == McpHttpServer::advertised_name(long_name) &&
                      alias != McpHttpServer::advertised_name(long_name + QStringLiteral("y")),
                  QStringLiteral("over-long names get a stable, distinct ≤50-char alias"), alias);
        const QString dotted = McpHttpServer::advertised_name(QStringLiteral("report.add.section"));
        rep.check(name_rx.match(dotted).hasMatch() && dotted != QLatin1String("report_add_section"),
                  QStringLiteral("names with invalid characters get a hashed alias"), dotted);
        rep.check(McpHttpServer::advertised_name(QStringLiteral("ui_act")) == QLatin1String("ui_act"),
                  QStringLiteral("valid names are advertised unchanged"));
    }

    // ── snippets ──
    {
        const QString url = server.url();
        rep.check(server.claude_code_add_command().contains(url) &&
                      server.claude_code_add_command().contains(QStringLiteral("Authorization: Bearer ") + token),
                  QStringLiteral("claude_code_add_command() carries the URL and token"));
        const QJsonObject generic = QJsonDocument::fromJson(server.generic_http_json().toUtf8())
                                        .object()
                                        .value(QStringLiteral("mcpServers"))
                                        .toObject()
                                        .value(QStringLiteral("fincept_terminal"))
                                        .toObject();
        rep.check(generic.value(QStringLiteral("url")).toString() == url &&
                      generic.value(QStringLiteral("headers")).toObject().value(QStringLiteral("Authorization")).toString() ==
                          QStringLiteral("Bearer ") + token,
                  QStringLiteral("generic_http_json() is valid JSON with the URL and header"));
        const QJsonObject env_cfg = server.claude_mcp_config_env()
                                        .value(QStringLiteral("mcpServers"))
                                        .toObject()
                                        .value(QStringLiteral("fincept"))
                                        .toObject();
        rep.check(env_cfg.value(QStringLiteral("headers")).toObject().value(QStringLiteral("Authorization")).toString() ==
                      QLatin1String("Bearer ${FINCEPT_MCP_TOKEN}"),
                  QStringLiteral("claude_mcp_config_env() uses ${FINCEPT_MCP_TOKEN} expansion"));
        rep.check(server.codex_config_toml().contains(QStringLiteral("url = \"") + url + QLatin1Char('"')) &&
                      server.codex_config_toml().contains(QStringLiteral("tool_timeout_sec")) &&
                      server.codex_config_overrides().size() == 6,
                  QStringLiteral("codex_config_toml() / codex_config_overrides()"));
        const QJsonObject gemini = QJsonDocument::fromJson(server.gemini_settings_json().toUtf8())
                                       .object()
                                       .value(QStringLiteral("mcpServers"))
                                       .toObject()
                                       .value(QStringLiteral("fincept_terminal"))
                                       .toObject();
        rep.check(gemini.value(QStringLiteral("httpUrl")).toString() == url &&
                      server.gemini_add_command().contains(url),
                  QStringLiteral("gemini_settings_json() / gemini_add_command() carry the URL"));
        const QJsonObject vscode = QJsonDocument::fromJson(server.vscode_json().toUtf8())
                                       .object()
                                       .value(QStringLiteral("servers"))
                                       .toObject()
                                       .value(QStringLiteral("fincept_terminal"))
                                       .toObject();
        rep.check(vscode.value(QStringLiteral("url")).toString() == url &&
                      vscode.value(QStringLiteral("type")).toString() == QLatin1String("http"),
                  QStringLiteral("vscode_json() uses the servers/type=http shape"));
    }

    server.stop();
    rep.check(!server.is_running() && server.port() == 0 && server.url().isEmpty(),
              QStringLiteral("stop() releases the port"));
    server.clear_selftest_overrides();

    out(QStringLiteral("──────────────────────────────────────────────────────────────"));
    out(QStringLiteral("  %1 passed, %2 failed — %3 ms").arg(rep.pass).arg(rep.fail).arg(total.elapsed()));
    return rep.fail == 0 ? 0 : 1;
}

} // namespace fincept::mcp
