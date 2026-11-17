#pragma once
// McpHttpServer_p.h — PRIVATE implementation header of McpHttpServer.
//
// Shared by the translation units of the facade and included from nowhere else:
//   McpHttpServer.cpp        lifecycle (engine thread), settings, token, snippets, activity
//   McpHttpServer_Tools.cpp  the backend: catalogue, auth, call policy, execution, result shaping
//   McpHttpServer_Rpc.cpp    the backend's content: instructions, prompts, resources
// The protocol itself (HTTP, JSON-RPC, sessions, streaming) is McpServerEngine.
//
// Threading
//   McpHttpServer and Impl live on the GUI thread. The engine lives on its own
//   "mcp-http" thread. The backend is called from the engine thread and from
//   its pool threads, so everything it reads is in BackendShared (atomics and
//   mutex-guarded values), held by shared_ptr so a call still running at
//   shutdown never touches a destroyed Impl.

#include "mcp/McpTypes.h"
#include "mcp/server/McpHttpServer.h"
#include "mcp/server/McpServerEngine.h"

#include <QHash>
#include <QMutex>
#include <QString>

#include <atomic>
#include <memory>

class QThread;
class QThreadPool;
class QTimer;

namespace fincept::mcp {

namespace mcp_http_detail {

inline constexpr const char* kTag = "McpHttpServer";

// Persisted settings — AppConfig (QSettings), the same store that holds
// `mcp/allow_destructive_tools`. The bearer token lives in SecureStorage.
inline constexpr const char* kKeyEnabledAtStartup = "mcp_server/enabled_at_startup";
inline constexpr const char* kKeyAllowUiControl = "mcp_server/allow_ui_control";
inline constexpr const char* kKeyAllowScripts = "mcp_server/allow_scripts";
inline constexpr const char* kKeyResultBudgetKb = "mcp_server/result_budget_kb";
inline constexpr const char* kKeyPort = "mcp_server/port";
inline constexpr const char* kTokenStorageKey = "mcp_server/token";

inline constexpr quint16 kMinUserPort = 1024;
// Backend pool. Threads only BLOCK for an approval prompt (≤ 50 s); a tool call
// itself is started on a pool thread and completes through a continuation, so
// no thread waits for a tool. Python work is capped separately by PythonRunner.
inline constexpr int kPoolThreads = 8;
inline constexpr int kMaxAdvertisedNameLen = 50; // "mcp__fincept__" + 50 ≤ the 64-char API limit
inline constexpr int kAliasPrefixLen = 41;       // 41 + '_' + 8 hex = 50
inline constexpr int kDefaultResultBudgetKb = 32;
inline constexpr int kMinResultBudgetKb = 8;
inline constexpr int kMaxResultBudgetKb = 256;
inline constexpr int kMaxErrorTextChars = 8000;
inline constexpr int kActivityCapacity = 200;
inline constexpr int kSummaryMaxChars = 120;
inline constexpr int kMaxTrackedClients = 256;

/// Whitespace-collapsed, length-bounded text for logs / the activity summary.
QString clip_text(const QString& text, int max_chars);
/// ≤ 50 chars matching ^[a-zA-Z0-9_-]+$. Valid internal names pass through
/// unchanged; anything else becomes a sanitised prefix + '_' + 8 hex chars of
/// SHA-256(internal name), so it is stable across restarts.
QString make_advertised_name(const QString& internal_name);
/// 32 bytes from the system CSPRNG, base64url without padding (43 chars).
QString generate_token();
/// The run-scoped tokens (one table for the process; thread-safe on its own).
McpRunTokenTable& run_token_table();

/// Everything the backend reads, from any thread.
struct BackendShared {
    std::atomic<bool> accepting{false}; // false while stopped: queued calls bail out
    std::atomic<bool> allow_ui_control{true};
    std::atomic<bool> allow_scripts{true};
    std::atomic<int> result_budget_bytes{kDefaultResultBudgetKb * 1024};

