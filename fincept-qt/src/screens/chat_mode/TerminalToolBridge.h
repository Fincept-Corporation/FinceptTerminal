#pragma once
#include "screens/chat_mode/ChatModeTypes.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace fincept::chat_mode {

/// Bridges the desktop app's internal MCP tools (count provided by
/// McpProvider::tool_count()) to the Finagent backend.
///
/// ONE app-level instance (instance()), not one per window: the cloud agent's
/// tool calls are per user, and a run outlives the screen that started it.
/// Lifecycle: active while a Chat Mode screen is visible in any window, while any
/// cloud run of TaskRunService is going (also with Chat Mode hidden or the window
/// locked); it stops kActiveTailMs after the last of these ends.
///  1. Registers all enabled internal tools via POST /terminal-tools/register
///  2. Polls GET /terminal-tools/pending for tool calls
///  3. Executes locally via McpProvider::call_tool_async()
///  4. Returns results via POST /terminal-tools/result
///
/// Each call runs under mcp::ScopedCallOrigin("Fincept agent: <run title>") and,
/// while any active cloud run is unattended (a scheduled run), under
/// mcp::ScopedPaperRun — money actions are refused outright. Every call (and
/// every refusal) becomes ToolCall / ToolResult events of the active cloud
/// run(s) (data.ambiguous = true when several runs could have made it: the
/// server does not say which).
///
/// Poll cadence. The agent can only call a desktop tool while one of this user's
/// agent runs is in progress, so the bridge polls every kFastPollMs only while a
/// cloud run is Running, for kActiveTailMs after that ends and for kCallTailMs after a delivered call.
/// Otherwise it polls every kIdlePollMs — just often enough to keep the
/// registration alive: every poll renews the server's 300 s registration TTL
/// (finagent/terminal_bridge.py). Servers that do not report `registered` in the
/// poll reply are re-registered every kReregisterIntervalMs instead, as before.
///
/// Registration state: newer servers say whether the agent can see the tools
/// (`agent_visible` on register and on every poll), whether the registration
/// still exists (`registered` on every poll — false: register again) and whether
/// their bridge works at all (`available` — false: wait, do not register). Older
/// servers say none of it; the tools are then "unconfirmed" until the agent calls one.
///
/// Watches McpProvider::generation() to auto-re-register on tool changes.
///
/// Polling pauses while signed out and while the Fincept API is blocking this
/// IP (HttpClient::is_ip_blocked()), and backs off on repeated failures (401s
/// count toward that block, a Retry-After is honoured) until the auth state changes.
class TerminalToolBridge : public QObject {
    Q_OBJECT
  public:
    /// The app-level bridge (created on first use, deleted with the application).
    static TerminalToolBridge& instance();

    /// A Chat Mode screen became visible / hidden (counted across windows).
    void set_view_visible(bool visible);

    bool is_active() const { return active_; }
    const ToolBridgeStatus& status() const { return status_; }

    static constexpr qint64 kFastPollMs = 3000;
    /// Idle cadence: well inside the server's 300 s registration TTL even when a
    /// poll or two fails.
    static constexpr qint64 kIdlePollMs = 120 * 1000;
    static constexpr qint64 kActiveTailMs = 15 * 1000;
    static constexpr qint64 kCallTailMs = 30 * 1000;
    /// Largest tool result kept in a run event (larger results are summarised).
    static constexpr int kMaxResultBytesInEvent = 64 * 1024;

  signals:
    /// What the chat header shows about the tools (see ToolBridgeStatus).
    void tools_status_changed(const fincept::chat_mode::ToolBridgeStatus& status);
    /// A tool ran for the cloud agent. `result` is mcp::ToolResult::to_json().
    void tool_executed(const QString& tool_name, bool success, const QJsonObject& result);
    /// A remote call was refused, or registration started failing (once per streak).
    void bridge_error(const QString& message);
    /// is_active() changed.
    void active_changed(bool active);

  private slots:
    void on_poll_tick();

  private:
    explicit TerminalToolBridge(QObject* parent = nullptr);

    /// Start the bridge — register tools and begin polling.
    void start();
    /// Stop polling and deactivate.
    void stop();
    /// Start / schedule a stop according to views and runs.
    void update_activation();
    bool wanted() const;
    /// A cloud run started / ended.
    void on_runs_changed();
    /// Cloud runs that could have made a tool call now (Planning / Running).
    QStringList calling_runs() const;

