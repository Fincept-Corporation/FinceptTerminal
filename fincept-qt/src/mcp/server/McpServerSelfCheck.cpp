// McpServerSelfCheck.cpp — see McpServerSelfCheck.h.

#include "mcp/server/McpServerSelfCheck.h"

#include "core/config/EditionNames.h"
#include "core/logging/Logger.h"
#include "mcp/server/McpClientProfile.h"
#include "mcp/server/McpWire.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QThread>
#include <QUrl>

#include <algorithm>

namespace fincept::mcp {

namespace self_check_detail {

constexpr const char* kTag = "McpSelfCheck";

struct HttpResult {
    bool ok = false;
    int status = 0;
    QHash<QByteArray, QByteArray> headers;
    QByteArray body;
    QString error;
};

/// One request on a fresh loopback connection; reads a Content-Length or a
/// chunked body (an SSE answer is chunked). Blocking, bounded by `timeout_ms`.
HttpResult http_exchange(const QUrl& url, const QByteArray& method, const QList<QPair<QByteArray, QByteArray>>& headers,
                         const QByteArray& body, int timeout_ms) {
    HttpResult r;
    QTcpSocket sock;
    QElapsedTimer t;
    t.start();
    sock.connectToHost(url.host(), static_cast<quint16>(url.port(80)));
    if (!sock.waitForConnected(std::max(500, timeout_ms / 3))) {
        r.error = QStringLiteral("cannot connect: %1").arg(sock.errorString());
        return r;
    }
    QByteArray raw = method + ' ' + url.path().toUtf8() + " HTTP/1.1\r\nHost: " + url.host().toUtf8() + ':' +
                     QByteArray::number(url.port(80)) + "\r\n";
    for (const auto& h : headers)
        raw += h.first + ": " + h.second + "\r\n";
    raw += "Content-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
    sock.write(raw);
    QByteArray buf;
    qsizetype body_start = -1;
    auto remaining = [&]() { return static_cast<int>(std::max<qint64>(1, timeout_ms - t.elapsed())); };
    while (t.elapsed() < timeout_ms) {
        if (sock.bytesAvailable() == 0 && !sock.waitForReadyRead(remaining()))
            break;
        buf += sock.readAll();
        if (body_start < 0) {
            const qsizetype end = buf.indexOf("\r\n\r\n");
            if (end < 0)
                continue;
            body_start = end + 4;
            const QList<QByteArray> lines = buf.left(end).split('\n');
            r.status = lines.value(0).trimmed().split(' ').value(1).toInt();
            for (qsizetype i = 1; i < lines.size(); ++i) {
                const QByteArray line = lines.at(i).trimmed();
                const qsizetype colon = line.indexOf(':');
                if (colon > 0)
                    r.headers.insert(line.left(colon).trimmed().toLower(), line.mid(colon + 1).trimmed());
            }
        }
        const QByteArray rest = buf.mid(body_start);
        if (r.headers.value("transfer-encoding").toLower() == "chunked") {
            if (rest.endsWith("0\r\n\r\n"))
                break;
        } else if (r.headers.contains("content-length")) {
            if (rest.size() >= r.headers.value("content-length").toLongLong())
                break;
        }
    }
    if (body_start < 0) {
        r.error = sock.state() == QAbstractSocket::ConnectedState ? QStringLiteral("no answer within %1 ms").arg(timeout_ms)
                                                                  : QStringLiteral("connection closed without an answer");
        return r;
    }
    QByteArray rest = buf.mid(body_start);
    if (r.headers.value("transfer-encoding").toLower() == "chunked") {
        QByteArray decoded;
        qsizetype pos = 0;
        for (;;) {
            const qsizetype eol = rest.indexOf("\r\n", pos);
            if (eol < 0)
                break;
            bool ok = false;
            const qint64 n = rest.mid(pos, eol - pos).trimmed().toLongLong(&ok, 16);
            if (!ok || n <= 0)
                break;
            decoded += rest.mid(eol + 2, n);
            pos = eol + 2 + n + 2;
        }
        rest = decoded;
    } else if (r.headers.contains("content-length")) {
        rest = rest.left(r.headers.value("content-length").toLongLong());
    }
    r.body = rest;
    r.ok = true;
    return r;
}

/// The JSON-RPC message answering `id` in a JSON or text/event-stream body.
QJsonObject rpc_answer(const HttpResult& r, int id) {
    if (r.headers.value("content-type").startsWith("text/event-stream")) {
        for (const QByteArray& line : r.body.split('\n')) {
            if (!line.startsWith("data:"))
                continue;
            const QJsonObject o = QJsonDocument::fromJson(line.mid(5).trimmed()).object();
            if (o.value(QStringLiteral("id")).toInt(-1) == id)
                return o;
        }
        return {};
    }
    return QJsonDocument::fromJson(r.body).object();
}

struct ProbeClient {
    profiles::ClientKind kind;
    const char* name;
    const char* protocol;
};

// What each harness sends in `initialize` (Codex: codex-rs, 2025-06-18; Gemini
// CLI / Qwen Code: their mcp-client.ts; the rest as reported by the vendors or
// assumed — detection only needs the product name in it).
const QList<ProbeClient>& probe_clients() {
    static const QList<ProbeClient> k = {
        {profiles::ClientKind::Generic, "fincept-self-check", "2025-11-25"},
        {profiles::ClientKind::ClaudeCode, "claude-code", "2025-11-25"},
        {profiles::ClientKind::ClaudeDesktop, "claude-ai", "2025-06-18"},
        {profiles::ClientKind::Codex, "codex-mcp-client", "2025-06-18"},
        {profiles::ClientKind::GeminiCli, "gemini-cli-mcp-client", "2025-06-18"},
        {profiles::ClientKind::QwenCode, "qwen-code-mcp-client", "2025-06-18"},
        {profiles::ClientKind::Cursor, "cursor-vscode", "2025-06-18"},
        {profiles::ClientKind::Copilot, "github-copilot-cli", "2025-06-18"},
        {profiles::ClientKind::Grok, "grok-cli", "2025-06-18"},
        {profiles::ClientKind::VsCode, "Visual Studio Code", "2025-06-18"},
    };
    return k;
}

QByteArray rpc_body(int id, const QString& method, const QJsonObject& params) {
    QJsonObject m{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                  {QStringLiteral("id"), id},
                  {QStringLiteral("method"), method}};
    if (!params.isEmpty())
        m.insert(QStringLiteral("params"), params);
    return QJsonDocument(m).toJson(QJsonDocument::Compact);
}

} // namespace self_check_detail

QStringList SelfCheckReport::lines() const {
    QStringList out;
    for (const SelfCheckStep& s : steps) {
        out << QStringLiteral("[%1] %2 · %3%4 (%5 ms)")
                   .arg(s.ok ? QStringLiteral("PASS") : QStringLiteral("FAIL"), s.profile, s.step,
                        s.detail.isEmpty() ? QString() : QStringLiteral(" — ") + s.detail)
                   .arg(s.ms);
    }
    return out;
}

SelfCheckReport run_mcp_self_check(const SelfCheckOptions& options) {
    using namespace self_check_detail;
    SelfCheckReport report;
    const QUrl url(options.url);
    const QByteArray bearer = QByteArrayLiteral("Bearer ") + options.token.toUtf8();
    auto add = [&report](const QString& profile, const QString& step, bool ok, const QString& detail, qint64 ms) {
        report.steps.append(SelfCheckStep{profile, step, ok, detail, ms});
    };
    if (!url.isValid() || url.host() != QLatin1String("127.0.0.1")) {
        add(QStringLiteral("server"), QStringLiteral("address"), false, QStringLiteral("the MCP server is not running"), 0);
        return report;
    }
    static const QRegularExpression name_rx(QStringLiteral("^[A-Za-z0-9_-]{1,64}$"));

    // Whose server is this? Asked WITHOUT a token (GET /health): the other
    // Fincept edition may be listening on this port, and a token is never sent
    // to it. Nothing else runs unless the edition matches.
    {
        QElapsedTimer t;
        t.start();
        QUrl health = url;
        health.setPath(QStringLiteral("/health"));
        const HttpResult r = http_exchange(health, "GET", {}, QByteArray(), options.timeout_ms);
        const QString edition = QJsonDocument::fromJson(r.body).object().value(QStringLiteral("edition")).toString();
        const bool ours = r.ok && r.status == 200 && edition == options.expected_edition;
        add(QStringLiteral("server"), QStringLiteral("edition"), ours,
            ours ? QStringLiteral("this terminal (%1 edition)").arg(edition)
                 : !r.ok ? QStringLiteral("no answer at %1: %2").arg(url.toString(), r.error)
                 : edition.isEmpty()
                     ? QStringLiteral("the program on port %1 is not this Fincept Terminal (no edition reported); "
                                      "no token was sent")
                           .arg(url.port())
                     : QStringLiteral("port %1 is answered by the %2 edition, not this %3 edition; no token was sent")
                           .arg(url.port())
                           .arg(edition, options.expected_edition),
            t.elapsed());
        if (!ours)
            return report;
    }

    // The MCP 2026-07-28 fallback: a modern probe must get a plain 400 so a
    // dual-era client (Claude Code's newer runtime) falls back to initialize.
    {
        QElapsedTimer t;
        t.start();
        const HttpResult r = http_exchange(url, "POST",
                                           {{"Authorization", bearer},
                                            {"Content-Type", "application/json"},
                                            {"Accept", "application/json, text/event-stream"},
                                            {"MCP-Protocol-Version", "2026-07-28"}},
                                           rpc_body(1, QStringLiteral("server/discover"), {}), options.timeout_ms);
        add(QStringLiteral("protocol"), QStringLiteral("2026-07-28 probe falls back"), r.ok && r.status == 400,
            r.ok ? QStringLiteral("HTTP %1").arg(r.status) : r.error, t.elapsed());
    }

    for (const ProbeClient& pc : probe_clients()) {
        const profiles::ClientProfile& prof = profiles::profile(pc.kind);
        const QString who = QString::fromLatin1(prof.id);
        QList<QPair<QByteArray, QByteArray>> headers{{"Authorization", bearer},
                                                     {"Content-Type", "application/json"},
                                                     {"Accept", "application/json, text/event-stream"},
                                                     {"User-Agent", QByteArray(pc.name) + "/self-check"}};
        // initialize
        QElapsedTimer t;
        t.start();
        const QJsonObject init_params{
            {QStringLiteral("protocolVersion"), QString::fromLatin1(pc.protocol)},
            {QStringLiteral("capabilities"), QJsonObject{}},
            {QStringLiteral("clientInfo"),
             QJsonObject{{QStringLiteral("name"), QString::fromLatin1(pc.name)}, {QStringLiteral("version"), QStringLiteral("self-check")}}}};
        HttpResult r = http_exchange(url, "POST", headers, rpc_body(1, QStringLiteral("initialize"), init_params),
                                     options.timeout_ms);
        const QJsonObject init = rpc_answer(r, 1).value(QStringLiteral("result")).toObject();
        const QByteArray session = r.headers.value("mcp-session-id");
        const bool init_ok = r.ok && r.status == 200 &&
                             init.value(QStringLiteral("protocolVersion")).toString() == QLatin1String(pc.protocol) &&
                             !session.isEmpty();
        add(who, QStringLiteral("initialize"), init_ok,
            init_ok ? QStringLiteral("protocol %1, session issued").arg(QLatin1String(pc.protocol))
                    : (r.ok ? QStringLiteral("HTTP %1").arg(r.status) : r.error),
            t.elapsed());
        if (!init_ok)
            continue;
        headers.append({"Mcp-Session-Id", session});
        headers.append({"MCP-Protocol-Version", QByteArray(pc.protocol)});

        t.restart();
        r = http_exchange(url, "POST", headers,
                          QJsonDocument(QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                                                    {QStringLiteral("method"), QStringLiteral("notifications/initialized")}})
                              .toJson(QJsonDocument::Compact),
                          options.timeout_ms);
        add(who, QStringLiteral("initialized"), r.ok && r.status == 202, r.ok ? QStringLiteral("HTTP %1").arg(r.status) : r.error,
            t.elapsed());

        // tools/list, checked against the profile
        t.restart();
        r = http_exchange(url, "POST", headers, rpc_body(2, QStringLiteral("tools/list"), {}), options.timeout_ms);
        const QJsonArray tools = rpc_answer(r, 2).value(QStringLiteral("result")).toObject().value(QStringLiteral("tools")).toArray();
        QStringList problems;
        for (const QJsonValue& v : tools) {
            const QJsonObject tool = v.toObject();
            const QString name = tool.value(QStringLiteral("name")).toString();
            if (!name_rx.match(name).hasMatch() || name.size() > prof.tool_name_budget)
                problems << QStringLiteral("name %1").arg(name);
            const QJsonObject schema = tool.value(QStringLiteral("inputSchema")).toObject();
            if (schema.value(QStringLiteral("type")).toString() != QLatin1String("object"))
                problems << QStringLiteral("%1 schema is not an object").arg(name);
            QString why;
            if (prof.dialect == profiles::SchemaDialect::OpenAiStrict && !profiles::is_strict_schema(schema, &why))
                problems << QStringLiteral("%1 not strict (%2)").arg(name, why);
        }
        const bool list_ok = r.ok && r.status == 200 && !tools.isEmpty() && problems.isEmpty();
        add(who, QStringLiteral("tools/list"), list_ok,
            list_ok ? QStringLiteral("%1 tools, %2 KB").arg(tools.size()).arg(r.body.size() / 1024.0, 0, 'f', 1)
                    : (problems.isEmpty() ? (r.ok ? QStringLiteral("HTTP %1").arg(r.status) : r.error)
                                          : problems.mid(0, 3).join(QStringLiteral("; "))),
            t.elapsed());

        // one harmless call
        if (options.call_tool) {
            t.restart();
            r = http_exchange(url, "POST", headers,
                              rpc_body(3, QStringLiteral("tools/call"),
                                       QJsonObject{{QStringLiteral("name"), QStringLiteral("ui_overview")},
                                                   {QStringLiteral("arguments"), QJsonObject{}}}),
                              options.timeout_ms);
            const QJsonObject res = rpc_answer(r, 3).value(QStringLiteral("result")).toObject();
            const QJsonArray content = res.value(QStringLiteral("content")).toArray();
            const bool call_ok = r.ok && r.status == 200 && !content.isEmpty() &&
                                 content.at(0).toObject().value(QStringLiteral("type")).toString() == QLatin1String("text") &&
                                 (prof.structured_content || !res.contains(QStringLiteral("structuredContent")));
            add(who, QStringLiteral("tools/call ui_overview"), call_ok,
                call_ok ? (res.value(QStringLiteral("isError")).toBool() ? QStringLiteral("answered (tool reported an error)")
                                                                         : QStringLiteral("answered"))
                        : (r.ok ? QStringLiteral("HTTP %1, unexpected result shape").arg(r.status) : r.error),
                t.elapsed());
        }

        t.restart();
        r = http_exchange(url, "POST", headers, rpc_body(4, QStringLiteral("ping"), {}), options.timeout_ms);
        add(who, QStringLiteral("ping"), r.ok && r.status == 200 && rpc_answer(r, 4).contains(QStringLiteral("result")),
            r.ok ? QStringLiteral("HTTP %1").arg(r.status) : r.error, t.elapsed());

        t.restart();
        r = http_exchange(url, "DELETE", headers, QByteArray(), options.timeout_ms);
        add(who, QStringLiteral("end session"), r.ok && r.status == 200, r.ok ? QStringLiteral("HTTP %1").arg(r.status) : r.error,
            t.elapsed());
    }

