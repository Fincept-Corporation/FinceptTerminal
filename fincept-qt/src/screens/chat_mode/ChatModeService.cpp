#include "screens/chat_mode/ChatModeService.h"

#include "auth/AuthManager.h"
#include "core/logging/Logger.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace fincept::chat_mode {

static constexpr const char* API_BASE = "https://api.fincept.in";

// ── Singleton ─────────────────────────────────────────────────────────────────

ChatModeService& ChatModeService::instance() {
    static ChatModeService s;
    return s;
}

ChatModeService::ChatModeService(QObject* parent) : QObject(parent) {
    nam_ = new QNetworkAccessManager(this);
    sse_nam_ = new QNetworkAccessManager(this);
}

// ── Auth helpers ──────────────────────────────────────────────────────────────

QString ChatModeService::base_url() const {
    return QString::fromLatin1(API_BASE);
}

QString ChatModeService::api_key() const {
    return auth::AuthManager::instance().session().api_key;
}

QString ChatModeService::session_token() const {
    return auth::AuthManager::instance().session().session_token;
}

QNetworkRequest ChatModeService::build_request(const QString& path) const {
    QNetworkRequest req{QUrl(base_url() + path)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setHeader(QNetworkRequest::UserAgentHeader, "FinceptTerminal/4.0");
    const QString key = api_key();
    const QString tok = session_token();
    if (!key.isEmpty())
        req.setRawHeader("X-API-Key", key.toUtf8());
    if (!tok.isEmpty())
        req.setRawHeader("X-Session-Token", tok.toUtf8());
    return req;
}

// ── Reply handler ─────────────────────────────────────────────────────────────

void ChatModeService::handle_reply(QNetworkReply* reply,
                                   std::function<void(bool ok, QJsonDocument doc, QString error)> cb) {
    // 15-second timeout — abort hanging requests so they don't silently block
    auto* timer = new QTimer(reply);
    timer->setSingleShot(true);
    timer->setInterval(15000);
    connect(timer, &QTimer::timeout, reply, [reply]() {
        LOG_WARN("ChatModeService", QString("Request timed out: %1").arg(reply->url().path()));
        reply->abort();
    });
    timer->start();

    connect(reply, &QNetworkReply::finished, this, [this, reply, cb = std::move(cb)]() mutable {
        // Read all data BEFORE deleteLater
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->readAll();
        const auto net_err = reply->error();
        const QString url_path = reply->url().path();
        reply->deleteLater();

        LOG_DEBUG("ChatModeService", QString("%1  HTTP %2  %3 bytes").arg(url_path).arg(status).arg(data.size()));

        if (status == 402) {
            LOG_WARN("ChatModeService", "Insufficient credits (402)");
            emit insufficient_credits();
            cb(false, {}, "Insufficient credits");
            return;
        }

        if (status == 401 || status == 403) {
            LOG_WARN("ChatModeService", QString("Auth error %1 on %2").arg(status).arg(url_path));
            cb(false, {}, QString("Auth error HTTP_%1").arg(status));
            return;
        }

        QJsonParseError pe;
        QJsonDocument doc = QJsonDocument::fromJson(data, &pe);

        if (net_err != QNetworkReply::NoError && net_err != QNetworkReply::OperationCanceledError) {
            const QString err =
                QString("HTTP %1 %2: %3")
                    .arg(status)
                    .arg(url_path)
                    .arg(QNetworkReply::staticMetaObject
                             .enumerator(QNetworkReply::staticMetaObject.indexOfEnumerator("NetworkError"))
                             .valueToKey(net_err));
            LOG_WARN("ChatModeService", err);
            cb(false, doc, err);
            return;
        }

        if (pe.error != QJsonParseError::NoError) {
            const QString err = QString("JSON parse error on %1: %2").arg(url_path, pe.errorString());
            LOG_WARN("ChatModeService", err);
            cb(false, {}, err);
            return;
        }

        // Check API-level success flag if present
        const QJsonObject root = doc.object();
        if (root.contains("success") && !root["success"].toBool()) {
            const QString err = root["message"].toString("API returned success:false");
            LOG_WARN("ChatModeService", QString("%1: %2").arg(url_path, err));
            cb(false, doc, err);
            return;
        }

        cb(true, doc, {});
    });
}

// ── HTTP verbs ────────────────────────────────────────────────────────────────

void ChatModeService::get(const QString& path, std::function<void(bool, QJsonDocument, QString)> cb) {
    auto* reply = nam_->get(build_request(path));
    handle_reply(reply, std::move(cb));
}

void ChatModeService::post(const QString& path, const QJsonObject& body,
                           std::function<void(bool, QJsonDocument, QString)> cb) {
    auto* reply = nam_->post(build_request(path), QJsonDocument(body).toJson(QJsonDocument::Compact));
    handle_reply(reply, std::move(cb));
}

void ChatModeService::put(const QString& path, const QJsonObject& body,
                          std::function<void(bool, QJsonDocument, QString)> cb) {
    auto* reply = nam_->put(build_request(path), QJsonDocument(body).toJson(QJsonDocument::Compact));
    handle_reply(reply, std::move(cb));
}

void ChatModeService::del(const QString& path, std::function<void(bool, QJsonDocument, QString)> cb) {
    auto* reply = nam_->deleteResource(build_request(path));
    handle_reply(reply, std::move(cb));
}

void ChatModeService::del_with_body(const QString& path, const QJsonObject& body,
                                    std::function<void(bool, QJsonDocument, QString)> cb) {
    QNetworkRequest req = build_request(path);
    auto* reply = nam_->sendCustomRequest(req, "DELETE", QJsonDocument(body).toJson(QJsonDocument::Compact));
    handle_reply(reply, std::move(cb));
}

// ── Session CRUD ──────────────────────────────────────────────────────────────

void ChatModeService::create_session(const QString& title, SessionCallback cb) {
    LOG_INFO("ChatModeService", QString("Creating session: \"%1\"").arg(title));
    QJsonObject body;
    body["title"] = title;
    post("/chat/sessions", body, [cb = std::move(cb)](bool ok, QJsonDocument doc, QString err) {
        if (!ok) {
            LOG_WARN("ChatModeService", "create_session failed: " + err);
            cb(false, {}, err);
            return;
        }
        const QJsonObject data = doc.object().value("data").toObject();
        const QJsonObject session = data.value("session").toObject();
        const QString uuid = session.value("session_uuid").toString();
        LOG_INFO("ChatModeService", QString("Session created: %1").arg(uuid));
        cb(true, ChatSession::from_json(session), {});
    });
}

void ChatModeService::list_sessions(SessionsCallback cb) {
    LOG_DEBUG("ChatModeService", "Listing sessions");
    get("/chat/sessions", [cb = std::move(cb)](bool ok, QJsonDocument doc, QString err) {
        if (!ok) {
            LOG_WARN("ChatModeService", "list_sessions failed: " + err);
            cb(false, {}, err);
            return;
        }
        const QJsonArray arr = doc.object().value("data").toObject().value("sessions").toArray();
        QVector<ChatSession> sessions;
        sessions.reserve(arr.size());
        for (const auto& v : arr)
            sessions.append(ChatSession::from_json(v.toObject()));
        LOG_INFO("ChatModeService", QString("Listed %1 sessions").arg(sessions.size()));
        cb(true, sessions, {});
    });
}

void ChatModeService::get_session(const QString& uuid, SessionCallback cb) {
    LOG_DEBUG("ChatModeService", QString("Getting session: %1").arg(uuid));
    get("/chat/sessions/" + uuid, [cb = std::move(cb), uuid](bool ok, QJsonDocument doc, QString err) {
        if (!ok) {
            LOG_WARN("ChatModeService", "get_session failed: " + err);
            cb(false, {}, err);
            return;
        }
        const QJsonObject data = doc.object().value("data").toObject();
        const QJsonObject session = data.value("session").toObject();
        const int msg_count = data.value("messages").toArray().size();
        LOG_INFO("ChatModeService", QString("Got session %1 with %2 messages").arg(uuid).arg(msg_count));
        cb(true, ChatSession::from_json(session), {});
    });
}

void ChatModeService::delete_session(const QString& uuid, VoidCallback cb) {
    del("/chat/sessions/" + uuid, [cb = std::move(cb)](bool ok, QJsonDocument, QString err) { cb(ok, err); });
}

void ChatModeService::rename_session(const QString& uuid, const QString& title, VoidCallback cb) {
    QJsonObject body;
    body["title"] = title;
    put("/chat/sessions/" + uuid + "/title", body,
        [cb = std::move(cb)](bool ok, QJsonDocument, QString err) { cb(ok, err); });
}

void ChatModeService::activate_session(const QString& uuid, VoidCallback cb) {
    put("/chat/sessions/" + uuid + "/activate", {},
        [cb = std::move(cb)](bool ok, QJsonDocument, QString err) { cb(ok, err); });
}

// ── Save message ──────────────────────────────────────────────────────────────

void ChatModeService::save_message(const QString& session_uuid, const QString& role, const QString& content,
                                   MessageCallback cb, const QString& provider, const QString& model, int tokens_used,
                                   int response_time_ms) {
    LOG_INFO("ChatModeService", QString("Saving %1 message to session %2").arg(role, session_uuid));
    QJsonObject body;
    body["role"] = role;
    body["content"] = content;
    if (!provider.isEmpty())
        body["provider"] = provider;
    if (!model.isEmpty())
        body["model"] = model;
    if (tokens_used > 0)
        body["tokens_used"] = tokens_used;
    if (response_time_ms > 0)
        body["response_time_ms"] = response_time_ms;

    post("/chat/sessions/" + session_uuid + "/save-message", body,
         [cb = std::move(cb)](bool ok, QJsonDocument doc, QString err) {
             if (!ok) {
                 cb(false, {}, {}, err);
                 return;
             }
             const QJsonObject data = doc.object().value("data").toObject();
             const QJsonObject msg_obj = data.value("message").toObject();
             ChatMessage msg;
             msg.uuid = msg_obj["message_uuid"].toString();
             msg.role = msg_obj["role"].toString();
             msg.content = msg_obj["content"].toString();
             msg.created_at = msg_obj["created_at"].toString();
             const QString new_title = data["new_title"].toString();
             cb(true, msg, new_title, {});
         });
}

// ── Session utilities ─────────────────────────────────────────────────────────

void ChatModeService::get_stats(StatsCallback cb) {
    get("/chat/stats", [cb = std::move(cb)](bool ok, QJsonDocument doc, QString err) {
        if (!ok) {
            cb(false, {}, err);
            return;
        }
        const QJsonObject data = doc.object().value("data").toObject(doc.object());
        cb(true, ChatStats::from_json(data), {});
    });
}

void ChatModeService::search_messages(const QString& query, SearchCallback cb) {
    const QString path =
        QString("/chat/search?query=%1&limit=20").arg(QString::fromUtf8(QUrl::toPercentEncoding(query)));
    get(path, [cb = std::move(cb)](bool ok, QJsonDocument doc, QString err) {
        if (!ok) {
            cb(false, {}, err);
            return;
        }
        // Response: { data: { results: [...], total: N, query: "..." } }
        const QJsonArray arr = doc.object().value("data").toObject().value("results").toArray();
        QVector<ChatMessage> results;
        results.reserve(arr.size());
        for (const auto& v : arr) {
            const QJsonObject o = v.toObject();
            ChatMessage m;
            m.uuid = o["message_uuid"].toString();
            m.role = o["role"].toString();
            m.content = o["content"].toString();
            m.created_at = o["created_at"].toString();
            results.append(m);
        }
        cb(true, results, {});
    });
}

void ChatModeService::bulk_delete_sessions(const QStringList& uuids, VoidCallback cb) {
    QJsonObject body;
    QJsonArray arr;
    for (const auto& u : uuids)
        arr.append(u);
    body["session_uuids"] = arr;
    del_with_body("/chat/sessions/bulk-delete", body,
                  [cb = std::move(cb)](bool ok, QJsonDocument, QString err) { cb(ok, err); });
}

void ChatModeService::export_sessions(const QStringList& uuids, std::function<void(bool, QJsonArray, QString)> cb) {
    QJsonObject body;
    QJsonArray arr;
    for (const auto& u : uuids)
        arr.append(u);
    body["session_uuids"] = arr;
    body["format"] = "json";
    post("/chat/export", body, [cb = std::move(cb)](bool ok, QJsonDocument doc, QString err) {
        if (!ok) {
            cb(false, {}, err);
            return;
        }
        const QJsonArray data = doc.object().value("data").toObject().value("data").toArray();
        cb(true, data, {});
    });
}

// ── Optimize prompt ───────────────────────────────────────────────────────────

void ChatModeService::optimize_prompt(const QString& prompt, const QString& mode, OptimizeCallback cb) {
    LOG_INFO("ChatModeService", QString("Optimizing prompt (%1): \"%2\"").arg(mode, prompt.left(60)));
    QJsonObject body;
    body["prompt"] = prompt;
    if (!mode.isEmpty())
        body["mode"] = mode;

    // Longer timeout for LLM calls
    auto* reply =
        nam_->post(build_request("/chat/optimize-prompt"), QJsonDocument(body).toJson(QJsonDocument::Compact));
    // Override default 15s timeout — LLM needs more time
    auto* timer = new QTimer(reply);
    timer->setSingleShot(true);
    timer->setInterval(45000);
    connect(timer, &QTimer::timeout, reply, [reply]() {
        LOG_WARN("ChatModeService", "optimize-prompt timed out");
        reply->abort();
    });
    timer->start();

    connect(reply, &QNetworkReply::finished, this, [this, reply, cb = std::move(cb)]() mutable {
        const QByteArray data = reply->readAll();
        const auto net_err = reply->error();
        reply->deleteLater();

        if (net_err != QNetworkReply::NoError && net_err != QNetworkReply::OperationCanceledError) {
            cb(false, {}, "Optimize prompt request failed");
            return;
        }

        QJsonParseError pe;
        QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
        if (pe.error != QJsonParseError::NoError) {
            cb(false, {}, "JSON parse error");
            return;
        }

        const QJsonObject root = doc.object();
        if (root.contains("success") && !root["success"].toBool()) {
            cb(false, {}, root["message"].toString("Optimize failed"));
            return;
        }

        const QJsonObject res_data = root.value("data").toObject();
        OptimizedPrompt result;
        result.original = res_data["original"].toString();

        // Response can have optimized_prompt as array or optimized as string
        if (res_data.contains("optimized")) {
            result.optimized = res_data["optimized"].toString();
        } else if (res_data.contains("optimized_prompt")) {
            // Array format — extract the "text" type entry
            const QJsonArray arr = res_data["optimized_prompt"].toArray();
            for (const auto& v : arr) {
                const QJsonObject item = v.toObject();
                if (item["type"].toString() == "text") {
                    result.optimized = item["text"].toString();
                    break;
                }
            }
        }

        LOG_INFO("ChatModeService", QString("Prompt optimized: %1 chars -> %2 chars")
                                        .arg(result.original.length())
                                        .arg(result.optimized.length()));
        cb(true, result, {});
    });
}

// ── Agent chat (non-streaming) ────────────────────────────────────────────────

void ChatModeService::agent_chat(const QString& query, const QString& session_id, StreamMode mode, AgentChatCallback cb,
                                 const QString& source, bool auto_approve) {
    LOG_INFO("ChatModeService",
             QString("Agent chat [%1]: \"%2\"").arg(mode == StreamMode::Deep ? "deep" : "lite", query.left(60)));

    QJsonObject body;
    body["query"] = query;
    body["mode"] = (mode == StreamMode::Deep) ? "deep" : "lite";
    body["auto_approve"] = auto_approve;
    if (!session_id.isEmpty())
        body["session_id"] = session_id;
    if (!source.isEmpty())
        body["source"] = source;

    // Longer timeout for agent calls
    auto* reply = nam_->post(build_request("/chat/agent/chat"), QJsonDocument(body).toJson(QJsonDocument::Compact));
    auto* timer = new QTimer(reply);
    timer->setSingleShot(true);
    timer->setInterval(120000); // 2 min for deep analysis
    connect(timer, &QTimer::timeout, reply, [reply]() {
        LOG_WARN("ChatModeService", "agent/chat timed out");
        reply->abort();
    });
    timer->start();

    connect(reply, &QNetworkReply::finished, this, [this, reply, cb = std::move(cb)]() mutable {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->readAll();
        const auto net_err = reply->error();
        reply->deleteLater();

        if (status == 402) {
            emit insufficient_credits();
            cb(false, {}, "Insufficient credits");
            return;
        }

        if (net_err != QNetworkReply::NoError && net_err != QNetworkReply::OperationCanceledError) {
            cb(false, {}, "Agent chat request failed");
            return;
        }

        QJsonParseError pe;
        QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
        if (pe.error != QJsonParseError::NoError) {
            cb(false, {}, "JSON parse error");
            return;
        }

        const QJsonObject root = doc.object();
        if (root.contains("success") && !root["success"].toBool()) {
            cb(false, {}, root["message"].toString("Agent chat failed"));
            return;
        }

        cb(true, AgentChatResponse::from_json(root), {});
    });
}

// ── Agent memory ──────────────────────────────────────────────────────────────

void ChatModeService::list_memory(MemoriesCallback cb) {
    get("/chat/agent/memory", [cb = std::move(cb)](bool ok, QJsonDocument doc, QString err) {
        if (!ok) {
            cb(false, {}, err);
            return;
        }
        QVector<AgentMemory> memories;
        const QJsonArray arr = doc.object().value("memories").toArray();
        for (const auto& v : arr)
            memories.append(AgentMemory::from_json(v.toObject()));
        cb(true, memories, {});
    });
}

void ChatModeService::save_memory(const QString& key, const QString& value, const QString& memory_type,
                                  VoidCallback cb) {
    QJsonObject body;
    body["key"] = key;
    body["value"] = value;
    body["memory_type"] = memory_type;
    post("/chat/agent/memory", body, [cb = std::move(cb)](bool ok, QJsonDocument, QString err) { cb(ok, err); });
}

void ChatModeService::delete_memory(const QString& key, VoidCallback cb) {
    del("/chat/agent/memory/" + key, [cb = std::move(cb)](bool ok, QJsonDocument, QString err) { cb(ok, err); });
}

void ChatModeService::clear_all_memory(VoidCallback cb) {
    del("/chat/agent/memory", [cb = std::move(cb)](bool ok, QJsonDocument, QString err) { cb(ok, err); });
}

// ── Agent schedules ───────────────────────────────────────────────────────────

void ChatModeService::list_schedules(SchedulesCallback cb) {
    get("/chat/agent/schedules", [cb = std::move(cb)](bool ok, QJsonDocument doc, QString err) {
        if (!ok) {
            cb(false, {}, err);
            return;
        }
        QVector<AgentSchedule> schedules;
        const QJsonArray arr = doc.object().value("schedules").toArray();
        for (const auto& v : arr)
            schedules.append(AgentSchedule::from_json(v.toObject()));
        cb(true, schedules, {});
    });
}

void ChatModeService::create_schedule(const QString& query, const QString& cron_expression, const QString& session_id,
                                      ScheduleCallback cb) {
    QJsonObject body;
    body["query"] = query;
    body["cron_expression"] = cron_expression;
    if (!session_id.isEmpty())
        body["session_id"] = session_id;
    post("/chat/agent/schedules", body, [cb = std::move(cb)](bool ok, QJsonDocument doc, QString err) {
        if (!ok) {
            cb(false, {}, err);
            return;
        }
        const QJsonObject data = doc.object().value("data").toObject(doc.object());
        cb(true, AgentSchedule::from_json(data), {});
    });
}

void ChatModeService::delete_schedule(const QString& schedule_id, VoidCallback cb) {
    del("/chat/agent/schedules/" + schedule_id,
        [cb = std::move(cb)](bool ok, QJsonDocument, QString err) { cb(ok, err); });
}

void ChatModeService::pause_schedule(const QString& schedule_id, VoidCallback cb) {
    put("/chat/agent/schedules/" + schedule_id + "/pause", {},
        [cb = std::move(cb)](bool ok, QJsonDocument, QString err) { cb(ok, err); });
}

void ChatModeService::resume_schedule(const QString& schedule_id, VoidCallback cb) {
    put("/chat/agent/schedules/" + schedule_id + "/resume", {},
        [cb = std::move(cb)](bool ok, QJsonDocument, QString err) { cb(ok, err); });
}

// ── Agent tasks ───────────────────────────────────────────────────────────────

void ChatModeService::list_tasks(TasksCallback cb) {
    get("/chat/agent/tasks?limit=50", [cb = std::move(cb)](bool ok, QJsonDocument doc, QString err) {
        if (!ok) {
            cb(false, {}, err);
            return;
        }
        QVector<AgentTask> tasks;
        const QJsonArray arr = doc.object().value("tasks").toArray();
        for (const auto& v : arr)
            tasks.append(AgentTask::from_json(v.toObject()));
        cb(true, tasks, {});
    });
}

void ChatModeService::create_task(const QString& query, const QString& session_id, TaskCallback cb) {
    QJsonObject body;
    body["query"] = query;
    if (!session_id.isEmpty())
        body["session_id"] = session_id;
    post("/chat/agent/tasks", body, [cb = std::move(cb)](bool ok, QJsonDocument doc, QString err) {
        if (!ok) {
            cb(false, {}, err);
            return;
        }
        const QJsonObject data = doc.object().value("data").toObject(doc.object());
        cb(true, AgentTask::from_json(data), {});
    });
}

void ChatModeService::get_task(const QString& task_id, TaskCallback cb) {
    get("/chat/agent/tasks/" + task_id, [cb = std::move(cb)](bool ok, QJsonDocument doc, QString err) {
        if (!ok) {
            cb(false, {}, err);
            return;
        }
        const QJsonObject data = doc.object().value("data").toObject(doc.object());
        cb(true, AgentTask::from_json(data), {});
    });
}

void ChatModeService::cancel_task(const QString& task_id, VoidCallback cb) {
    del("/chat/agent/tasks/" + task_id, [cb = std::move(cb)](bool ok, QJsonDocument, QString err) { cb(ok, err); });
}

void ChatModeService::send_task_feedback(const QString& task_id, const QString& feedback, VoidCallback cb) {
    QJsonObject body;
    body["feedback"] = feedback;
    post("/chat/agent/tasks/" + task_id + "/feedback", body,
         [cb = std::move(cb)](bool ok, QJsonDocument, QString err) { cb(ok, err); });
}

void ChatModeService::get_task_history(const QString& task_id, HistoryCallback cb) {
    get("/chat/agent/tasks/" + task_id + "/history",
        [cb = std::move(cb), task_id](bool ok, QJsonDocument doc, QString err) {
            if (!ok) {
                cb(false, {}, {}, {}, err);
                return;
            }
            const QJsonObject root = doc.object();
            const QString tid = root["task_id"].toString();
            const QString thread = root["thread_id"].toString();
            QVector<TaskHistoryStep> steps;
            for (const auto& v : root["history"].toArray())
                steps.append(TaskHistoryStep::from_json(v.toObject()));
            cb(true, tid, thread, steps, {});
        });
}

void ChatModeService::get_task_activity(const QString& task_id, ActivityCallback cb, int after_id, int limit) {
    QString path = QString("/chat/agent/tasks/%1/activity?limit=%2").arg(task_id).arg(limit);
    if (after_id > 0)
        path += QString("&after_id=%1").arg(after_id);

    get(path, [cb = std::move(cb), task_id](bool ok, QJsonDocument doc, QString err) {
        if (!ok) {
            cb(false, {}, {}, 0, err);
            return;
        }
        const QJsonObject root = doc.object();
        QVector<TaskActivity> events;
        for (const auto& v : root["events"].toArray())
            events.append(TaskActivity::from_json(v.toObject()));
        const int count = root["count"].toInt();
        cb(true, task_id, events, count, {});
    });
}

// ── Task activity SSE stream ──────────────────────────────────────────────────

QNetworkReply* ChatModeService::stream_task_activity(const QString& task_id, int after_id) {
    abort_task_activity_stream();

    const QString path = QString("/chat/agent/tasks/%1/activity/stream?after_id=%2").arg(task_id).arg(after_id);
    QNetworkRequest req = build_request(path);
    req.setRawHeader("Accept", "text/event-stream");
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);

    task_sse_reply_ = sse_nam_->get(req);
    task_sse_event_.clear();

    connect(task_sse_reply_, &QNetworkReply::readyRead, this, [this]() {
        while (task_sse_reply_ && task_sse_reply_->canReadLine()) {
            const QByteArray line = task_sse_reply_->readLine().trimmed();
            handle_task_sse_line(line);
        }
    });

    connect(task_sse_reply_, &QNetworkReply::finished, this, [this]() {
        if (!task_sse_reply_)
            return;
        task_sse_reply_->deleteLater();
        task_sse_reply_ = nullptr;
    });

    return task_sse_reply_;
}

