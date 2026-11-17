#pragma once
// McpServerEngine.h — the protocol engine of the terminal's external MCP server:
// HTTP/1.1 connections, Streamable-HTTP MCP (JSON-RPC 2.0), sessions, client
// profiles, cancellation, per-call deadlines, concurrency limits and SSE
// streaming for long calls. QtCore + QtNetwork only.
//
// What the engine does NOT know: where tools come from, what the user allowed,
// how a tool runs. That is the McpServerBackend (McpHttpServer_Tools.cpp in the
// app; a fake in tests/tst_mcp_server_engine.cpp), so the whole protocol surface
// is tested against a real loopback socket without the application.
//
// Threading
//   The engine lives on ONE thread (McpHttpServer gives it its own "mcp-http"
//   thread, so nothing here ever runs on — or waits for — the GUI thread). Every
//   socket, timer and table below belongs to that thread. The backend is called
//   on it and must never block it: McpServerBackend::call_tool hands the work to
//   the backend's own pool and reports back through the CallDone callback,
//   which may be invoked from ANY thread, any number of times (only the first
//   counts), or never — the engine's own deadline answers the client anyway.
//
// Transport (MCP 2025-11-25 / 2025-06-18 / 2025-03-26, Streamable HTTP; 2024-11-05
// clients that POST JSON work as well)
//   POST   /mcp  JSON-RPC request, notification, response or batch.
//                 Requests are answered with application/json; a tools/call that
//                 is still running after kSseUpgradeMs — or that has progress to
//                 report — switches its response to text/event-stream (chunked),
//                 sending notifications/progress and keep-alive comments until
//                 the result, so no client-side idle timer cuts a long call.
//                 Only notifications/responses → 202, empty body.
//   GET    /mcp  405 (no server-initiated stream; clients handle this per spec)
//   DELETE /mcp  ends the session named by Mcp-Session-Id
//   GET    /health  {"ok":true,…} without a token
//   Sessions: `initialize` returns an Mcp-Session-Id; it identifies the client
//   (profile, cancellation scope) and is optional — a request without one, or
//   with one this process does not know (the terminal restarted), is served
//   statelessly instead of failing, so a client never has to reconnect.

#include "mcp/server/McpClientProfile.h"
#include "mcp/server/McpHttpServer_RunTokens.h"
#include "mcp/server/McpServerTypes.h"
#include "mcp/server/McpWire.h"

#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

#include <atomic>
#include <functional>
#include <memory>

class QTcpServer;
class QTcpSocket;
class QTimer;

