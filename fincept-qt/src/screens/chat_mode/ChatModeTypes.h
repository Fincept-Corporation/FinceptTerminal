#pragma once
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace fincept::chat_mode {

// ── Stream mode ───────────────────────────────────────────────────────────────
enum class StreamMode { Lite, Deep };

// ── Session ───────────────────────────────────────────────────────────────────
struct ChatSession {
    QString  uuid;
    QString  title;
    bool     is_active    = false;
    int      message_count = 0;
    QString  created_at;
    QString  updated_at;
    QString  last_message_at;

    static ChatSession from_json(const QJsonObject& o) {
        ChatSession s;
        s.uuid          = o["session_uuid"].toString();
        s.title         = o["title"].toString();
        s.is_active     = o["is_active"].toBool();
        s.message_count = o["message_count"].toInt();
        s.created_at    = o["created_at"].toString();
        s.updated_at    = o["updated_at"].toString();
        s.last_message_at = o["last_message_at"].toString();
        return s;
    }
};

// ── Message ───────────────────────────────────────────────────────────────────
struct ChatMessage {
    QString  uuid;
    QString  role;      // "user" | "assistant"
    QString  content;
    QString  created_at;
    int      tokens_used    = 0;
    int      response_time_ms = 0;

    static ChatMessage from_json(const QJsonObject& o) {
        ChatMessage m;
        m.uuid             = o["message_uuid"].toString();
        m.role             = o["role"].toString();
        m.content          = o["content"].toString();
        m.created_at       = o["created_at"].toString();
        m.tokens_used      = o["tokens_used"].toInt();
        m.response_time_ms = o["response_time_ms"].toInt();
        return m;
    }
};

// ── Agent memory entry ────────────────────────────────────────────────────────
struct AgentMemory {
    QString key;
    QString value;
    QString memory_type;   // "fact" | "preference" | "entity"
    QString created_at;

    static AgentMemory from_json(const QJsonObject& o) {
        AgentMemory m;
        m.key         = o["key"].toString();
        m.value       = o["value"].toString();
        m.memory_type = o["memory_type"].toString();
        m.created_at  = o["created_at"].toString();
        return m;
    }
};

// ── Agent schedule ────────────────────────────────────────────────────────────
struct AgentSchedule {
    QString schedule_id;
    QString query;
    QString cron_expression;
    QString status;     // "active" | "paused"
    QString session_id;
    QString created_at;

    static AgentSchedule from_json(const QJsonObject& o) {
        AgentSchedule s;
        s.schedule_id     = o["schedule_id"].toString();
        s.query           = o["query"].toString();
        s.cron_expression = o["cron_expression"].toString();
        s.status          = o["status"].toString();
        s.session_id      = o["session_id"].toString();
        s.created_at      = o["created_at"].toString();
        return s;
    }
};

// ── Background agent task ─────────────────────────────────────────────────────
struct AgentTask {
    QString task_id;
    QString query;
    QString status;     // "queued" | "running" | "complete" | "error" | "cancelled"
    QString session_id;
    QString result;
    QString created_at;
    QString updated_at;

    static AgentTask from_json(const QJsonObject& o) {
        AgentTask t;
        t.task_id    = o["task_id"].toString();
        t.query      = o["query"].toString();
        t.status     = o["status"].toString();
        t.session_id = o["session_id"].toString();
        t.result     = o["result"].toString();
        t.created_at = o["created_at"].toString();
        t.updated_at = o["updated_at"].toString();
        return t;
    }
};

// ── Chat stats ────────────────────────────────────────────────────────────────
struct ChatStats {
    int total_sessions  = 0;
    int total_messages  = 0;
    int active_sessions = 0;

    static ChatStats from_json(const QJsonObject& o) {
        ChatStats s;
        s.total_sessions  = o["total_sessions"].toInt();
        s.total_messages  = o["total_messages"].toInt();
        s.active_sessions = o["active_sessions"].toInt();
        return s;
    }
};

} // namespace fincept::chat_mode
