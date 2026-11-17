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
//                                        (a long tools/call streams its answer as SSE)
//   GET  /mcp                          — 405 (no server-initiated stream)
//   DELETE /mcp                        — ends the Mcp-Session-Id session
//   GET  /health                       — {"ok":true,"name":"fincept-terminal","version":...}
// The protocol engine is McpServerEngine (sessions, client profiles,
// cancellation, deadlines, concurrency limits); this class is its facade.
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
// Threading: the listener, every socket and the protocol engine live on the
// server's own "mcp-http" thread — the GUI thread never parses, serialises or
// waits for a client, and initialize / tools/list / ping answer even while the
// GUI is busy. A tool call is started on a dedicated QThreadPool (never
// QThreadPool::globalInstance(), which the LLM fan-out and the agent bridge
// already block on) and completes through a continuation, so no thread waits
// for a tool; the UI tools hop to the GUI thread themselves and never block it.
// Every call is answered by its deadline (the tool's budget plus a grace) even
// if the tool never resolves. Long Python scripts answer with a job id that
// `python(action=status)` long-polls.

#include "core/config/EditionNames.h"
#include "mcp/server/McpHttpServer_RunTokens.h"
#include "mcp/server/McpServerTypes.h"

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <optional>

namespace fincept::mcp {

class McpHttpServer : public QObject {
    Q_OBJECT
  public:
    static McpHttpServer& instance();

    static constexpr quint16 kDefaultPort = edition::kMcpDefaultPort;          // 41720
    static constexpr int kPortFallbackRange = edition::kMcpPortFallbackRange; // tries port..port+19 when busy
    // The private build's ports (47821, 47999) are never bound: refused as a
    // configured port and skipped by the fallback scan (edition::kPrivateMcpPorts).

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

    // ── Run-scoped tokens (in-app harness calls; see McpHttpServer_RunTokens.h) ──
    /// A bearer token for ONE in-app harness call, valid until end_run() or
    /// `ttl_ms`, whichever comes first. The server enforces `policy` itself
    /// (tools/list hides, tools/call refuses; paper_only runs every call as a
    /// PAPER run). The long-lived token() keeps working for external clients.
    /// Thread-safe. The token is handed to the child through the environment
    /// (kTokenEnvVar), never argv, and is never logged.
    QString begin_run(const RunTokenPolicy& policy, qint64 ttl_ms);
    /// Retire a run token (thread-safe; no-op for an unknown one).
    void end_run(const QString& run_token);
    /// The policy of a live run token; nullopt for the long-lived token and for
    /// anything unknown, ended or expired. Thread-safe.
    std::optional<RunTokenPolicy> run_token_policy(const QString& supplied) const;
    /// Live run tokens (diagnostics, tests).
    int live_run_tokens() const;

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
    /// codex mcp add fincept_community --env FINCEPT_COMMUNITY_MCP_TOKEN=<token> -- <python> <bridge> --url <url>
    QString codex_add_command() const;
    /// [mcp_servers.fincept_community] url = "<url>"  bearer_token_env_var = "FINCEPT_COMMUNITY_MCP_TOKEN"
    QString codex_config_toml() const;
    /// gemini mcp add --transport http fincept_community <url> -H "Authorization: Bearer <token>" -s user
    QString gemini_add_command() const;
    /// ~/.gemini/settings.json: {"mcpServers":{"fincept_community":{"httpUrl":…,"headers":{…},"timeout":…}}}
    QString gemini_settings_json() const;
    /// {"mcpServers":{"fincept_community":{"type":"http","url":"<url>","headers":{"Authorization":"Bearer <token>"}}}}
    QString generic_http_json() const;
    /// VS Code (Copilot) .vscode/mcp.json: {"servers":{"fincept_community":{"type":"http",…}}}
    QString vscode_json() const;
    /// Claude Desktop style stdio entry: {"mcpServers":{"fincept_community":{"command":<python>,"args":[<bridge>,"--url",<url>],"env":{"FINCEPT_COMMUNITY_MCP_TOKEN":<token>}}}}
    /// (Flatpak: "command":"flatpak","args":["run","--command=<python>",<app id>,<bridge>,"--url",<url>]).
    QString stdio_bridge_json() const;
    /// The bridge's command line for a stdio client on the user's system:
    /// <python> <bridge> --url <url>; in the Flatpak the interpreter and the
    /// script are inside the sandbox, so: flatpak run --command=<python> <app id> <bridge> --url <url>.
    QStringList stdio_bridge_argv() const;
    /// Interpreter used for the stdio bridge (stdlib only; prefers the app's venv-numpy2 python).
    QString python_for_bridge() const;
    /// Absolute path of scripts/fincept_mcp/fincept_mcp_stdio.py.
    QString bridge_script_path() const;

    // ── For in-app agent runs (token passed via env FINCEPT_COMMUNITY_MCP_TOKEN) ────────
    /// Claude Code --mcp-config object; header uses ${FINCEPT_COMMUNITY_MCP_TOKEN} expansion.
    QJsonObject claude_mcp_config_env() const;
    /// Codex CLI arguments: {"-c", "mcp_servers.fincept_community.url=\"<url>\"", "-c",
    /// "mcp_servers.fincept_community.bearer_token_env_var=\"FINCEPT_COMMUNITY_MCP_TOKEN\""}.
    QStringList codex_config_overrides() const;
    /// FINCEPT_COMMUNITY_MCP_TOKEN — edition-specific: the private build sets its
    /// own token in FINCEPT_MCP_TOKEN, which this edition never reads.
    static constexpr const char* kTokenEnvVar = edition::kMcpTokenEnvVar;

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