void ChatModeService::abort_task_activity_stream() {
    if (task_sse_reply_) {
        task_sse_reply_->abort();
        task_sse_reply_->deleteLater();
        task_sse_reply_ = nullptr;
    }
}

void ChatModeService::handle_task_sse_line(const QByteArray& line) {
    if (line.startsWith("event:")) {
        task_sse_event_ = QString::fromUtf8(line.mid(6).trimmed());
        return;
    }

    if (line.startsWith("data:")) {
        const QByteArray json_bytes = line.mid(5).trimmed();
        QJsonParseError pe;
        const QJsonDocument doc = QJsonDocument::fromJson(json_bytes, &pe);
        if (pe.error != QJsonParseError::NoError)
            return;

        const QString& ev = task_sse_event_;
        if (ev == "activity") {
            emit task_activity_event(TaskActivity::from_json(doc.object()));
        } else if (ev == "done") {
            emit task_activity_done();
        }

        task_sse_event_.clear();
    }
}

// ── Credits ──────────────────────────────────────────────────────────────────

void ChatModeService::get_credits(CreditsCallback cb) {
    const QString path = QString("/user/profile?_t=%1").arg(QDateTime::currentMSecsSinceEpoch());
    get(path, [cb = std::move(cb)](bool ok, QJsonDocument doc, QString err) {
        if (!ok) {
            cb(false, 0, err);
            return;
        }
        const int credits = doc.object().value("data").toObject().value("credit_balance").toInt();
        cb(true, credits, {});
    });
}

