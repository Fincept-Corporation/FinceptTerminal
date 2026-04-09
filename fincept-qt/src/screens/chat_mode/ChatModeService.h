#pragma once
#include "screens/chat_mode/ChatModeTypes.h"

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QVector>
#include <functional>

namespace fincept::chat_mode {

/// Singleton service for all api.fincept.in/chat/* calls.
/// All callbacks are delivered on the Qt event loop (main thread).
/// SSE streaming uses a dedicated QNetworkAccessManager to avoid
/// blocking the shared HttpClient instance.
class ChatModeService : public QObject {
    Q_OBJECT
  public:
    static ChatModeService& instance();

    // ── Session CRUD ──────────────────────────────────────────────────────────
    using SessionCallback  = std::function<void(bool ok, ChatSession session, QString error)>;
    using SessionsCallback = std::function<void(bool ok, QVector<ChatSession> sessions, QString error)>;
    using VoidCallback     = std::function<void(bool ok, QString error)>;

    void create_session(const QString& title, SessionCallback cb);
    void list_sessions(SessionsCallback cb);
    void get_session(const QString& uuid, SessionCallback cb);
    void delete_session(const QString& uuid, VoidCallback cb);
    void rename_session(const QString& uuid, const QString& title, VoidCallback cb);
    void activate_session(const QString& uuid, VoidCallback cb);

    // ── Save message ─────────────────────────────────────────────────────────
    using MessageCallback = std::function<void(bool ok, ChatMessage msg, QString new_title, QString error)>;

    void save_message(const QString& session_uuid, const QString& role,
                      const QString& content, MessageCallback cb,
                      const QString& provider = {}, const QString& model = {},
                      int tokens_used = 0, int response_time_ms = 0);

    // ── Session utilities ────────────────────────────────────────────────────
    using StatsCallback  = std::function<void(bool ok, ChatStats stats, QString error)>;
    using SearchCallback = std::function<void(bool ok, QVector<ChatMessage> results, QString error)>;

    void get_stats(StatsCallback cb);
    void search_messages(const QString& query, SearchCallback cb);
    void bulk_delete_sessions(const QStringList& uuids, VoidCallback cb);
    void export_sessions(const QStringList& uuids,
                         std::function<void(bool ok, QJsonArray data, QString error)> cb);

    // ── Optimize prompt ──────────────────────────────────────────────────────
    using OptimizeCallback = std::function<void(bool ok, OptimizedPrompt result, QString error)>;

    void optimize_prompt(const QString& prompt, const QString& mode, OptimizeCallback cb);

    // ── Agent chat (non-streaming) ───────────────────────────────────────────
    using AgentChatCallback = std::function<void(bool ok, AgentChatResponse resp, QString error)>;

    void agent_chat(const QString& query, const QString& session_id,
                    StreamMode mode, AgentChatCallback cb,
                    const QString& source = {}, bool auto_approve = false);

    // ── Agent memory ──────────────────────────────────────────────────────────
    using MemoriesCallback = std::function<void(bool ok, QVector<AgentMemory> memories, QString error)>;

    void list_memory(MemoriesCallback cb);
    void save_memory(const QString& key, const QString& value,
                     const QString& memory_type, VoidCallback cb);
    void delete_memory(const QString& key, VoidCallback cb);
    void clear_all_memory(VoidCallback cb);

    // ── Agent schedules ───────────────────────────────────────────────────────
    using SchedulesCallback = std::function<void(bool ok, QVector<AgentSchedule> schedules, QString error)>;
    using ScheduleCallback  = std::function<void(bool ok, AgentSchedule schedule, QString error)>;

    void list_schedules(SchedulesCallback cb);
    void create_schedule(const QString& query, const QString& cron_expression,
                         const QString& session_id, ScheduleCallback cb);
    void delete_schedule(const QString& schedule_id, VoidCallback cb);
    void pause_schedule(const QString& schedule_id, VoidCallback cb);
    void resume_schedule(const QString& schedule_id, VoidCallback cb);

    // ── Agent tasks ───────────────────────────────────────────────────────────
    using TasksCallback   = std::function<void(bool ok, QVector<AgentTask> tasks, QString error)>;
    using TaskCallback    = std::function<void(bool ok, AgentTask task, QString error)>;
    using HistoryCallback = std::function<void(bool ok, QString task_id, QString thread_id,
                                               QVector<TaskHistoryStep> history, QString error)>;
    using ActivityCallback = std::function<void(bool ok, QString task_id,
                                                QVector<TaskActivity> events, int count, QString error)>;

    void list_tasks(TasksCallback cb);
    void create_task(const QString& query, const QString& session_id, TaskCallback cb);
    void get_task(const QString& task_id, TaskCallback cb);
    void cancel_task(const QString& task_id, VoidCallback cb);
    void send_task_feedback(const QString& task_id, const QString& feedback, VoidCallback cb);
    void get_task_history(const QString& task_id, HistoryCallback cb);
    void get_task_activity(const QString& task_id, ActivityCallback cb,
                           int after_id = 0, int limit = 500);

