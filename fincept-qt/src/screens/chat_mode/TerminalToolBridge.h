#pragma once
#include <QJsonArray>
#include <QObject>
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
