// TerminalMcpBridge.cpp — Local HTTP/1.1 bridge for the Python finagent toolkit.
//
// Why hand-rolled HTTP framing: Qt 6.8.3 in this build does not include the
// QHttpServer module (only Qt6::Network and Qt6::WebSockets are present).
// The contract is small and we control both ends, so a tiny parser is
// cheaper than adding a Qt module dependency.

#include "mcp/TerminalMcpBridge.h"

#include "core/logging/Logger.h"
#include "mcp/McpManager.h"
#include "mcp/McpProvider.h"
#include "mcp/McpService.h"

#include <QFutureWatcher>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUuid>

namespace fincept::mcp {

static constexpr const char* TAG = "TerminalMcpBridge";

// HTTP/1.1 framing limits. Localhost-only with a known peer (the agent
// subprocess), so generous but bounded — anything past these is a bug or an
// unwelcome process probing the port.
static constexpr int kMaxHeaderBytes = 16 * 1024;
static constexpr int kMaxBodyBytes = 4 * 1024 * 1024; // 4 MB

// Per-thread flags: set by the bridge across the synchronous portion of
// McpProvider::call_tool_async (where auth runs). The McpProvider auth
// checker reads these to distinguish agent-originated calls from chat-path
// calls without changing the AuthChecker signature.
static thread_local bool tls_call_in_progress = false;
static thread_local bool tls_destructive_allowed = false;

bool TerminalMcpBridge::is_call_in_progress() {
    return tls_call_in_progress;
}

bool TerminalMcpBridge::is_destructive_allowed() {
    return tls_destructive_allowed;
}

namespace {
struct CallFlagGuard {
    explicit CallFlagGuard(bool destructive_ok) {
        tls_call_in_progress = true;
        tls_destructive_allowed = destructive_ok;
    }
    ~CallFlagGuard() {
        tls_call_in_progress = false;
        tls_destructive_allowed = false;
    }
};
} // namespace

TerminalMcpBridge& TerminalMcpBridge::instance() {
    static TerminalMcpBridge s;
    return s;
}

TerminalMcpBridge::TerminalMcpBridge(QObject* parent) : QObject(parent) {}

// ── Lifecycle ────────────────────────────────────────────────────────────────

bool TerminalMcpBridge::start() {
    if (active_)
        return true;

    server_ = new QTcpServer(this);
    connect(server_, &QTcpServer::newConnection, this, &TerminalMcpBridge::on_new_connection);

    if (!server_->listen(QHostAddress::LocalHost, 0)) {
        LOG_ERROR(TAG, "Failed to bind 127.0.0.1: " + server_->errorString());
        server_->deleteLater();
        server_ = nullptr;
        return false;
    }

    token_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    destructive_token_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    active_ = true;
    LOG_INFO(TAG, QString("Listening on %1 (tokens issued)").arg(endpoint()));
    return true;
}

void TerminalMcpBridge::stop() {
    if (!active_)
        return;
    active_ = false;

    for (auto it = states_.constBegin(); it != states_.constEnd(); ++it) {
        if (it.key()) {
            it.key()->disconnectFromHost();
            it.key()->deleteLater();
        }
    }
    states_.clear();

    if (server_) {
        server_->close();
        server_->deleteLater();
        server_ = nullptr;
    }
    LOG_INFO(TAG, "Stopped");
}

QString TerminalMcpBridge::endpoint() const {
    if (!active_ || !server_)
        return {};
    return QString("http://127.0.0.1:%1").arg(server_->serverPort());
}

// ── Tool catalog serialisation ───────────────────────────────────────────────

QJsonArray TerminalMcpBridge::tool_definitions(const ToolFilter& filter) const {
    // Goes through McpService so external MCP servers (configured via the
    // MCP Servers tab) are visible to agents too. The returned UnifiedTool
    // list is already filter-applied and capped.
    auto tools = McpService::instance().get_all_tools(filter);

    QJsonArray out;
    for (const auto& t : tools) {
        QJsonObject schema = t.input_schema;
        if (schema.isEmpty()) {
            schema["type"] = "object";
            schema["properties"] = QJsonObject();
        }
        QJsonObject entry;
        entry["name"] = t.name;
        entry["description"] = t.description;
        // camelCase per MCP wire spec — what TerminalToolkit reads.
        entry["inputSchema"] = schema;
        // Embed server_id so the bridge can route external tools without a
        // separate name-prefix scheme. Optional — Python ignores unknown keys.
        entry["serverId"] = t.server_id;
        out.append(entry);
    }
    return out;
}

// ── Connection handling ─────────────────────────────────────────────────────

void TerminalMcpBridge::on_new_connection() {
    while (server_ && server_->hasPendingConnections()) {
        QTcpSocket* sock = server_->nextPendingConnection();
        states_.insert(sock, RequestState{});
        connect(sock, &QTcpSocket::readyRead, this, &TerminalMcpBridge::on_ready_read);
        connect(sock, &QTcpSocket::disconnected, this, &TerminalMcpBridge::on_disconnected);
    }
}

void TerminalMcpBridge::on_disconnected() {
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock)
        return;
    states_.remove(sock);
    sock->deleteLater();
}