    // ── Task activity SSE stream ──────────────────────────────────────────────
    /// Starts an SSE stream for task activity. Emits task_activity_event as events arrive.
    /// Returns the reply for caller to manage.
    QNetworkReply* stream_task_activity(const QString& task_id, int after_id = 0);
    void abort_task_activity_stream();

    // ── MCP servers ───────────────────────────────────────────────────────────
    using McpServersCallback = std::function<void(bool ok, QVector<McpServer> servers,
                                                   int total_tools, QString error)>;
    using McpServerCallback  = std::function<void(bool ok, McpServer server, QString error)>;

    void list_mcp_servers(McpServersCallback cb);
    void add_mcp_server(const QString& name, const QJsonObject& config, McpServerCallback cb);
    void delete_mcp_server(const QString& server_name, VoidCallback cb);
    void refresh_mcp_servers(McpServersCallback cb);

    // ── Agent monitors ────────────────────────────────────────────────────────
    using MonitorsCallback = std::function<void(bool ok, QVector<AgentMonitor> monitors, QString error)>;
    using MonitorCallback  = std::function<void(bool ok, AgentMonitor monitor, QString error)>;

    void list_monitors(MonitorsCallback cb);
    void create_monitor(const QString& name, const QString& source_type,
                        const QJsonObject& source_config, const QJsonObject& trigger_config,
                        const QString& analysis_query, int check_interval_seconds,
                        const QString& session_id, MonitorCallback cb);
    void delete_monitor(const QString& monitor_id, VoidCallback cb);
    void pause_monitor(const QString& monitor_id, VoidCallback cb);
    void resume_monitor(const QString& monitor_id, VoidCallback cb);

    // ── Credits ───────────────────────────────────────────────────────────────
    using CreditsCallback = std::function<void(bool ok, int credits, QString error)>;
    void get_credits(CreditsCallback cb);

    // ── Terminal tool bridge ─────────────────────────────────────────────────
    using RegisterCallback     = std::function<void(bool ok, int registered, QString error)>;
    using PendingCallsCallback = std::function<void(bool ok, QJsonArray calls, QString error)>;

    void register_terminal_tools(const QJsonArray& tools, const QString& version,
                                 int tool_count, RegisterCallback cb);
    void poll_pending_calls(PendingCallsCallback cb, int limit = 10);
    void submit_tool_result(const QString& call_id, const QJsonObject& result, VoidCallback cb);

    // ── SSE streaming ─────────────────────────────────────────────────────────
    /// Starts a streaming agent call. Emits signals below as events arrive.
    /// Returns the reply so caller can abort early if needed.
    QNetworkReply* stream_message(const QString& message,
                                  const QString& session_id,
                                  StreamMode      mode = StreamMode::Lite,
                                  const QString& source = {},
                                  bool            auto_approve = false,
                                  int             profile_id = 1);

    /// Abort any active SSE stream.
    void abort_stream();

    /// Low-level GET for callers that need raw document access (e.g. messages array).
    void get(const QString& path,
             std::function<void(bool ok, QJsonDocument doc, QString error)> cb);

  signals:
    // SSE events — agent stream
    void stream_session_meta(const QString& session_id, const QString& new_title);
    void stream_text_delta(const QString& text);
    void stream_tool_end(const QString& tool_name, int duration_ms);
    void stream_step_start(int step_number);
    void stream_step_finish(int tokens_used);
    void stream_thinking(const QString& content);
    void stream_finish(int total_tokens);
    void stream_error(const QString& message);
    void stream_heartbeat();

    // SSE events — task activity stream
    void task_activity_event(const TaskActivity& event);
    void task_activity_done();

    // Credit error (HTTP 402)
    void insufficient_credits();

  private:
    explicit ChatModeService(QObject* parent = nullptr);

    QNetworkRequest build_request(const QString& path) const;
    QString         base_url() const;
    QString         api_key() const;
    QString         session_token() const;

    void post(const QString& path, const QJsonObject& body,
              std::function<void(bool ok, QJsonDocument doc, QString error)> cb);
    void put(const QString& path, const QJsonObject& body,
             std::function<void(bool ok, QJsonDocument doc, QString error)> cb);
    void del(const QString& path,
             std::function<void(bool ok, QJsonDocument doc, QString error)> cb);
    void del_with_body(const QString& path, const QJsonObject& body,
                       std::function<void(bool ok, QJsonDocument doc, QString error)> cb);

    void handle_reply(QNetworkReply* reply,
                      std::function<void(bool ok, QJsonDocument doc, QString error)> cb);

    void handle_sse_line(const QByteArray& line);
    void handle_task_sse_line(const QByteArray& line);

    QNetworkAccessManager* nam_     = nullptr;   // for regular JSON calls
    QNetworkAccessManager* sse_nam_ = nullptr;   // dedicated for SSE stream
    QNetworkReply*         sse_reply_ = nullptr;  // active agent SSE connection
    QString                sse_current_event_;    // tracks "event:" field

    QNetworkReply*         task_sse_reply_ = nullptr;  // active task activity SSE
    QString                task_sse_event_;
};

} // namespace fincept::chat_mode
