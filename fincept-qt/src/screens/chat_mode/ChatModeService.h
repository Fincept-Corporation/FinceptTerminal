#pragma once
#include "screens/chat_mode/ChatModeTypes.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QSet>
#include <QVector>

#include <functional>

namespace fincept::chat_mode {

/// Singleton service for the api.fincept.in/chat/* REST calls.
/// All callbacks are delivered on the Qt event loop (main thread).
///
/// The agent stream (/chat/agent/stream and …/resume) is not here: every turn is
/// a run of agent_tasks::TaskRunService, streamed by its own
/// agent_tasks::CloudAgentClient (one reply per run, so several runs and windows
/// never share — or abort — one connection). This class only lends it the shared
/// request / error / auth helpers below.
class ChatModeService : public QObject {
    Q_OBJECT
  public:
    static ChatModeService& instance();

    // ── Session CRUD ──────────────────────────────────────────────────────────
    using SessionCallback = std::function<void(bool ok, ChatSession session, QString error)>;
    using SessionsCallback = std::function<void(bool ok, QVector<ChatSession> sessions, QString error)>;
    using VoidCallback = std::function<void(bool ok, QString error)>;

    using SessionsPageCallback = std::function<void(bool ok, ChatSessionsPage page, QString error)>;
    /// GET /chat/sessions/{uuid}: the conversation and all its messages (oldest
    /// first). `http_status` is the reply's status (404: the conversation is gone).
    using SessionDetailCallback = std::function<void(bool ok, ChatSession session, QVector<ChatMessage> messages,
                                                     QString error, int http_status)>;

    /// GET /chat/sessions pages with `offset` + `has_more` on newer servers; older
    /// ones take only `limit` (newest first, default 50), and the sidebar then
    /// pages by asking for a larger `limit`.
    static constexpr int kSessionPageSize = 50;

    void create_session(const QString& title, SessionCallback cb);
    /// The `limit` most recently updated sessions.
    void list_sessions(int limit, SessionsCallback cb);
    /// `limit` sessions starting at `offset` (newest first). `offset` is sent only
    /// when > 0, so the first page is the same request older servers understand.
    void list_sessions_page(int limit, int offset, SessionsPageCallback cb);
    /// The conversation with its messages (transcript load, and the run engine's
    /// recovery of an answer the server stored after the stream was lost).
    void load_session(const QString& uuid, SessionDetailCallback cb);
    void delete_session(const QString& uuid, VoidCallback cb);
    void rename_session(const QString& uuid, const QString& title, VoidCallback cb);
    void activate_session(const QString& uuid, VoidCallback cb);

    // ── Session utilities ────────────────────────────────────────────────────
    using StatsCallback = std::function<void(bool ok, ChatStats stats, QString error)>;
    using SearchCallback = std::function<void(bool ok, QVector<ChatMessage> results, QString error)>;

    void get_stats(StatsCallback cb);
    void search_messages(const QString& query, SearchCallback cb);
    void export_sessions(const QStringList& uuids, std::function<void(bool ok, QJsonArray data, QString error)> cb);

    // ── Optimize prompt ──────────────────────────────────────────────────────
    using OptimizeCallback = std::function<void(bool ok, OptimizedPrompt result, QString error)>;

    void optimize_prompt(const QString& prompt, const QString& mode, OptimizeCallback cb);

    // ── Agent memory ──────────────────────────────────────────────────────────
    using MemoriesCallback = std::function<void(bool ok, QVector<AgentMemory> memories, QString error)>;

    void list_memory(MemoriesCallback cb);
    void save_memory(const QString& key, const QString& value, const QString& memory_type, VoidCallback cb);
    void delete_memory(const QString& key, VoidCallback cb);
    void clear_all_memory(VoidCallback cb);

    // ── Agent investment profiles ─────────────────────────────────────────────
    using ProfilesCallback = std::function<void(bool ok, QVector<AgentProfile> profiles, QString error)>;

    /// GET /profile/profiles — the account's investment profiles, default first.
    void list_profiles(ProfilesCallback cb);

    // (The server's background-task routes — /chat/agent/tasks, their feedback and
    // activity stream — are not used any more: long tasks are runs of
    // agent_tasks::TaskRunService.)

    // ── MCP servers ───────────────────────────────────────────────────────────
    using McpServersCallback = std::function<void(bool ok, QVector<McpServer> servers, int total_tools, QString error)>;
    using McpServerCallback = std::function<void(bool ok, McpServer server, QString error)>;

    void list_mcp_servers(McpServersCallback cb);
    void add_mcp_server(const QString& name, const QJsonObject& config, McpServerCallback cb);
    void delete_mcp_server(const QString& server_name, VoidCallback cb);
    void refresh_mcp_servers(McpServersCallback cb);

    // ── Credits ───────────────────────────────────────────────────────────────
    using CreditsCallback = std::function<void(bool ok, double credits, QString error)>;
    void get_credits(CreditsCallback cb);
    /// A balance learned from the server (the `finish` event of an agent turn):
    /// reported to AuthManager (header chip) and emitted as credit_balance_changed().
    void report_credit_balance(double balance);
    /// Re-read the balance from /user/profile after `delay_ms` (the server settles
    /// a charge shortly after a turn) and report it as above.
    void refresh_credit_balance(int delay_ms = 0);