void TerminalMcpBridge::on_ready_read() {
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock)
        return;
    auto it = states_.find(sock);
    if (it == states_.end())
        return;

    RequestState& st = it.value();
    st.buffer.append(sock->readAll());

    if (!st.headers_parsed) {
        const int header_end = st.buffer.indexOf("\r\n\r\n");
        if (header_end < 0) {
            if (st.buffer.size() > kMaxHeaderBytes) {
                write_error(sock, 431, "Request header too large");
            }
            return; // wait for more
        }
        if (!parse_headers(st)) {
            write_error(sock, 400, "Malformed request");
            return;
        }
        if (st.content_length > kMaxBodyBytes) {
            write_error(sock, 413, "Payload too large");
            return;
        }
    }

    // Body fully arrived?
    const int header_end = st.buffer.indexOf("\r\n\r\n");
    const int body_start = header_end + 4;
    const int body_have = st.buffer.size() - body_start;
    if (body_have < st.content_length)
        return; // wait for more bytes

    try_dispatch(sock);
}

bool TerminalMcpBridge::parse_headers(RequestState& st) {
    const int header_end = st.buffer.indexOf("\r\n\r\n");
    if (header_end < 0)
        return false;
    const QByteArray head = st.buffer.left(header_end);
    const QList<QByteArray> lines = head.split('\n');
    if (lines.isEmpty())
        return false;

    // Request line: METHOD SP PATH SP HTTP/1.1
    const QByteArray req_line = lines.front().trimmed();
    const QList<QByteArray> parts = req_line.split(' ');
    if (parts.size() < 2)
        return false;
    st.method = QString::fromLatin1(parts[0]);
    st.path = QString::fromLatin1(parts[1]);

    for (int i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines[i].trimmed();
        if (line.isEmpty())
            continue;
        const int colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        const QString key = QString::fromLatin1(line.left(colon).trimmed()).toLower();
        const QString val = QString::fromLatin1(line.mid(colon + 1).trimmed());
        st.headers.insert(key, val);
    }
    st.content_length = st.headers.value("content-length", "0").toInt();
    st.headers_parsed = true;
    return true;
}

// ── Request routing ─────────────────────────────────────────────────────────

void TerminalMcpBridge::try_dispatch(QTcpSocket* sock) {
    auto it = states_.find(sock);
    if (it == states_.end())
        return;
    RequestState& st = it.value();

    // Authn — must come before any work happens.
    const QString supplied = st.headers.value("x-mcp-token");
    if (supplied != token_) {
        LOG_WARN(TAG, QString("Rejecting request — token mismatch (path=%1)").arg(st.path));
        write_error(sock, 401, "Invalid or missing X-MCP-Token");
        return;
    }

    // Strip query string for path matching.
    const QString path_only = st.path.section('?', 0, 0);

    if (st.method == "POST" && path_only == "/tool") {
        const int header_end = st.buffer.indexOf("\r\n\r\n");
        const QByteArray body = st.buffer.mid(header_end + 4, st.content_length);
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isObject()) {
            write_error(sock, 400, "Invalid JSON body");
            return;
        }
        handle_post_tool(sock, doc.object());
        return;
    }

    if (st.method == "GET" && path_only == "/tools") {
        handle_get_tools(sock);
        return;
    }

    write_error(sock, 404, QString("Unknown route: %1 %2").arg(st.method, path_only));
}

// ── POST /tool ──────────────────────────────────────────────────────────────

