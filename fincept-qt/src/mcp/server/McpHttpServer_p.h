#pragma once
// McpHttpServer_p.h — PRIVATE implementation header of McpHttpServer.
//
// Shared by the four translation units of the server and included from nowhere
// else:
//   McpHttpServer.cpp        lifecycle, settings, token, snippets, activity
//   McpHttpServer_Http.cpp   HTTP/1.1 framing, loopback/bearer checks, responses
//   McpHttpServer_Rpc.cpp    JSON-RPC dispatch on the GUI thread, worker hand-off
//   McpHttpServer_Tools.cpp  tool catalogue, call policy, result shaping (workers)
//
// Threading: everything in McpHttpServer::Impl is GUI-thread state except
// `shared` (WorkerShared), which pool threads hold by shared_ptr so a call still
// running at shutdown never touches a destroyed Impl.

#include "mcp/McpTypes.h"
#include "mcp/server/McpHttpServer.h"

#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QMutex>
#include <QPair>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTcpSocket>
#include <QVector>

#include <atomic>
#include <memory>

class QTcpServer;
class QThreadPool;
class QTimer;

namespace fincept::mcp {

namespace mcp_http_detail {

inline constexpr const char* kTag = "McpHttpServer";

// Persisted settings — AppConfig (QSettings), the same store that holds
// `mcp/allow_destructive_tools`. The bearer token lives in SecureStorage.
inline constexpr const char* kKeyEnabledAtStartup = "mcp_server/enabled_at_startup";
inline constexpr const char* kKeyAllowActions = "mcp_server/allow_actions";
inline constexpr const char* kKeyToolset = "mcp_server/toolset";
inline constexpr const char* kKeyPort = "mcp_server/port";
inline constexpr const char* kTokenStorageKey = "mcp_server/token";

// HTTP limits — everything a client controls is bounded.
inline constexpr qsizetype kMaxHeaderBytes = 64 * 1024;
inline constexpr qint64 kMaxBodyBytes = 8 * 1024 * 1024;
inline constexpr int kMaxConnections = 64;
inline constexpr qint64 kIdleTimeoutMs = 60 * 1000;
inline constexpr int kKeepAliveHintSecs = 55; // below the idle timeout, so clients retire sockets first
inline constexpr int kSweepIntervalMs = 5000;
inline constexpr quint16 kMinUserPort = 1024;

// MCP surface.
inline constexpr int kPoolThreads = 4;
inline constexpr int kToolsPageSize = 100;
inline constexpr int kMaxAdvertisedNameLen = 50; // "mcp__fincept__" + 50 ≤ the 64-char API limit
inline constexpr int kAliasPrefixLen = 41;       // 41 + '_' + 8 hex = 50
inline constexpr int kResultBudgetBytes = 24 * 1024;
inline constexpr int kMaxErrorTextChars = 8000;
inline constexpr int kActivityCapacity = 200;
inline constexpr int kSummaryMaxChars = 120;
inline constexpr int kClientNameMaxChars = 64;
inline constexpr int kMaxTrackedClients = 256;
inline constexpr const char* kLatestProtocol = "2025-06-18";
inline constexpr const char* kToolInvoke = "tool_invoke";
inline constexpr const char* kResultFetch = "result_fetch";
inline constexpr const char* kLiveTradingCategory = "live-trading";

// ── Small shared helpers (McpHttpServer_Tools.cpp) ───────────────────────────
qint64 now_ms();
QByteArray compact_json(const QJsonObject& object);
QByteArray compact_json(const QJsonArray& array);
/// Whitespace-collapsed, length-bounded text for logs / the activity summary.
QString clip_text(const QString& text, int max_chars);
/// ≤ 50 chars matching ^[a-zA-Z0-9_-]+$. Valid internal names pass through
/// unchanged; anything else becomes a sanitised prefix + '_' + 8 hex chars of
/// SHA-256(internal name), so it is stable across restarts.
QString make_advertised_name(const QString& internal_name);

// ── JSON-RPC envelopes (McpHttpServer_Rpc.cpp) ──────────────────────────────
QJsonObject rpc_result(const QJsonValue& id, const QJsonValue& result);
QJsonObject rpc_error_body(int code, const QString& message);
QJsonObject rpc_error(const QJsonValue& id, int code, const QString& message);

// ── Token (McpHttpServer.cpp) ────────────────────────────────────────────────
/// 32 bytes from the system CSPRNG, base64url without padding (43 chars).
QString generate_token();

// ── HTTP/1.1 connection state (McpHttpServer_Http.cpp; GUI thread only) ──────
struct Conn {
    quint64 id = 0;
    QByteArray buf; // bytes received but not yet consumed