    void register_tools();
    void execute_call(const QString& call_id, const QString& tool_name, const QJsonObject& arguments);
    /// ToolCall / ToolResult events on the runs that could have made the call.
    void record_call_event(const QStringList& run_ids, bool result, const QString& call_id, const QString& tool,
                           const QString& body, bool success, const QJsonObject& data);
    void reset_backoff();
    void set_status(ToolBridgeStatus status);
    /// The status implied by the last registration plus what the server last said.
    void refresh_status();
    void on_agent_activity(bool active);
    /// Fast cadence wanted now (agent run in progress, or within a tail after one).
    bool wants_fast_poll(qint64 now) const;

    static constexpr int kTickMs = 3000; // local timer; a poll is sent only when due
    /// Servers that do not report `registered` forget a registration 300 s after it
    /// was made (finagent/terminal_bridge.py TOOL_TTL) — renew it well before that.
    static constexpr qint64 kReregisterIntervalMs = 240 * 1000;
    /// Least time between re-registrations prompted by a poll saying `registered: false`
    /// (the bridge answers that while its store is down — register then fails too).
    static constexpr qint64 kLapseReregisterMinMs = 30 * 1000;
    static constexpr qint64 kMaxPollBackoffMs = 5 * 60 * 1000;

    QTimer* poll_timer_ = nullptr;
    QTimer* stop_timer_ = nullptr; // the tail after the last reason to be active
    bool active_ = false;
    int view_visible_ = 0;
    bool runs_active_ = false; // a cloud run holds a slot (last value seen)
    quint64 last_gen_ = 0; // last McpProvider generation we registered
    qint64 last_register_ok_ms_ = 0; // epoch ms of the last accepted registration (0 = none)

    // One registration / one poll in flight at a time. The tick timer fires regardless of
    // how long the last request takes (15 s timeout), so a slow or failing backend used
    // to pile up overlapping registrations and polls.
    bool register_in_flight_ = false;
    bool poll_in_flight_ = false;
    qint64 register_retry_not_before_ms_ = 0; // epoch ms; a failed registration backs off before the next try
    int poll_failures_ = 0;                   // consecutive failed polls
    qint64 poll_not_before_ms_ = 0;           // epoch ms; next poll after a failure (exponential backoff)
    qint64 last_poll_ms_ = 0;                 // epoch ms the last poll was sent (0 = none yet)
    qint64 fast_until_ms_ = 0;                // epoch ms; fast cadence until then (tails)

    // What the server has told us (newer servers only).
    bool server_tracks_registration_ = false; // poll replies carry `registered`
    bool reregister_requested_ = false;       // the last poll said `registered: false`
    bool visibility_known_ = false;           // replies carry `agent_visible`
    bool agent_visible_ = false;
    /// The server's bridge is down (pending `available: false`, or register 503
    /// terminal_bridge_unavailable): no registration attempts and only the idle
    /// cadence until a poll says `available: true` again.
    bool bridge_unavailable_ = false;
    QString register_error_;                  // last registration failure (server text)

    int offered_count_ = 0;          // tools accepted by the last registration
    QVector<RejectedTool> rejected_; // tools the last registration refused
    bool confirmed_ = false;         // the agent has called one (older servers' only proof)
    ToolBridgeStatus status_;

    // call_ids currently executing. A poll can hand back a call the previous poll
    // already started (it stays "pending" server-side until its result is posted), and
    // running a tool twice is not harmless — some of them mutate terminal state.
    QSet<QString> in_flight_calls_;

    // UI-only categories to exclude from registration
    static const QStringList EXCLUDED_CATEGORIES;

    /// Categories that can move real money or execute arbitrary local code.
    /// Never registered with — and never executed for — the cloud agent.
    ///
    /// Rationale: the LOCAL agent path (mcp::TerminalMcpBridge) marks its calls
    /// with a thread-local flag that AgentService's McpProvider auth checker
    /// reads, so `is_destructive` tools are denied unless that agent's config
    /// carries the per-agent destructive capability token (CreateAgentPanel's
    /// "Allow destructive tools" checkbox). This bridge dispatches calls that
    /// originate from the *cloud* Finagent backend and sets no such flag, so
    /// every tool it runs is treated as a user-driven chat call and skips the
    /// gate entirely. Until an equivalent opt-in exists, fail closed on the
    /// classes where a mistake is unrecoverable.
    static const QStringList BLOCKED_CATEGORIES;

    /// Individually blocked tools whose category also holds harmless read-only
    /// tools (so a whole-category block would be too broad).
    static const QStringList BLOCKED_TOOLS;

    /// True when `name` / `category` names a tool the remote agent must not run.
    static bool is_remote_blocked(const QString& name, const QString& category);
};

} // namespace fincept::chat_mode