void TerminalMcpBridge::handle_post_tool(QTcpSocket* sock, const QJsonObject& body) {
    const QString call_id = body.value("id").toString();
    const QString tool_name = body.value("tool").toString();
    const QJsonObject args = body.value("args").toObject();
    // Optional — Python may set serverId for external routing. Defaults to
    // the internal Fincept catalog so legacy callers keep working.
    const QString server_id = body.value("serverId").toString(QString(INTERNAL_SERVER_ID));

    if (tool_name.isEmpty()) {
        write_error(sock, 400, "Missing 'tool' field");
        return;
    }

    LOG_INFO(TAG, QString("Tool call %1: %2 on %3").arg(call_id, tool_name, server_id));

    QPointer<TerminalMcpBridge> self = this;
    QPointer<QTcpSocket> sock_guard = sock;

    // Internal vs external routing. Both shapes return QFuture<ToolResult>
    // and we resolve via QFutureWatcher so the socket write happens on the
    // main thread when the future completes — never blocks newConnection.
    // The auth checker (installed by AgentService) inspects
    // is_call_in_progress() and is_destructive_allowed() to apply
    // agent-specific gating. The flag scope covers only the synchronous
    // dispatch — the auth check runs there before call_tool_async returns
    // its future.
    auto it_state = states_.find(sock);
    const QString destructive_hdr = (it_state != states_.end())
                                        ? it_state.value().headers.value("x-mcp-allow-destructive")
                                        : QString();
    const bool destructive_ok =
        !destructive_token_.isEmpty() && destructive_hdr == destructive_token_;

    QFuture<ToolResult> future;
    {
        CallFlagGuard guard(destructive_ok);
        if (server_id == INTERNAL_SERVER_ID) {
            future = McpProvider::instance().call_tool_async(tool_name, args);
        } else {
            // External — McpService::execute_openai_function_async handles the
            // QtConcurrent::run wrapping for blocking JSON-RPC clients. Build
            // the wire-form name it expects.
            const QString fn = server_id + "__" + McpProvider::encode_tool_name_for_wire(tool_name);
            future = McpService::instance().execute_openai_function_async(fn, args);
        }
    }

    auto* watcher = new QFutureWatcher<ToolResult>(this);
    connect(watcher, &QFutureWatcher<ToolResult>::finished, this,
            [self, sock_guard, tool_name, call_id, watcher]() {
                const auto fut = watcher->future();
                ToolResult result = (fut.resultCount() > 0)
                                        ? fut.result()
                                        : ToolResult::fail("Tool produced no result");
                watcher->deleteLater();

                if (!self || !sock_guard) {
                    return; // bridge or peer gone
                }

                QJsonObject payload = result.to_json();
                if (!call_id.isEmpty())
                    payload["id"] = call_id;
                self->write_json_response(sock_guard, 200, payload);
                emit self->tool_called(tool_name, result.success);
            });
    watcher->setFuture(future);
}

// ── GET /tools ──────────────────────────────────────────────────────────────

void TerminalMcpBridge::handle_get_tools(QTcpSocket* sock) {
    // Default filter — the catalog the agent gets at boot is also what
    // dynamic refresh returns. AgentService::build_payload owns the per-run
    // filter; this endpoint is a fallback and uses the same defaults.
    ToolFilter filter;
    filter.exclude_categories = {"navigation", "system", "settings", "ai-chat", "meta"};
    QJsonObject payload;
    payload["tools"] = tool_definitions(filter);
    write_json_response(sock, 200, payload);
}

// ── HTTP response helpers ───────────────────────────────────────────────────

static const char* status_text(int code) {
    switch (code) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 404: return "Not Found";
        case 413: return "Payload Too Large";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        default:  return "Unknown";
    }
}

void TerminalMcpBridge::write_json_response(QTcpSocket* sock, int status, const QJsonObject& body) {
    if (!sock || sock->state() != QAbstractSocket::ConnectedState)
        return;

    const QByteArray body_bytes = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QByteArray response;
    response.reserve(body_bytes.size() + 256);
    response += "HTTP/1.1 " + QByteArray::number(status) + ' ' + status_text(status) + "\r\n";
    response += "Content-Type: application/json\r\n";
    response += "Content-Length: " + QByteArray::number(body_bytes.size()) + "\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += body_bytes;

    sock->write(response);
    sock->flush();
    sock->disconnectFromHost();
}

void TerminalMcpBridge::write_error(QTcpSocket* sock, int status, const QString& message) {
    QJsonObject body;
    body["success"] = false;
    body["error"] = message;
    write_json_response(sock, status, body);
}

} // namespace fincept::mcp