// ── Terminal tool bridge ──────────────────────────────────────────────────────

void ChatModeService::register_terminal_tools(const QJsonArray& tools, const QString& version, int tool_count,
                                              RegisterCallback cb) {
    LOG_INFO("ChatModeService", QString("Registering %1 terminal tools (v%2)").arg(tool_count).arg(version));
    QJsonObject body;
    body["tools"] = tools;
    body["terminal_version"] = version;
    body["tool_count"] = tool_count;
    post("/chat/agent/terminal-tools/register", body, [cb = std::move(cb)](bool ok, QJsonDocument doc, QString err) {
        if (!ok) {
            cb(false, 0, err);
            return;
        }
        const int reg = doc.object()["registered"].toInt();
        cb(true, reg, {});
    });
}

void ChatModeService::poll_pending_calls(PendingCallsCallback cb, int limit) {
    const QString path = QString("/chat/agent/terminal-tools/pending?limit=%1").arg(limit);
    get(path, [cb = std::move(cb)](bool ok, QJsonDocument doc, QString err) {
        if (!ok) {
            cb(false, {}, err);
            return;
        }
        cb(true, doc.object()["calls"].toArray(), {});
    });
}

void ChatModeService::submit_tool_result(const QString& call_id, const QJsonObject& result, VoidCallback cb) {
    QJsonObject body;
    body["call_id"] = call_id;
    body["result"] = result;
    post("/chat/agent/terminal-tools/result", body,
         [cb = std::move(cb)](bool ok, QJsonDocument, QString err) { cb(ok, err); });
}