    // ── Terminal tool bridge ─────────────────────────────────────────────────
    /// `unavailable`: 503 terminal_bridge_unavailable (the server's bridge is down);
    /// `retry_after_s` > 0 when the reply said when to come back.
    using RegisterCallback =
        std::function<void(bool ok, ToolRegistration reg, QString error, bool unavailable, int retry_after_s)>;
    /// `retry_after_s` > 0 when a failed poll's reply said when to come back.
    using PendingCallsCallback =
        std::function<void(bool ok, PendingToolCalls pending, QString error, int retry_after_s)>;
    using ToolResultCallback = std::function<void(ToolResultDelivery delivery, QString error)>;

    void register_terminal_tools(const QJsonArray& tools, const QString& version, int tool_count, RegisterCallback cb);
    void poll_pending_calls(PendingCallsCallback cb, int limit = 10);
    void submit_tool_result(const QString& call_id, const QJsonObject& result, ToolResultCallback cb);

    // ── Trade proposals ───────────────────────────────────────────────────────
    /// `retry_after_s`: the server's Retry-After (409 proposal_in_progress sends 2), 0 = none.
    using ProposalCallback =
        std::function<void(ProposalOutcome outcome, ProposalDecision decision, QString message, int retry_after_s)>;
    /// POST /chat/agent/proposals/{id}: execute (approved) or discard a trade the
    /// agent proposed. No agent turn, no credits; a repeated decision returns the
    /// first outcome. `session_id` (optional) is where the server records the
    /// outcome when the proposal does not name its conversation.
    void decide_proposal(const QString& proposal_id, bool approved, const QString& session_id, ProposalCallback cb);

    using RawCallback = std::function<void(bool ok, QJsonDocument doc, QString error)>;

    /// Low-level GET for callers that need raw document access.
    void get(const QString& path, RawCallback cb);

    // ── Shared with agent_tasks::CloudAgentClient (the agent stream) ─────────
    /// A request to `path` with the current credentials (X-API-Key, X-Session-Token).
    QNetworkRequest build_request(const QString& path) const;
    /// The network manager for long-lived streams (kept apart from REST calls).
    QNetworkAccessManager* stream_network() const { return sse_nam_; }
    /// Shared 401/403 bookkeeping for every reply from the Fincept API (this class
    /// does not go through HttpClient): a 401 goes to AuthManager (drops a dead
    /// session token, re-validates the key), a 403 body to HttpClient's IP-block
    /// state (`*ip_blocked` tells whether it was one). Returns true when the request
    /// may be re-sent once: its session token was the only reason and is gone now.
    bool report_auth_failure(QNetworkReply* reply, int status, const QByteArray& raw_body, bool* ip_blocked);
    /// What the user is shown for a failed reply: the server's own explanation plus
    /// what to do about it (see the .cpp).
    static QString reply_error_text(int status, const QJsonObject& body, QNetworkReply::NetworkError net_err,
                                    bool ip_blocked, int retry_after_s = 0);
    /// Retry-After in seconds (0 = none), the X-Error-Code (lower case, empty when
    /// not sent), and whether a refusal means the server has no such route.
    static int reply_retry_after(const QNetworkReply* reply);
    static QString reply_error_code(const QNetworkReply* reply);
    static bool reply_route_missing(int status, const QJsonObject& body, const QString& code);
    static QString network_error_name(QNetworkReply::NetworkError err);

  signals:
    /// Credit error (HTTP 402) of a REST call. `message` is the server's explanation
    /// plus how to fix it (may be empty). (An agent turn's 402 is a run error.)
    void insufficient_credits(const QString& message);
    /// The account balance changed (report_credit_balance(), refresh_credit_balance()).
    void credit_balance_changed(double balance);

  private:
    explicit ChatModeService(QObject* parent = nullptr);

    /// Re-sends a request built afresh (current credentials) — used once, for a
    /// request the server refused only because its session token had expired.
    using Resend = std::function<QNetworkReply*()>;
    static constexpr int kDefaultTimeoutMs = 15000;

    /// What a reply says besides its body, for callers that must tell refusals apart
    /// or honour the server's back-off.
    struct ReplyInfo {
        int status = 0;        // HTTP status, 0 = no HTTP answer
        int retry_after_s = 0; // Retry-After, 0 = none
        /// X-Error-Code: the machine-readable reason of a refusal (newer servers;
        /// the body keeps error_code "HTTP_ERROR"). Empty on older servers.
        QString code;
        /// The server has no route for this request (older deployment).
        bool route_missing = false;
    };
    using StatusCallback = std::function<void(bool ok, QJsonDocument doc, QString error, ReplyInfo info)>;

    QString base_url() const;
    QString api_key() const;
    QString session_token() const;

    void post(const QString& path, const QJsonObject& body, RawCallback cb);
    void put(const QString& path, const QJsonObject& body, RawCallback cb);
    void del(const QString& path, RawCallback cb);
    /// POST with a longer watchdog than kDefaultTimeoutMs (LLM-backed calls).
    void post_slow(const QString& path, const QJsonObject& body, int timeout_ms, RawCallback cb);
    void get_status(const QString& path, StatusCallback cb);
    void post_status(const QString& path, const QJsonObject& body, int timeout_ms, StatusCallback cb);

    void handle_reply(QNetworkReply* reply, StatusCallback cb, Resend resend, int timeout_ms = kDefaultTimeoutMs);
    static StatusCallback adapt(RawCallback cb);

    QNetworkAccessManager* nam_ = nullptr;     // for regular JSON calls
    QNetworkAccessManager* sse_nam_ = nullptr; // long-lived streams (agent turns)
};

} // namespace fincept::chat_mode
