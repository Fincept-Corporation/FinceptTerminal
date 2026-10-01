#pragma once
#include <QJsonArray>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace fincept::chat_mode {

/// Bridges the desktop app's internal MCP tools (count provided by
/// McpProvider::tool_count()) to the Finagent backend.
///
/// Lifecycle: start() on chat mode entry, stop() on exit.
///  1. Registers all enabled internal tools via POST /terminal-tools/register
///  2. Polls GET /terminal-tools/pending every 3 s for tool calls
///  3. Executes locally via McpProvider::call_tool()
///  4. Returns results via POST /terminal-tools/result
///
/// Watches McpProvider::generation() to auto-re-register on tool changes.
class TerminalToolBridge : public QObject {
    Q_OBJECT
  public:
    explicit TerminalToolBridge(QObject* parent = nullptr);

    /// Start the bridge — register tools and begin polling.
    void start();

    /// Stop polling and deactivate.
    void stop();

    bool is_active() const { return active_; }

  signals:
    void tools_registered(int count);
    void tool_executed(const QString& tool_name, bool success);
    void bridge_error(const QString& message);

  private slots:
    void on_poll_tick();

  private:
    void register_tools();
    void execute_call(const QString& call_id, const QString& tool_name, const QJsonObject& arguments);

    QTimer* poll_timer_ = nullptr;
    bool active_ = false;
    quint64 last_gen_ = 0; // last McpProvider generation we registered

    // One registration / one poll in flight at a time. The 3 s timer fires regardless of
    // how long the last request takes (15 s timeout), so a slow or failing backend used
    // to pile up overlapping registrations and polls.
    bool register_in_flight_ = false;
    bool poll_in_flight_ = false;
    qint64 register_retry_not_before_ms_ = 0; // epoch ms; a failed registration backs off before the next try
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
