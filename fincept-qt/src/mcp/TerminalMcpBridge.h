#pragma once
// TerminalMcpBridge.h — Local HTTP bridge that exposes the internal MCP tool
// catalog to the Python `finagent_core` agent subprocess.
//
// The Python side (`scripts/agents/finagent_core/tools/terminal_toolkit.py`)
// speaks this contract:
//
//   POST <endpoint>/tool   body: {id, tool, args, serverId?} → ToolResult JSON
//   GET  <endpoint>/tools                → {tools: [{name, description, inputSchema, serverId, timeoutMs}]}
//
// Auth: every request must include `X-MCP-Token: <token>`, compared in constant
// time against the live RUN tokens minted by `begin_run()` — one per agent
// process, bound to that agent's own tool policy and retired when the process
// exits. There is no process-wide token: a credential that outlives every run
// and binds no policy had no caller and was pure attack surface. Agents share
// the parent's user session, so the token only guards against OTHER local
// processes stumbling onto the port.
//
// Result size: responses are shaped to a 24 KB budget (structure-aware, via
// ResultStore/shrink_json). Anything trimmed is marked in place and the full
// payload is parked for `result_fetch` — never silently cut.
//
// Every request is also checked against auth/LoopbackGuard.h (Host must be our
// own loopback authority; no cross-site fetch metadata), which is what stops a
// web page the user has open — or a DNS-rebound hostname — from driving this
// port. This endpoint is machine-to-machine, so cross-site navigations are
// rejected too.
//
// Transport: QTcpServer + manual HTTP/1.1 framing (Qt6::HttpServer is not
// available in this Qt 6.8.3 build). Localhost-only binding on 127.0.0.1:0
// (OS-assigned port). Connection: close after each response — no keep-alive.

#include "mcp/McpTypes.h"

#include <QHash>
#include <QJsonArray>
#include <QMutex>
#include <QObject>
#include <QString>

#include <optional>

class QTcpServer;
class QTcpSocket;
class QThreadPool;

namespace fincept::mcp {

/// What an agent run is shown, and therefore what it may dispatch.
///   Essentials — the agent config named no categories / name patterns: the
///                curated essentials set (mcp/server/EssentialTools.h) incl.
///                tool_list / tool_describe / tool_invoke, which reach every
///                other tool the run's policy permits.
///   Filtered   — the agent named categories or name patterns: exactly the
///                matching tools (McpService::get_agent_tools), capped only by
///                the agent's own max_tools.
enum class AgentToolCatalog { Essentials, Filtered };

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

    /// Open a run scope and mint the `X-MCP-Token` the agent should present.
    ///
    /// The token binds `filter` / `include_external` / `catalog` to every call
    /// made with it, so the per-agent tool policy is enforced at DISPATCH and
    /// not only when the catalog is built. Without this an agent that knows a
    /// tool's name could POST a tool its own configuration excluded — the
    /// catalog is a hint, the bridge is the boundary.
    ///
    /// Returns an empty string when the bridge is not listening (nothing to
    /// scope). Pair with `end_run()`; scopes are also swept by age and count so
    /// a caller that never retires one cannot grow the map without bound.
    QString begin_run(const ToolFilter& filter, bool include_external, AgentToolCatalog catalog);

    /// Retire a run scope minted by `begin_run()`. No-op for an empty token or
    /// a token that has already been retired.
    void end_run(const QString& run_token);

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

    /// RAII helper that re-establishes the agent-gating flags on ANOTHER thread.
    /// The flags above are thread_local, so when an external tool call hops onto
    /// a QtConcurrent pool thread (McpService::execute_openai_function_async) the
    /// agent context is lost and the auth checker would treat it as a chat call
    /// — bypassing the destructive-tool gate. Capture the flags on the bridge
    /// thread, then construct one of these inside the pool-thread lambda.
    class ScopedCallFlags {
      public:
        ScopedCallFlags(bool call_in_progress, bool destructive_allowed);
        ~ScopedCallFlags();
        ScopedCallFlags(const ScopedCallFlags&) = delete;
        ScopedCallFlags& operator=(const ScopedCallFlags&) = delete;

      private:
        bool prev_in_progress_;
        bool prev_destructive_;
    };

    /// Build the catalog payload that AgentService injects into
    /// `config["terminal_tools"]`. Output shape (per item):
    ///   { "name": str, "description": str, "inputSchema": {JSON Schema},
    ///     "serverId": str, "timeoutMs": int }
    /// camelCase `inputSchema` matches the MCP wire spec and the Python
    /// `TerminalToolkit`. `serverId` must be echoed back on POST /tool for an
    /// external tool (otherwise the call is routed to the internal registry);
    /// `timeoutMs` is the tool's own budget, which the toolkit uses for its HTTP
    /// timeout instead of one fixed value for every tool.
    ///
    /// Every entry is one this run's policy will actually dispatch — the same
    /// check handle_post_tool applies — so the catalog never advertises a tool
    /// the bridge would refuse.
    ///
    /// `include_external` controls whether tools from user-configured
    /// external MCP servers (Notion, Slack, etc., visible in the MCP Servers
    /// tab) are included alongside internal Fincept tools.
    QJsonArray tool_definitions(const ToolFilter& filter, bool include_external, AgentToolCatalog catalog) const;

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
        /// Run scope this request authenticated against. Resolved once in
        /// try_dispatch() so the handlers don't re-walk the token map.
        QString run_token;
    };

    /// One live agent run: the tool policy its calls are held to.
    struct RunScope {
        ToolFilter filter;
        bool include_external = true;
        AgentToolCatalog catalog = AgentToolCatalog::Filtered;
        qint64 started_ms = 0;
    };

    bool parse_headers(RequestState& st);
    void try_dispatch(QTcpSocket* sock);

    /// Constant-time match of a supplied `X-MCP-Token` against every live run
    /// token. On a hit, `run_token_out` receives it.
    bool authenticate(const QString& supplied, QString* run_token_out) const;

    std::optional<RunScope> run_scope(const QString& run_token) const;

    /// Drop run scopes that are too old or too numerous. Caller holds
    /// `runs_mutex_`. A leaked scope is fail-safe (the policy stays enforced),
    /// but it must not accumulate for the life of the process.
    void sweep_runs_locked();

    void handle_post_tool(QTcpSocket* sock, const QJsonObject& body);
    void handle_get_tools(QTcpSocket* sock);

    void write_json_response(QTcpSocket* sock, int status, const QJsonObject& body);
    void write_error(QTcpSocket* sock, int status, const QString& message);

    QTcpServer* server_ = nullptr;
    bool active_ = false;
    QString destructive_token_;
    QHash<QTcpSocket*, RequestState> states_;

    /// Dedicated pool for the synchronous part of a dispatch (auth, schema
    /// validation, sync handlers, async kick-off). Never the global pool: the
    /// LLM tool fan-out and McpService's external-tool hops live there, and an
    /// agent hammering the bridge must not starve them. Created on first use.
    QThreadPool* dispatch_pool_ = nullptr;

    /// Live run scopes keyed by the run token handed to the agent. Socket
    /// handling is GUI-thread-only, but begin_run/end_run are called from the
    /// agent layer, so this one map is mutex-guarded.
    QHash<QString, RunScope> runs_;
    mutable QMutex runs_mutex_;
};

} // namespace fincept::mcp