    // The request being read / served.
    bool head_done = false;
    QByteArray head; // request line + header block incl. the blank line (for LoopbackGuard)
    QByteArray method;
    QByteArray target;
    QByteArray version;
    QHash<QByteArray, QByteArray> headers; // lower-case name → value
    qint64 content_length = 0;
    bool chunked = false;
    int chunk_state = 0; // 0 size line · 1 data · 2 CRLF after data · 3 trailers
    qint64 chunk_left = 0;
    QByteArray body;
    bool expect_continue = false;

    bool keep_alive = true;
    bool busy = false;    // a request is being served (tools/call may take a while)
    bool closing = false; // close initiated / peer gone — ignore further input
    qint64 last_activity_ms = 0;
    QString client_name; // clientInfo.name from an `initialize` on this connection
};

void reset_request(Conn& c);
bool parse_request_head(Conn& c, int* status, QString* why);
/// 1 = body complete, 0 = need more bytes, -1 = error (`*status` set).
int decode_chunked_body(Conn& c, int* status);
/// Path component of a request target: query/fragment and one trailing '/' stripped.
QByteArray request_path(const QByteArray& target);
const char* reason_phrase(int status);

// ── One POST whose replies include tools/call results still running ─────────
struct PendingHttp {
    QPointer<QTcpSocket> sock;
    bool batch = false;
    QVector<QJsonValue> responses; // one per request in the message, in order
    int outstanding = 0;
};

// ── Tool catalogue snapshot (McpHttpServer_Tools.cpp) ───────────────────────
struct CatalogTool {
    QString name;       // internal registry name
    QString advertised; // name in tools/list
    QString description;
    QString category;
    QJsonObject input_schema; // normalised: type=object, properties present
    bool destructive = false;
    AuthLevel auth = AuthLevel::None;
};

/// Live-trading tools and tools at AuthLevel >= Verified (ExplicitConfirm in
/// practice) need the user's per-call approval in ToolApprovalDialog.
bool needs_approval(const CatalogTool& tool);

struct Catalogue {
    quint64 generation = 0;                // McpProvider::generation() it was built from
    QHash<QString, CatalogTool> tools;     // enabled internal tools, by internal name
    QHash<QString, QString> by_advertised; // advertised name → internal name
    QJsonArray full_entries;               // tools/list entries, every tool, sorted
    QJsonArray essential_entries;          // tools/list entries, curated subset, curated order
    bool has_tool_invoke = false;
};
using CataloguePtr = std::shared_ptr<const Catalogue>;

/// Internal name for what a client called: advertised alias, else the internal
/// name itself. Empty when unknown or disabled.
QString resolve_tool_name(const Catalogue& catalogue, const QString& called);

// ── State shared with pool threads ───────────────────────────────────────────
struct WorkerShared {
    std::atomic<bool> alive{true};     // false once the server object is being destroyed
    std::atomic<bool> accepting{false}; // false while stopped: queued calls bail out
    std::atomic<bool> allow_actions{false};
    QMutex catalogue_mutex;
    CataloguePtr catalogue;
};

/// The catalogue for the provider's current generation, rebuilt when stale.
/// Thread-safe; the rebuild runs outside the lock.
CataloguePtr current_catalogue(WorkerShared& shared);

// ── tools/call ───────────────────────────────────────────────────────────────
struct CallJob {
    quint64 http_id = 0;
    int slot = 0;
    quint64 epoch = 0;
    QJsonValue rpc_id;
    QString called_name;
    QJsonObject args;
    QString client;
};

struct CallOutcome {
    bool rpc_error = false; // `payload` is a JSON-RPC error body {code, message}
    QJsonObject payload;    // otherwise an MCP CallToolResult
    bool success = false;
    bool denied = false;
    QString summary; // activity log: short, never arguments or results
};

/// Policy → (approval) → McpService::execute_tool → shaping. Runs on a pool thread.
CallOutcome run_tool_call(WorkerShared& shared, const CallJob& job);

/// MCP CallToolResult carrying only an error message.
QJsonObject error_call_result(const QString& message);

} // namespace mcp_http_detail

struct McpHttpServer::Impl {
    explicit Impl(McpHttpServer* owner) : q(owner) {}

