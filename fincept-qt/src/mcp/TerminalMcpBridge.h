#pragma once
// TerminalMcpBridge.h — Local HTTP bridge that exposes the internal MCP tool
// catalog to the Python `finagent_core` agent subprocess.
//
// The Python side (`scripts/agents/finagent_core/tools/terminal_toolkit.py`)
// already speaks this contract:
//
//   POST <endpoint>/tool       body: {id, tool, args}   → ToolResult JSON
//   (optional) GET <endpoint>/tools                     → [{name, description, inputSchema}]
//
// Auth: every request must include `X-MCP-Token: <token>`. The token is a
// UUID generated per-process and injected into the agent config payload by
// AgentService — agents share the parent's user session, so the token only
// guards against OTHER local processes stumbling onto the port.
//
// Transport: QTcpServer + manual HTTP/1.1 framing (Qt6::HttpServer is not
// available in this Qt 6.8.3 build). Localhost-only binding on 127.0.0.1:0
// (OS-assigned port). Connection: close after each response — no keep-alive.

#include "mcp/McpTypes.h"

#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QString>

class QTcpServer;
class QTcpSocket;

namespace fincept::mcp {

class TerminalMcpBridge : public QObject {
    Q_OBJECT
  public:
    static TerminalMcpBridge& instance();

    /// Bind the server. Idempotent — repeat calls are no-ops once active.
    /// Returns true if the bridge is listening after the call.
    bool start();

    /// Close the server and drop all live connections.
    void stop();

    bool is_active() const { return active_; }

    /// "http://127.0.0.1:<port>" — no path, no query. Empty when not active.
    QString endpoint() const;

    /// Per-process UUID. Agents include this as `X-MCP-Token` on every call.
    QString token() const { return token_; }

    /// Second per-process UUID. Only injected into agent configs whose
    /// `allow_destructive_tools=true`; the agent's toolkit echoes it back as
    /// `X-MCP-Allow-Destructive` on each request. Bridge compares the header
    /// to this value — match → destructive tools permitted for THAT call.
    /// Treated as a capability token; never log it.
    QString destructive_token() const { return destructive_token_; }

    /// True iff the current thread is inside a bridge-dispatched tool call.
    /// Read by the McpProvider auth checker (installed by AgentService) so it
    /// can apply agent-specific gating without affecting the chat path.
    /// Set on the calling thread for the synchronous portion of
    /// McpProvider::call_tool_async (auth check + dispatch); cleared after.
    static bool is_call_in_progress();

    /// True iff the in-flight bridge call presented the matching destructive
    /// token. Only meaningful while is_call_in_progress() is true.
    static bool is_destructive_allowed();

    /// Build the catalog payload that AgentService injects into
    /// `config["terminal_tools"]`. Output shape (per item):
    ///   { "name": str, "description": str, "inputSchema": {JSON Schema} }
    /// Note: camelCase `inputSchema` — matches MCP wire spec and what the
    /// Python `TerminalToolkit` constructor expects.
    ///
    /// `include_external` controls whether tools from user-configured
    /// external MCP servers (Notion, Slack, etc., visible in the MCP Servers
    /// tab) are included alongside internal Fincept tools. Defaults to true
    /// to match prior behaviour.
    QJsonArray tool_definitions(const ToolFilter& filter, bool include_external = true) const;

    TerminalMcpBridge(const TerminalMcpBridge&) = delete;
    TerminalMcpBridge& operator=(const TerminalMcpBridge&) = delete;

  signals:
    void tool_called(const QString& tool_name, bool success);
    void bridge_error(const QString& message);

  private slots:
    void on_new_connection();
    void on_ready_read();
    void on_disconnected();

  private:
    explicit TerminalMcpBridge(QObject* parent = nullptr);

    struct RequestState {
        QByteArray buffer;
        bool headers_parsed = false;
        QString method;
        QString path;
        int content_length = 0;
        QHash<QString, QString> headers;
    };

    bool parse_headers(RequestState& st);
    void try_dispatch(QTcpSocket* sock);

    void handle_post_tool(QTcpSocket* sock, const QJsonObject& body);
    void handle_get_tools(QTcpSocket* sock);

    void write_json_response(QTcpSocket* sock, int status, const QJsonObject& body);
    void write_error(QTcpSocket* sock, int status, const QString& message);

    QTcpServer* server_ = nullptr;
    bool active_ = false;
    QString token_;
    QString destructive_token_;
    QHash<QTcpSocket*, RequestState> states_;
};

} // namespace fincept::mcp