    /// The long-lived bearer token the server accepts right now (the stored
    /// one, or the self-test's). Set on the GUI thread at start / regenerate.
    mutable QMutex auth_mutex;
    QString auth_token;
    QByteArray auth_fingerprint;

    /// Facts read on the GUI thread at start(), served from any thread.
    mutable QMutex facts_mutex;
    QString app_version;
    QString screens_text;

    QMutex catalogue_mutex;
    server::CataloguePtr catalogue;

    /// Created on first use; owned (see ~McpHttpServer).
    QMutex pool_mutex;
    QThreadPool* pool = nullptr;
    QThreadPool* ensure_pool();
};

/// The catalogue for the provider's current generation, rebuilt when stale.
/// Thread-safe; the rebuild runs outside the lock.
server::CataloguePtr current_catalogue(BackendShared& shared);

/// Instructions sent once per session in the `initialize` result.
QString server_instructions();
/// prompts/* and resources/* (McpHttpServer_Rpc.cpp).
bool serve_content(const BackendShared& shared, const QString& method, const QJsonObject& params, QJsonObject* result,
                   int* error_code, QString* error_message);
/// The screens list (GUI thread: reads ComponentCatalog).
QString build_screens_text();

/// The terminal's backend for McpServerEngine (McpHttpServer_Tools.cpp).
std::shared_ptr<server::McpServerBackend> make_terminal_backend(std::shared_ptr<BackendShared> shared);

} // namespace mcp_http_detail

struct McpHttpServer::Impl {
    explicit Impl(McpHttpServer* owner) : q(owner) {}

    McpHttpServer* q = nullptr;
    std::shared_ptr<mcp_http_detail::BackendShared> shared = std::make_shared<mcp_http_detail::BackendShared>();
    std::shared_ptr<server::McpServerBackend> backend;
    QThread* thread = nullptr;             // "mcp-http": the engine's thread
    server::McpServerEngine* engine = nullptr; // lives on `thread`; created by start(), deleted there by stop()
    quint16 bound_port = 0;
    QString last_error;

    // Settings cache (persisted values; the permissions live in `shared`).
    bool enabled_at_startup = false;
    bool persisted_allow_ui_control = true;
    bool persisted_allow_scripts = true;
    int persisted_result_budget_kb = mcp_http_detail::kDefaultResultBudgetKb;
    quint16 configured_port = McpHttpServer::kDefaultPort;

    // The persisted token, as read from SecureStorage (guarded by token_mutex).
    mutable QMutex token_mutex;
    mutable QString token;
    /// Why the stored token could not be read; "" = readable. While set, no
    /// token exists in memory and none is minted (McpTokenPolicy.h).
    mutable QString token_error;

    // start() wanted while the token could not be read: retried with back-off
    // until it starts or stop() is called (GUI thread).
    QTimer* token_retry = nullptr;
    int token_retry_attempt = 0;
    bool start_pending = false;

    // Self-test overrides (never persisted).
    bool test_mode = false;
    quint16 test_port = 0;
    QString test_token;

    QHash<QString, ExternalClientInfo> distinct_clients; // "name|user-agent" → info, since start()

    mutable QMutex activity_mutex;
    QList<ExternalCallRecord> activity; // newest first
    ExternalServerStats counters;       // guarded by activity_mutex

    QString effective_token() const;
    /// The bearer token: the one in memory, else the stored one, else (only when
    /// the store was READ and holds none) a newly minted one. Empty when the store
    /// could not be read (token_error says why) — never a fresh token then.
    QString load_or_create_token() const;
    QString effective_url() const;
    void publish_auth_token(const QString& token);
    void record_activity(const ExternalCallRecord& record);
    void note_client(const QString& name, const QString& version, const QString& user_agent);
    void note_client_call(const QString& client);
    /// Stop and delete the engine and its thread (GUI thread; bounded wait).
    void teardown_engine(const QString& reason);
};

} // namespace fincept::mcp