namespace fincept::mcp::server {

using CancelFlag = std::shared_ptr<std::atomic<bool>>;

// ── Catalogue ────────────────────────────────────────────────────────────────

struct ToolMeta {
    QString internal;          ///< registry name
    QString advertised;        ///< name in tools/list
    QString category;
    int timeout_ms = 30000;    ///< the tool's own budget (the engine adds a grace on top)
    QJsonObject input_schema;  ///< as declared (before any client dialect)
    bool has_output_schema = false; ///< results must then keep structuredContent (MCP spec)
};

/// What tools/list advertises, and how a called name resolves. Immutable once
/// built; shared by pointer across threads.
struct Catalogue {
    quint64 generation = 0;
    QJsonArray entries;                // tools/list entries, catalogue order (Standard dialect)
    QHash<QString, ToolMeta> tools;    // by advertised name
    QHash<QString, QString> internal_to_advertised;
    /// Advertised name first, then the internal name. nullptr when unknown.
    const ToolMeta* resolve(const QString& called) const;
};
using CataloguePtr = std::shared_ptr<const Catalogue>;

// ── Backend ──────────────────────────────────────────────────────────────────

/// How a request authenticated.
struct AuthInfo {
    bool ok = false;
    bool run_token = false;        ///< a run-scoped token (in-app harness call)
    RunTokenPolicy policy;         ///< run_token only
    QByteArray fingerprint;        ///< short hash of the token: binds a session to its credential
};

struct CallRequest {
    quint64 call_id = 0;
    QString called_name;           ///< as the client called it
    ToolMeta tool;                 ///< resolved
    QJsonObject args;              ///< after the client dialect was undone
    QString client_label;          ///< who is asking ("claude-code", "codex · terminal AI")
    QString client_id;             ///< stable: the profile's actor_id ("claude_code", "codex", …, "external")
    QString run_id;                ///< the run-scoped token's run id; empty for the long-lived token
    profiles::ClientKind kind = profiles::ClientKind::Generic;
    bool paper_only = false;
    int result_budget_bytes = 0;   ///< 0 = backend default
    qint64 deadline_ms = 0;        ///< wall-clock ms since epoch: the engine answers by then whatever happens
    CancelFlag cancel;             ///< never null
    /// Thread-safe; may be called from any thread while the call runs.
    /// progress in [0, total] (total <= 0 = unknown), message short.
    std::function<void(double progress, double total, const QString& message)> progress;
};

struct CallOutcome {
    bool rpc_error = false; ///< `payload` is a JSON-RPC error body {code, message}
    QJsonObject payload;    ///< otherwise an MCP CallToolResult
    bool success = false;
    bool denied = false;
    QString summary;        ///< activity log: short, never arguments or results
};

using CallDone = std::function<void(CallOutcome)>;

class McpServerBackend {
  public:
    virtual ~McpServerBackend() = default;
    /// Bearer token check (constant time). Engine thread; must not block.
    virtual AuthInfo authenticate(const QByteArray& bearer) const = 0;
    /// Thread-safe; cheap after the first build.
    virtual CataloguePtr catalogue() = 0;
    /// serverInfo of `initialize`: {"name","title","version","edition"}. "edition"
    /// is also reported by GET /health, unauthenticated, so a client can check
    /// which Fincept edition listens on a port before it sends a token.
    virtual QJsonObject server_info() const = 0;
    virtual QString instructions() const = 0;
    /// prompts/list, prompts/get, resources/list, resources/read,
    /// resources/templates/list. Return false for a method it does not serve.
    virtual bool content_request(const QString& method, const QJsonObject& params, QJsonObject* result,
                                 int* error_code, QString* error_message) = 0;
    /// Start a call. MUST NOT block the calling (engine) thread. `done` is
    /// thread-safe and may be called once from any thread.
    virtual void call_tool(const CallRequest& request, CallDone done) = 0;
};

// ── Engine ───────────────────────────────────────────────────────────────────

struct EngineLimits {
    int max_connections = 64;
    qint64 idle_timeout_ms = 60 * 1000;      ///< a quiet keep-alive connection is closed
    qint64 request_timeout_ms = 120 * 1000;  ///< a request must arrive completely within this
    int keep_alive_hint_secs = 55;
    int max_sessions = 512;
    qint64 session_idle_ms = 6LL * 3600 * 1000; ///< a session unused this long is forgotten
    int max_inflight = 48;                   ///< running tools/call, all clients together
    int max_inflight_per_client = 12;        ///< per session (or per client when stateless)
    qint64 deadline_grace_ms = 60 * 1000;    ///< on top of the tool's budget (approval prompts)
    qint64 deadline_floor_ms = 5 * 1000;
    qint64 deadline_ceiling_ms = 30LL * 60 * 1000;
    qint64 sse_upgrade_ms = 8 * 1000;        ///< a call still running this long streams its answer
    qint64 sse_heartbeat_ms = 15 * 1000;
    qint64 write_pause_bytes = 4 * 1024 * 1024; ///< stop reading pipelined requests above this backlog
    int tick_ms = 250;
    int sweep_interval_ms = 5000;            ///< idle connections / expired sessions
    int tools_page_size = 100;
    /// Ports never bound (another program's listener — the other Fincept
    /// edition's): skipped by the fallback scan, refused as the first port, and
    /// an OS-assigned port that lands on one is released and drawn again.
    QList<quint16> forbidden_ports;
};

class McpServerEngine : public QObject {
    Q_OBJECT
  public:
    explicit McpServerEngine(std::shared_ptr<McpServerBackend> backend, EngineLimits limits = {},
                             QObject* parent = nullptr);
    ~McpServerEngine() override;

    /// Engine thread. Bind 127.0.0.1:first_port, else the next ports up to
    /// `fallback_range` (0 = only that port; port 0 = OS-assigned).
    bool listen(quint16 first_port, int fallback_range, QString* error);
    /// Engine thread. Answer every call still running with a clean error, close
    /// every connection and the listener. Idempotent.
    void shutdown(const QString& reason);

    /// Any thread.
    quint16 port() const { return port_.load(); }
    int connection_count() const { return connections_.load(); }
    int session_count() const { return sessions_count_.load(); }
    int inflight_count() const { return inflight_.load(); }

    /// Result budget for new calls, in bytes (any thread).
    void set_result_budget_bytes(int bytes) { result_budget_.store(bytes); }

