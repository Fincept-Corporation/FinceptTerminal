#pragma once
#include <QJsonArray>
#include <QObject>
#include <QTimer>

namespace fincept::chat_mode {

/// Bridges the desktop app's 218 internal MCP tools to the Finagent backend.
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
    void execute_call(const QString& call_id, const QString& tool_name,
                      const QJsonObject& arguments);

    QTimer* poll_timer_   = nullptr;
    bool    active_       = false;
    quint64 last_gen_     = 0;      // last McpProvider generation we registered

    // UI-only categories to exclude from registration
    static const QStringList EXCLUDED_CATEGORIES;
};

} // namespace fincept::chat_mode