    McpHttpServer* q = nullptr;
    std::shared_ptr<mcp_http_detail::WorkerShared> shared = std::make_shared<mcp_http_detail::WorkerShared>();

    QThreadPool* pool = nullptr; // dedicated; never QThreadPool::globalInstance()
    QTcpServer* server = nullptr;
    QTimer* sweeper = nullptr;
    QHash<QTcpSocket*, mcp_http_detail::Conn> conns;
    QHash<quint64, mcp_http_detail::PendingHttp> pending;
    quint64 next_conn_id = 1;
    quint64 next_http_id = 1;
    quint64 epoch = 0; // bumped by every start(); replies from an older epoch are dropped
    QString last_error;

    // Settings cache (persisted values; allow_actions lives in `shared`).
    bool enabled_at_startup = false;
    bool persisted_allow_actions = false;
    ExternalToolset toolset = ExternalToolset::Essentials;
    quint16 configured_port = McpHttpServer::kDefaultPort;

    mutable QMutex token_mutex;
    mutable QString token;

    // Self-test overrides (never persisted).
    bool test_mode = false;
    quint16 test_port = 0;
    QString test_token;

    // Client identification — the server is stateless, so remember who said hello.
    QHash<QByteArray, QString> client_by_agent; // User-Agent → clientInfo.name
    QHash<QString, QString> distinct_clients;   // "name|user-agent" → name, since start()

    mutable QMutex activity_mutex;
    QList<ExternalCallRecord> activity; // newest first

    // McpHttpServer.cpp
    QString effective_token() const;
    QString load_or_create_token() const;
    QString effective_url() const;
    void ensure_pool();
    void record_activity(const ExternalCallRecord& record);

    // McpHttpServer_Http.cpp
    void on_new_connection();
    void on_ready_read(QTcpSocket* sock);
    void on_disconnected(QTcpSocket* sock, quint64 conn_id);
    void process(QTcpSocket* sock);
    bool reject_early(QTcpSocket* sock);
    void handle_request(QTcpSocket* sock);
    void respond(QTcpSocket* sock, int status, const QByteArray& body,
                 const QList<QPair<QByteArray, QByteArray>>& extra_headers = {}, bool force_close = false);
    void respond_json(QTcpSocket* sock, int status, const QJsonObject& body,
                      const QList<QPair<QByteArray, QByteArray>>& extra_headers = {}, bool force_close = false);
    void fail_and_close(QTcpSocket* sock, int status, const QString& message);
    void sweep();

    // McpHttpServer_Rpc.cpp
    void handle_mcp_post(QTcpSocket* sock);
    void handle_message(const QJsonValue& message, quint64 http_id, mcp_http_detail::PendingHttp& ph,
                        mcp_http_detail::Conn& conn, const QByteArray& user_agent);
    QJsonObject handle_method(const QString& method, const QJsonValue& id, const QJsonObject& params,
                              mcp_http_detail::Conn& conn, const QByteArray& user_agent);
    QJsonObject handle_initialize(const QJsonValue& id, const QJsonObject& params, mcp_http_detail::Conn& conn,
                                  const QByteArray& user_agent);
    QJsonObject handle_tools_list(const QJsonValue& id, const QJsonObject& params);
    void write_rpc_response(QTcpSocket* sock, const mcp_http_detail::PendingHttp& ph);
    void start_call(const mcp_http_detail::CallJob& job);
    void on_call_finished(const mcp_http_detail::CallJob& job, const mcp_http_detail::CallOutcome& outcome,
                          qint64 duration_ms);
    QString client_for(const mcp_http_detail::Conn& conn, const QByteArray& user_agent) const;
};

} // namespace fincept::mcp
