#pragma once
// McpHttpServer.h — Fincept Terminal as an MCP server for EXTERNAL agents.
//
// Exposes the terminal's internal tool catalogue over the Model Context Protocol
// (Streamable HTTP transport, JSON-RPC 2.0) so coding agents running on the same
// machine — Claude Code, OpenAI Codex CLI, Cursor, Claude Desktop (through the
// stdio bridge script) — can read live market data, research, portfolios and
// run terminal workflows.
//
//   POST http://127.0.0.1:<port>/mcp   — JSON-RPC request / notification / batch
//   GET  /mcp                          — 405 (no server-initiated stream)
//   GET  /health                       — {"ok":true,"name":"fincept-terminal","version":...}
//
// Security model
//   * Binds 127.0.0.1 only. Every request passes auth/LoopbackGuard.h (Host must
//     be our loopback authority; browser cross-site requests are refused), and
//     an Origin header that is not loopback is refused (DNS-rebinding defence).
//   * `Authorization: Bearer <token>` is required on every /mcp request. The
//     token is stable across restarts (SecureStorage key "mcp_server/token") so
//     client configs keep working; regenerate_token() revokes every client.
//   * Tools that change state (is_destructive) are refused unless the user has
//     enabled allow_actions(). Live-trading tools and tools whose auth level is
//     ExplicitConfirm (any level >= Verified) additionally require a per-call
//     approval in the terminal UI (ToolApprovalDialog) regardless of
//     allow_actions(); the approved call runs inside a ScopedApprovedCall.
//   * tool_invoke is held to the policy of the tool it invokes, and may only
//     reach built-in tools — the user's other MCP servers are never re-exported.
//
// Threading: the listener and all socket I/O live on the GUI thread. Tool calls
// run on a dedicated QThreadPool (never QThreadPool::globalInstance(), which the
// LLM fan-out and the agent bridge already block on), and replies are posted back
// to the GUI thread. Long tools use the existing job protocol (allow_defer=true):
// a call that overruns the grace window returns a {job_id,status:"running"}
// receipt the client polls with job_status / job_result.

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

namespace fincept::mcp {

/// One external tool call, for the Connect tab's activity log. Never holds
/// arguments or results verbatim (they may contain account data) — only a short
/// summary.
struct ExternalCallRecord {
    QDateTime at;
    QString client;          // clientInfo.name from `initialize` ("claude-code", "codex-mcp-client", …) or "unknown"
    QString tool;            // bare tool name as the client called it
    bool success = false;
    bool denied = false;     // refused by policy or by the user
    qint64 duration_ms = 0;
    QString summary;         // short status / error text, never secrets
};

/// Which tools `tools/list` advertises.
///   Essentials — meta tools (tool_list, tool_describe, job_*, result_fetch),
///                the generic `tool_invoke` dispatcher, and a curated set of
///                high-value read tools (see EssentialTools.h). Small enough for
///                any client's context; everything else stays reachable through
///                tool_list + tool_invoke.
///   Full       — every enabled internal tool (≈900). Only sensible for clients
///                that defer/search large tool lists (e.g. Claude Code).
/// The listing is a context-size choice, not a security boundary: tools/call
/// accepts any enabled internal tool and applies the same policy either way.
/// Clients pick up a toolset change when they reconnect (listChanged=false).
enum class ExternalToolset { Essentials, Full };

class McpHttpServer : public QObject {
    Q_OBJECT
  public:
    static McpHttpServer& instance();

    static constexpr quint16 kDefaultPort = 41720;
    static constexpr int kPortFallbackRange = 20; // tries port..port+20 when busy

    /// Bind and start serving. Idempotent. Returns true when listening.
    bool start();
    /// Stop listening and drop every live connection.
    void stop();
    /// Start only if the user enabled "start with the terminal". Called once
    /// from MCP initialisation after the tool catalogue is registered.
    void start_if_enabled();

    bool is_running() const;
    quint16 port() const;             // actually bound port, 0 when stopped
    QString url() const;              // "http://127.0.0.1:<port>/mcp", empty when stopped
    QString last_error() const;       // last bind / runtime error, for the UI