// ── MCP servers ───────────────────────────────────────────────────────────────

void ChatModeService::list_mcp_servers(McpServersCallback cb) {
    get("/chat/agent/mcp/servers", [cb = std::move(cb)](bool ok, QJsonDocument doc, QString err) {
        if (!ok) {
            cb(false, {}, 0, err);
            return;
        }
        const QJsonObject root = doc.object();
        QVector<McpServer> servers;
        for (const auto& v : root["servers"].toArray())
            servers.append(McpServer::from_json(v.toObject()));
        const int total_tools = root["total_tools"].toInt();
        cb(true, servers, total_tools, {});
    });
}

void ChatModeService::add_mcp_server(const QString& name, const QJsonObject& config, McpServerCallback cb) {
    LOG_INFO("ChatModeService", QString("Adding MCP server: %1").arg(name));
    QJsonObject body;
    body["name"] = name;
    body["config"] = config;
    post("/chat/agent/mcp/servers", body, [cb = std::move(cb), name](bool ok, QJsonDocument doc, QString err) {
        if (!ok) {
            cb(false, {}, err);
            return;
        }
        const QJsonObject root = doc.object();
        McpServer srv;
        srv.name = root["name"].toString(name);
        srv.tools_count = root["tools_loaded"].toInt();
        srv.status = root.contains("error") && !root["error"].toString().isEmpty() ? "error" : "connected";
        for (const auto& v : root["tools"].toArray())
            srv.tools.append(McpTool::from_json(v.toObject()));
        cb(true, srv, {});
    });
}