    // The stdio bridge (clients that only speak stdio).
    if (!options.python.isEmpty() && QFileInfo::exists(options.bridge_script)) {
        QElapsedTimer t;
        t.start();
        QProcess proc;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.remove(QString::fromLatin1(edition::kForeignMcpTokenEnvVar)); // never the other edition's token
        env.insert(QString::fromLatin1(edition::kMcpTokenEnvVar), options.token);
        proc.setProcessEnvironment(env);
        proc.start(options.python, {options.bridge_script, QStringLiteral("--url"), options.url});
        bool ok = proc.waitForStarted(10000);
        QString detail = ok ? QString() : QStringLiteral("could not start Python: %1").arg(proc.errorString());
        auto ask = [&proc, &options](const QByteArray& line, int id) {
            proc.write(line + '\n');
            QByteArray got;
            QElapsedTimer w;
            w.start();
            while (w.elapsed() < options.timeout_ms) {
                if (!proc.canReadLine() && !proc.waitForReadyRead(200))
                    continue;
                while (proc.canReadLine()) {
                    const QJsonObject o = QJsonDocument::fromJson(proc.readLine().trimmed()).object();
                    if (o.value(QStringLiteral("id")).toInt(-1) == id)
                        return o;
                }
            }
            return QJsonObject{};
        };
        if (ok) {
            const QJsonObject init = ask(rpc_body(1, QStringLiteral("initialize"),
                                                  QJsonObject{{QStringLiteral("protocolVersion"), QStringLiteral("2025-06-18")},
                                                              {QStringLiteral("capabilities"), QJsonObject{}},
                                                              {QStringLiteral("clientInfo"),
                                                               QJsonObject{{QStringLiteral("name"), QStringLiteral("fincept-self-check")}}}}),
                                         1);
            const QJsonObject list = ask(rpc_body(2, QStringLiteral("tools/list"), {}), 2);
            ok = init.value(QStringLiteral("result")).toObject().contains(QStringLiteral("serverInfo")) &&
                 !list.value(QStringLiteral("result")).toObject().value(QStringLiteral("tools")).toArray().isEmpty();
            detail = ok ? QStringLiteral("initialize + tools/list through stdio")
                        : QStringLiteral("no valid answer through the bridge");
        }
        proc.closeWriteChannel(); // the bridge exits when stdin closes
        if (!proc.waitForFinished(8000)) {
            proc.kill();
            proc.waitForFinished(2000);
            ok = false;
            detail += QStringLiteral("; did not exit when stdin closed");
        }
        add(QStringLiteral("stdio-bridge"), QStringLiteral("round trip"), ok, detail, t.elapsed());
    }

    report.ok = !report.steps.isEmpty() &&
                std::all_of(report.steps.cbegin(), report.steps.cend(), [](const SelfCheckStep& s) { return s.ok; });
    int failed = 0;
    for (const SelfCheckStep& s : report.steps)
        failed += s.ok ? 0 : 1;
    LOG_INFO(self_check_detail::kTag,
             logf("MCP connection self-check finished", {{"steps", static_cast<int>(report.steps.size())}, {"failed", failed}}));
    return report;
}

} // namespace fincept::mcp