    /// Stable bearer token (created on first use, persisted in SecureStorage).
    QString token() const;
    /// Mint a new token — every configured client must be updated.
    void regenerate_token();

    // ── User settings (persisted) ───────────────────────────────────────────
    bool enabled_at_startup() const;
    void set_enabled_at_startup(bool on);
    bool allow_actions() const;       // destructive tools permitted (approval rules still apply)
    void set_allow_actions(bool on);
    ExternalToolset toolset() const;
    void set_toolset(ExternalToolset t);
    quint16 configured_port() const;
    void set_configured_port(quint16 port); // takes effect on next start()

    /// Most recent external calls, newest first (bounded ring, max 200).
    QList<ExternalCallRecord> recent_activity() const;
    /// Number of distinct clients that called `initialize` since start().
    int connected_client_count() const;

    // ── Connection snippets for the Connect tab (token inlined unless noted) ──
    /// claude mcp add --transport http fincept <url> -H "Authorization: Bearer <token>" -s user
    QString claude_code_add_command() const;
    /// codex mcp add fincept --env FINCEPT_MCP_TOKEN=<token> -- <python> <bridge> --url <url>
    QString codex_add_command() const;
    /// [mcp_servers.fincept] url = "<url>"  bearer_token_env_var = "FINCEPT_MCP_TOKEN"
    QString codex_config_toml() const;
    /// {"mcpServers":{"fincept":{"type":"http","url":"<url>","headers":{"Authorization":"Bearer <token>"}}}}
    QString generic_http_json() const;
    /// Claude Desktop style stdio entry: {"mcpServers":{"fincept":{"command":<python>,"args":[<bridge>,"--url",<url>],"env":{"FINCEPT_MCP_TOKEN":<token>}}}}
    QString stdio_bridge_json() const;
    /// Interpreter used for the stdio bridge (stdlib only; prefers the app's venv-numpy2 python).
    QString python_for_bridge() const;
    /// Absolute path of scripts/mcp/fincept_mcp_stdio.py.
    QString bridge_script_path() const;

    // ── For in-app agent runs (token passed via env FINCEPT_MCP_TOKEN) ────────
    /// Claude Code --mcp-config object; header uses ${FINCEPT_MCP_TOKEN} expansion.
    QJsonObject claude_mcp_config_env() const;
    /// Codex CLI arguments: {"-c", "mcp_servers.fincept.url=\"<url>\"", "-c",
    /// "mcp_servers.fincept.bearer_token_env_var=\"FINCEPT_MCP_TOKEN\""}.
    QStringList codex_config_overrides() const;
    static constexpr const char* kTokenEnvVar = "FINCEPT_MCP_TOKEN";

    McpHttpServer(const McpHttpServer&) = delete;
    McpHttpServer& operator=(const McpHttpServer&) = delete;

  signals:
    /// Started / stopped / bound port / last_error / token regenerated / any
    /// persisted setting changed / a new distinct client called `initialize`
    /// (connected_client_count() grew). Always emitted on the GUI thread.
    void status_changed();
    /// A tool call finished (success, failure or denial). Emitted on the GUI
    /// thread, newest-first ordering matches recent_activity().
    void activity(const fincept::mcp::ExternalCallRecord& record);

  private:
    explicit McpHttpServer(QObject* parent = nullptr);
    ~McpHttpServer() override;

    // ── Headless self-test hooks (McpServerSelfTest.cpp) ────────────────────
    friend int run_mcp_server_selftest();
    /// Make the next start() bind `port` (0 = OS-assigned, no fallback range)
    /// and use `token` / `allow_actions` — all in memory, nothing is read from
    /// or written to persisted settings or SecureStorage.
    void set_selftest_overrides(quint16 port, const QString& token, bool allow_actions);
    void clear_selftest_overrides();
    /// The name tools/list advertises for an internal tool name
    /// (≤ 50 chars, ^[a-zA-Z0-9_-]+$; stable across restarts).
    static QString advertised_name(const QString& internal_name);

    struct Impl;
    Impl* d_ = nullptr; // owned; implementation details live in McpHttpServer.cpp
};

} // namespace fincept::mcp

Q_DECLARE_METATYPE(fincept::mcp::ExternalCallRecord)