void ChatModeService::delete_mcp_server(const QString& server_name, VoidCallback cb) {
    del("/chat/agent/mcp/servers/" + server_name,
        [cb = std::move(cb)](bool ok, QJsonDocument, QString err) { cb(ok, err); });
}

void ChatModeService::refresh_mcp_servers(McpServersCallback cb) {
    post("/chat/agent/mcp/servers/refresh", {}, [cb = std::move(cb)](bool ok, QJsonDocument doc, QString err) {
        if (!ok) {
            cb(false, {}, 0, err);
            return;
        }
        const QJsonObject root = doc.object();
        QVector<McpServer> servers;
        for (const auto& v : root["servers"].toArray())
            servers.append(McpServer::from_json(v.toObject()));
        const int total_tools = root["total_tools"].toInt();
        cb(true, servers, total_tools, {});
    });
}

// ── Agent monitors ────────────────────────────────────────────────────────────

void ChatModeService::list_monitors(MonitorsCallback cb) {
    get("/chat/agent/monitors", [cb = std::move(cb)](bool ok, QJsonDocument doc, QString err) {
        if (!ok) {
            cb(false, {}, err);
            return;
        }
        QVector<AgentMonitor> monitors;
        for (const auto& v : doc.object()["monitors"].toArray())
            monitors.append(AgentMonitor::from_json(v.toObject()));
        cb(true, monitors, {});
    });
}

