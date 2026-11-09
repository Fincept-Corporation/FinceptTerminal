#pragma once
// McpHttpServer.h — Fincept Terminal as an MCP server for EXTERNAL agents.
//
// Exposes the terminal's ten core tools (src/mcp/core/CoreTools.h) over the
// Model Context Protocol (Streamable HTTP transport, JSON-RPC 2.0) so agents on
// the same machine — Claude Code, OpenAI Codex CLI, Gemini CLI, Cursor, VS Code,
// Claude Desktop (through the stdio bridge script) — can operate the running
// terminal the way a person does: see what is open, open screens, read tables,
// charts and texts, work the controls, and run the bundled analytics scripts.
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
//   * Two user permissions: allow_ui_control() — operate the UI (ui_act,
//     ui_invoke, ui_open, ui_workspace); off = look-only — and allow_scripts()
//     — run the bundled Python scripts. Both default ON: risk is judged per
//     action, not per tool. ui_act / ui_invoke / python ask the user
//     (ToolApprovalDialog) before orders, money moves, spends, deletes and
//     sensitive scripts, whatever the permissions say; secret fields are never
//     read or typed. Tools with auth level >= Verified also need a per-call
//     approval (ScopedApprovedCall).
//   * Only the terminal's own tools are served — the user's other MCP servers
//     are never re-exported.
//
// Threading: the listener and all socket I/O live on the GUI thread. Tool calls
// run on a dedicated QThreadPool (never QThreadPool::globalInstance(), which the
// LLM fan-out and the agent bridge already block on); the UI tools hop to the
// GUI thread themselves and never block it. Replies are posted back to the GUI
// thread. Long Python scripts answer with a job id that `python(action=status)`
// long-polls.

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

/// One client that said hello since start(), for the settings page.
struct ExternalClientInfo {
    QString name;       // clientInfo.name ("claude-code", "codex-mcp-client", …)
    QString user_agent; // HTTP User-Agent, trimmed
    QDateTime first_seen;
    QDateTime last_seen;
    int calls = 0;
};

/// Counters since start(), for the settings page.
struct ExternalServerStats {
    QDateTime started_at;
    quint64 calls = 0;
    quint64 ok = 0;
    quint64 failed = 0;
    quint64 denied = 0;
};

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
    /// Operate the UI (ui_act, ui_invoke, ui_open, ui_workspace). Off = look-only.
    bool allow_ui_control() const;
    void set_allow_ui_control(bool on);
    /// Run the bundled Python scripts (python run / status / cancel).
    bool allow_scripts() const;
    void set_allow_scripts(bool on);
    /// Auto mode — every approval prompt is skipped for every agent (external
    /// clients, AI Chat, agent runs): orders, money moves, spends, deletes and
    /// sensitive scripts run without asking. Off by default. See
    /// ToolApprovalDialog::auto_approve(), which owns the setting.
    bool auto_approve() const;
    void set_auto_approve(bool on);
    /// Largest tool result sent inline, in KB; the rest is parked for ui_read.
    int result_budget_kb() const;
    void set_result_budget_kb(int kb);
    quint16 configured_port() const;
    void set_configured_port(quint16 port); // takes effect on next start()

    /// Most recent external calls, newest first (bounded ring, max 200).
    QList<ExternalCallRecord> recent_activity() const;
    /// Forget the activity log (the counters stay).
    void clear_activity();
    /// Number of distinct clients that called `initialize` since start().
    int connected_client_count() const;
    /// Those clients, most recently seen first.
    QList<ExternalClientInfo> clients() const;
    /// Counters since start().
    ExternalServerStats stats() const;

    // ── Connection snippets for the Connect tab (token inlined unless noted) ──
    /// claude mcp add --transport http fincept <url> -H "Authorization: Bearer <token>" -s user
    QString claude_code_add_command() const;
    /// codex mcp add fincept --env FINCEPT_MCP_TOKEN=<token> -- <python> <bridge> --url <url>
    QString codex_add_command() const;
    /// [mcp_servers.fincept] url = "<url>"  bearer_token_env_var = "FINCEPT_MCP_TOKEN"
    QString codex_config_toml() const;
    /// gemini mcp add --transport http fincept_terminal <url> -H "Authorization: Bearer <token>" -s user
    QString gemini_add_command() const;
    /// ~/.gemini/settings.json: {"mcpServers":{"fincept_terminal":{"httpUrl":…,"headers":{…},"timeout":…}}}
    QString gemini_settings_json() const;
    /// {"mcpServers":{"fincept":{"type":"http","url":"<url>","headers":{"Authorization":"Bearer <token>"}}}}
    QString generic_http_json() const;
    /// VS Code (Copilot) .vscode/mcp.json: {"servers":{"fincept_terminal":{"type":"http",…}}}
    QString vscode_json() const;
    /// Claude Desktop style stdio entry: {"mcpServers":{"fincept":{"command":<python>,"args":[<bridge>,"--url",<url>],"env":{"FINCEPT_MCP_TOKEN":<token>}}}}
    QString stdio_bridge_json() const;
    /// Interpreter used for the stdio bridge (stdlib only; prefers the app's venv-numpy2 python).
    QString python_for_bridge() const;
    /// Absolute path of scripts/fincept_mcp/fincept_mcp_stdio.py.
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
    /// and use `token` / the two permissions — all in memory, nothing is read
    /// from or written to persisted settings or SecureStorage.
    void set_selftest_overrides(quint16 port, const QString& token, bool allow_ui_control, bool allow_scripts);
    void clear_selftest_overrides();
    /// The name tools/list advertises for an internal tool name
    /// (≤ 50 chars, ^[a-zA-Z0-9_-]+$; stable across restarts).
    static QString advertised_name(const QString& internal_name);

    struct Impl;
    Impl* d_ = nullptr; // owned; implementation details live in McpHttpServer.cpp
};

} // namespace fincept::mcp

Q_DECLARE_METATYPE(fincept::mcp::ExternalCallRecord)
Q_DECLARE_METATYPE(fincept::mcp::ExternalClientInfo)
