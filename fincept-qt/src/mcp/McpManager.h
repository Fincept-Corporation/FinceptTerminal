#pragma once
// McpManager.h — External MCP server lifecycle management (Qt port)
// Manages multiple external MCP server instances with health checking.

#include "core/result/Result.h"
#include "mcp/McpClient.h"

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QTimer>

#include <atomic>
#include <memory>
#include <vector>

namespace fincept::mcp {

class McpManager : public QObject {
    Q_OBJECT
  public:
    static McpManager& instance();

    // ── Configuration ──────────────────────────────────────────────────

    /// Load server configs from the McpServerRepository
    void initialize();

    Result<void> save_server(const McpServerConfig& config);
    Result<void> remove_server(const QString& id);
    std::vector<McpServerConfig> get_servers() const;

    // ── Lifecycle ──────────────────────────────────────────────────────

    Result<void> start_server(const QString& id);
    Result<void> stop_server(const QString& id);
    Result<void> restart_server(const QString& id);
    void start_auto_servers();
    void stop_all();
    void shutdown();

    // ── Health Check ───────────────────────────────────────────────────

    void start_health_check(int interval_seconds = 60);
    void stop_health_check();

    // ── Tool Aggregation ───────────────────────────────────────────────

    std::vector<ExternalTool> get_all_external_tools();
    Result<QJsonObject> call_external_tool(const QString& server_id, const QString& tool_name, const QJsonObject& args);

    /// Returns captured log lines for a running server. For a server whose last start
    /// FAILED there is no live client, so this falls back to the output that attempt
    /// produced (kept until the server is started successfully or removed) — that output
    /// is the only explanation of why it errored. Empty if neither exists.
    QStringList get_logs(const QString& id) const;

    McpManager(const McpManager&) = delete;
    McpManager& operator=(const McpManager&) = delete;

  signals:
    /// Emitted whenever server list or status changes (save/remove/start/stop).
    void servers_changed();

  private:
    McpManager();

    QHash<QString, std::shared_ptr<McpClient>> clients_;
    QHash<QString, McpServerConfig> configs_;
    QHash<QString, std::vector<ExternalTool>> tool_cache_;
    QHash<QString, int> restart_attempts_;
    // Output of the most recent FAILED start per server (a failed attempt's McpClient is
    // discarded, taking its log buffer with it). One entry per server id, each bounded by
    // McpClient's own 500-line cap; guarded by mutex_.
    QHash<QString, QStringList> last_start_log_;

    QTimer* health_timer_ = nullptr;
    mutable QMutex mutex_;
    std::atomic<bool> health_check_running_{false}; // guards against overlapping async health passes

    static constexpr int MAX_RESTART_ATTEMPTS = 3;

    McpClient* get_client(const QString& id) const;
    void refresh_tools_for(const QString& id);
    // Handles one server's ping result on the MAIN thread (restart uses QProcess,
    // which is thread-affine and must stay on the thread that owns it).
    void on_health_result(const QString& id, bool ok);

  private slots:
    void do_health_check();
};

} // namespace fincept::mcp