void ChatModeService::create_monitor(const QString& name, const QString& source_type, const QJsonObject& source_config,
                                     const QJsonObject& trigger_config, const QString& analysis_query,
                                     int check_interval_seconds, const QString& session_id, MonitorCallback cb) {
    LOG_INFO("ChatModeService", QString("Creating monitor: %1").arg(name));
    QJsonObject body;
    body["name"] = name;
    body["source_type"] = source_type;
    body["source_config"] = source_config;
    body["trigger_config"] = trigger_config;
    body["analysis_query"] = analysis_query;
    body["check_interval_seconds"] = check_interval_seconds;
    if (!session_id.isEmpty())
        body["session_id"] = session_id;

    post("/chat/agent/monitors", body, [cb = std::move(cb)](bool ok, QJsonDocument doc, QString err) {
        if (!ok) {
            cb(false, {}, err);
            return;
        }
        const QJsonObject root = doc.object();
        cb(true, AgentMonitor::from_json(root), {});
    });
}

void ChatModeService::delete_monitor(const QString& monitor_id, VoidCallback cb) {
    del("/chat/agent/monitors/" + monitor_id,
        [cb = std::move(cb)](bool ok, QJsonDocument, QString err) { cb(ok, err); });
}

void ChatModeService::pause_monitor(const QString& monitor_id, VoidCallback cb) {
    put("/chat/agent/monitors/" + monitor_id + "/pause", {},
        [cb = std::move(cb)](bool ok, QJsonDocument, QString err) { cb(ok, err); });
}

