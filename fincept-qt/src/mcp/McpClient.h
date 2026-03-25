#pragma once
// McpClient.h — JSON-RPC 2.0 over stdio for external MCP servers (Qt port)
// Spawns an external MCP server process and communicates via stdin/stdout.

#include "core/result/Result.h"
#include "mcp/McpTypes.h"

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QMutex>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QWaitCondition>

#include <atomic>

namespace fincept::mcp {

struct McpServerConfig {
    QString id;
    QString name;
    QString description;
    QString command; // e.g. "npx", "uvx", "python"
    QStringList args;
    QHash<QString, QString> env;
    QString category;
    bool enabled = true;
    bool auto_start = false;
    ServerStatus status = ServerStatus::Stopped;
    QString created_at;
    QString updated_at;
};

class McpClient : public QObject {
    Q_OBJECT
  public:
    explicit McpClient(const McpServerConfig& config, QObject* parent = nullptr);
    ~McpClient() override;

    // Lifecycle
    Result<void> start();
    void stop();
    bool is_running() const;

    // MCP protocol methods (blocking, must NOT be called from UI thread)
    Result<QJsonObject> initialize();
    Result<std::vector<ExternalTool>> list_tools();
    Result<QJsonObject> call_tool(const QString& name, const QJsonObject& args);
    Result<void> ping();

    const McpServerConfig& config() const { return config_; }

    /// Returns captured stdout/stderr lines (last 500 lines).
    QStringList get_logs() const;

  private:
    struct PendingRequest {
        QJsonObject response;
        bool completed = false;
        QString error;
    };

    McpServerConfig config_;
    QThread* worker_thread_ = nullptr;
    QProcess* process_ = nullptr;
    std::atomic<bool> running_{false};

    // Request tracking — guarded by rpc_mutex_
    mutable QMutex rpc_mutex_;
    QWaitCondition rpc_cond_;
    QHash<int, PendingRequest*> pending_;
    int next_id_ = 1;

    Result<QJsonObject> send_request(const QString& method, const QJsonObject& params, int timeout_ms = 30000);
    void handle_line(const QByteArray& line);
    void append_log(const QString& line);

    static constexpr int MAX_LOG_LINES = 500;
    mutable QMutex log_mutex_;
    QStringList log_lines_;

  private slots:
    void on_ready_read();
    void on_finished(int exit_code, QProcess::ExitStatus status);
};

} // namespace fincept::mcp