  signals:
    /// A tool call finished, was refused, or timed out (also for refusals that
    /// never reached the backend). Emitted on the engine thread.
    void call_finished(const fincept::mcp::ExternalCallRecord& record);
    /// A client completed `initialize`.
    void client_initialized(const QString& name, const QString& version, const QString& user_agent,
                            const QString& profile_id, const QString& protocol);

  private:
    struct Conn;
    struct Pending;
    struct LiveCall;
    struct Session;
    struct ClientCtx;
    struct Link; // completion mailbox shared with backend threads

    void on_new_connection();
    void on_ready_read(QTcpSocket* sock);
    void on_disconnected(QTcpSocket* sock, quint64 conn_id);
    void on_bytes_written(QTcpSocket* sock);
    void process(QTcpSocket* sock);
    bool check_head(QTcpSocket* sock, Conn& c);
    void handle_request(QTcpSocket* sock, Conn& c);
    void handle_post(QTcpSocket* sock, Conn& c);
    void handle_delete(QTcpSocket* sock, Conn& c);
    void respond(QTcpSocket* sock, int status, const QByteArray& content_type, const QByteArray& body,
                 const wire::HeaderList& extra = {}, bool force_close = false);
    void respond_json(QTcpSocket* sock, int status, const QJsonObject& body, const wire::HeaderList& extra = {},
                      bool force_close = false);
    void fail_and_close(QTcpSocket* sock, int status, const QString& message);
    void write_raw(QTcpSocket* sock, const QByteArray& bytes);
    void finish_connection_request(QTcpSocket* sock, bool close_after);

    // JSON-RPC
    void handle_message(const QJsonValue& message, Pending& p, Conn& conn, ClientCtx& ctx);
    QJsonObject handle_initialize(const QJsonValue& id, const QJsonObject& params, Conn& conn, ClientCtx& ctx,
                                  QByteArray* new_session_id);
    QJsonObject handle_tools_list(const QJsonValue& id, const QJsonObject& params, const ClientCtx& ctx);
    void start_tool_call(const QJsonValue& id, const QJsonObject& params, Pending& p, ClientCtx& ctx);
    void cancel_request(const ClientCtx& ctx, const QJsonValue& request_id, const QString& reason);
    ClientCtx client_context(const Conn& conn, const wire::HttpRequest& req, const AuthInfo& auth);
    void record(const QString& client, const QString& tool, bool success, bool denied, qint64 ms,
                const QString& summary);

    // Calls
    void on_call_done(quint64 call_id, const CallOutcome& outcome);
    void on_call_progress(quint64 call_id, double progress, double total, const QString& message);
    void complete_slot(quint64 call_id, const QJsonObject& response, bool success, bool denied,
                       const QString& summary);
    void try_finish_pending(quint64 http_id);
    void write_pending_final(Pending& p);
    void open_sse(Pending& p);
    void send_sse(Pending& p, const QByteArray& event);
    void tick();
    void sweep();
    void drop_pending_for(QTcpSocket* sock);

    std::shared_ptr<McpServerBackend> backend_;
    EngineLimits limits_;
    std::shared_ptr<Link> link_;
    QTcpServer* server_ = nullptr;
    QTimer* ticker_ = nullptr;
    int ticks_ = 0;
    bool shut_down_ = false;

    QHash<QTcpSocket*, Conn*> conns_;
    QHash<quint64, Pending*> pending_;
    QHash<quint64, LiveCall*> calls_;
    QHash<QString, Session*> sessions_;
    QHash<QString, quint64> running_by_request_; // "<scope>|<id key>" → call id
    QHash<QString, int> inflight_by_client_;     // client scope → running calls
    QHash<QByteArray, QString> client_by_agent_; // User-Agent → clientInfo.name
    QHash<QString, profiles::ClientKind> cached_kinds_;
    struct ShapedList {
        quint64 generation = 0;
        QJsonArray entries;
    };
    QHash<int, ShapedList> shaped_lists_;        // ClientKind → shaped catalogue

    quint64 next_conn_id_ = 1;
    quint64 next_http_id_ = 1;
    quint64 next_call_id_ = 1;

    std::atomic<quint16> port_{0};
    std::atomic<int> connections_{0};
    std::atomic<int> sessions_count_{0};
    std::atomic<int> inflight_{0};
    std::atomic<int> result_budget_{0};
};

} // namespace fincept::mcp::server