void ChatModeService::resume_monitor(const QString& monitor_id, VoidCallback cb) {
    put("/chat/agent/monitors/" + monitor_id + "/resume", {},
        [cb = std::move(cb)](bool ok, QJsonDocument, QString err) { cb(ok, err); });
}

// ── SSE streaming ─────────────────────────────────────────────────────────────

QNetworkReply* ChatModeService::stream_message(const QString& message, const QString& session_id, StreamMode mode,
                                               const QString& source, bool auto_approve, int profile_id) {
    LOG_INFO("ChatModeService", QString("Streaming message to session %1 [%2]: \"%3\"")
                                    .arg(session_id)
                                    .arg(mode == StreamMode::Deep ? "deep" : "lite")
                                    .arg(message.left(60)));

    // Abort any previous stream
    abort_stream();

    QJsonObject body;
    body["message"] = message;
    body["session_id"] = session_id;
    body["mode"] = (mode == StreamMode::Deep) ? "deep" : "lite";
    body["auto_approve"] = auto_approve;
    body["profile_id"] = profile_id;
    if (!source.isEmpty())
        body["source"] = source;
    else
        body["source"] = QJsonValue::Null;

    QNetworkRequest req = build_request("/chat/agent/stream");
    // Accept SSE
    req.setRawHeader("Accept", "text/event-stream");
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);

    sse_reply_ = sse_nam_->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    sse_current_event_.clear();

    connect(sse_reply_, &QNetworkReply::readyRead, this, [this]() {
        while (sse_reply_ && sse_reply_->canReadLine()) {
            const QByteArray line = sse_reply_->readLine().trimmed();
            handle_sse_line(line);
        }
    });

    connect(sse_reply_, &QNetworkReply::finished, this, [this]() {
        if (!sse_reply_)
            return;
        const int status = sse_reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto net_err = sse_reply_->error();
        LOG_INFO("ChatModeService", QString("SSE stream finished — HTTP %1, net_err=%2").arg(status).arg(net_err));
        if (status == 402)
            emit insufficient_credits();
        if (net_err != QNetworkReply::NoError && net_err != QNetworkReply::OperationCanceledError) {
            const QString body = QString::fromUtf8(sse_reply_->readAll().left(200));
            LOG_WARN("ChatModeService", QString("SSE error body: %1").arg(body));
            emit stream_error(QString("Stream failed (HTTP %1)").arg(status));
        }
        sse_reply_->deleteLater();
        sse_reply_ = nullptr;
    });

    return sse_reply_;
}

