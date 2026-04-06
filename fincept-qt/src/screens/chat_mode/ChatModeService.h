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

    // ── Session utilities (Phase 2) ───────────────────────────────────────────
    using StatsCallback  = std::function<void(bool ok, ChatStats stats, QString error)>;
    using SearchCallback = std::function<void(bool ok, QVector<ChatMessage> results, QString error)>;

    void get_stats(StatsCallback cb);
    void search_messages(const QString& query, SearchCallback cb);
    void bulk_delete_sessions(const QStringList& uuids, VoidCallback cb);
    void export_sessions(const QStringList& uuids,
                         std::function<void(bool ok, QJsonArray data, QString error)> cb);

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
    using TasksCallback = std::function<void(bool ok, QVector<AgentTask> tasks, QString error)>;
    using TaskCallback  = std::function<void(bool ok, AgentTask task, QString error)>;

    void list_tasks(TasksCallback cb);
    void create_task(const QString& query, const QString& session_id, TaskCallback cb);
    void get_task(const QString& task_id, TaskCallback cb);
    void cancel_task(const QString& task_id, VoidCallback cb);
    void send_task_feedback(const QString& task_id, const QString& feedback, VoidCallback cb);

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
    // SSE events
    void stream_session_meta(const QString& session_id, const QString& new_title);
    void stream_text_delta(const QString& text);
    void stream_tool_end(const QString& tool_name, int duration_ms);
    void stream_step_finish(int tokens_used);
    void stream_finish(int total_tokens);
    void stream_error(const QString& message);
    void stream_heartbeat();

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

    QNetworkAccessManager* nam_     = nullptr;   // for regular JSON calls
    QNetworkAccessManager* sse_nam_ = nullptr;   // dedicated for SSE stream
    QNetworkReply*         sse_reply_ = nullptr; // active SSE connection
    QString                sse_current_event_;   // tracks "event:" field
};

} // namespace fincept::chat_mode