void ChatModeService::abort_stream() {
    if (sse_reply_) {
        sse_reply_->abort();
        sse_reply_->deleteLater();
        sse_reply_ = nullptr;
    }
}

void ChatModeService::handle_sse_line(const QByteArray& line) {
    if (line.startsWith("event:")) {
        sse_current_event_ = QString::fromUtf8(line.mid(6).trimmed());
        return;
    }

    if (line.startsWith("data:")) {
        const QByteArray json_bytes = line.mid(5).trimmed();
        QJsonParseError pe;
        const QJsonDocument doc = QJsonDocument::fromJson(json_bytes, &pe);
        if (pe.error != QJsonParseError::NoError)
            return;

        const QJsonObject data = doc.object();
        const QString& ev = sse_current_event_;

        if (ev == "session-meta") {
            const QString sid = data["session_id"].toString();
            const QString title = data["new_title"].toString();
            LOG_INFO("ChatModeService", QString("SSE session-meta: id=%1 title=%2").arg(sid, title));
            emit stream_session_meta(sid, title);
        } else if (ev == "text-delta") {
            emit stream_text_delta(data["text"].toString());
        } else if (ev == "tool-end") {
            const QString tool = data["tool"].toString();
            const int dur = data["durationMs"].toInt();
            LOG_DEBUG("ChatModeService", QString("SSE tool-end: %1 (%2ms)").arg(tool).arg(dur));
            emit stream_tool_end(tool, dur);
        } else if (ev == "step-start") {
            const int step = data["stepNumber"].toInt();
            LOG_DEBUG("ChatModeService", QString("SSE step-start: step %1").arg(step));
            emit stream_step_start(step);
        } else if (ev == "step-finish") {
            LOG_DEBUG("ChatModeService", QString("SSE step-finish: %1 tokens").arg(data["tokensUsed"].toInt()));
            emit stream_step_finish(data["tokensUsed"].toInt());
        } else if (ev == "thinking") {
            const QString content = data["content"].toString();
            LOG_DEBUG("ChatModeService", QString("SSE thinking: %1 chars").arg(content.length()));
            emit stream_thinking(content);
        } else if (ev == "finish") {
            const int total = data["totalTokens"].toInt();
            LOG_INFO("ChatModeService", QString("SSE finish: %1 total tokens").arg(total));
            emit stream_finish(total);
        } else if (ev == "heartbeat") {
            LOG_DEBUG("ChatModeService", "SSE heartbeat");
            emit stream_heartbeat();
        } else if (ev == "error") {
            const QString msg = data["message"].toString();
            LOG_WARN("ChatModeService", "SSE error: " + msg);
            emit stream_error(msg);
        } else if (!ev.isEmpty()) {
            LOG_DEBUG("ChatModeService", "SSE unknown event: " + ev);
        }

        sse_current_event_.clear();
    }
}

} // namespace fincept::chat_mode
