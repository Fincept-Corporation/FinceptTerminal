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
    QString uuid;
    QString title;
    bool is_active = false;
    int message_count = 0;
    QString created_at;
    QString updated_at;
    QString last_message_at;

    static ChatSession from_json(const QJsonObject& o) {
        ChatSession s;
        s.uuid = o["session_uuid"].toString();
        s.title = o["title"].toString();
        s.is_active = o["is_active"].toBool();
        s.message_count = o["message_count"].toInt();
        s.created_at = o["created_at"].toString();
        s.updated_at = o["updated_at"].toString();
        s.last_message_at = o["last_message_at"].toString();
        return s;
    }
};

// ── Message ───────────────────────────────────────────────────────────────────
struct ChatMessage {
    QString uuid;
    QString role; // "user" | "assistant"
    QString content;
    QString created_at;
    int tokens_used = 0;
    int response_time_ms = 0;
    QString provider;
    QString model;

    static ChatMessage from_json(const QJsonObject& o) {
        ChatMessage m;
        m.uuid = o["message_uuid"].toString();
        m.role = o["role"].toString();
        m.content = o["content"].toString();
        m.created_at = o["created_at"].toString();
        m.tokens_used = o["tokens_used"].toInt();
        m.response_time_ms = o["response_time_ms"].toInt();
        m.provider = o["provider"].toString();
        m.model = o["model"].toString();
        return m;
    }
};

// ── Agent memory entry ────────────────────────────────────────────────────────
struct AgentMemory {
    QString key;
    QString value;
    QString memory_type; // "fact" | "preference" | "entity"
    QString created_at;

    static AgentMemory from_json(const QJsonObject& o) {
        AgentMemory m;
        m.key = o["key"].toString();
        m.value = o["value"].toString();
        m.memory_type = o["memory_type"].toString();
        m.created_at = o["created_at"].toString();
        return m;
    }
};

// ── Agent schedule ────────────────────────────────────────────────────────────
struct AgentSchedule {
    QString schedule_id;
    QString query;
    QString cron_expression;
    QString status; // "active" | "paused"
    QString session_id;
    QString created_at;
    QString last_run_at;
    QString next_run_at;

    static AgentSchedule from_json(const QJsonObject& o) {
        AgentSchedule s;
        s.schedule_id = o["schedule_id"].toString();
        s.query = o["query"].toString();
        s.cron_expression = o["cron_expression"].toString();
        s.status = o["status"].toString();
        s.session_id = o["session_id"].toString();
        s.created_at = o["created_at"].toString();
        s.last_run_at = o["last_run_at"].toString();
        s.next_run_at = o["next_run_at"].toString();
        return s;
    }
};

// ── Background agent task ─────────────────────────────────────────────────────
struct AgentTask {
    QString task_id;
    QString query;
    QString status; // "queued" | "running" | "completed" | "error" | "cancelled"
    QString session_id;
    QString result;
    QString created_at;
    QString updated_at;
    QString started_at;
    QString completed_at;

    static AgentTask from_json(const QJsonObject& o) {
        AgentTask t;
        t.task_id = o["task_id"].toString();
        t.query = o["query"].toString();
        t.status = o["status"].toString();
        t.session_id = o["session_id"].toString();
        t.result = o["result"].toString();
        t.created_at = o["created_at"].toString();
        t.updated_at = o["updated_at"].toString();
        t.started_at = o["started_at"].toString();
        t.completed_at = o["completed_at"].toString();
        return t;
    }
};

// ── Task history step ─────────────────────────────────────────────────────────
struct TaskHistoryStep {
    int step = 0;
    QString timestamp;
    QStringList next;

    static TaskHistoryStep from_json(const QJsonObject& o) {
        TaskHistoryStep h;
        h.step = o["step"].toInt();
        h.timestamp = o["timestamp"].toString();
        for (const auto& v : o["next"].toArray())
            h.next.append(v.toString());
        return h;
    }
};

// ── Task activity event ───────────────────────────────────────────────────────
struct TaskActivity {
    int id = 0;
    QString event_type;
    QString timestamp;
    QJsonObject details;
    QJsonObject data;

    static TaskActivity from_json(const QJsonObject& o) {
        TaskActivity a;
        a.id = o["id"].toInt();
        a.event_type = o["event_type"].toString();
        a.timestamp = o["timestamp"].toString();
        a.details = o["details"].toObject();
        a.data = o["data"].toObject();
        return a;
    }
};

// ── Chat stats ────────────────────────────────────────────────────────────────
struct ChatStats {
    int total_sessions = 0;
    int total_messages = 0;
    int active_sessions = 0;

    static ChatStats from_json(const QJsonObject& o) {
        ChatStats s;
        s.total_sessions = o["total_sessions"].toInt();
        s.total_messages = o["total_messages"].toInt();
        s.active_sessions = o["active_sessions"].toInt();
        return s;
    }
};

// ── MCP tool ──────────────────────────────────────────────────────────────────
struct McpTool {
    QString name;
    QString description;
    QJsonObject input_schema;

    static McpTool from_json(const QJsonObject& o) {
        McpTool t;
        t.name = o["name"].toString();
        t.description = o["description"].toString();
        t.input_schema = o["input_schema"].toObject();
        return t;
    }
};

// ── MCP server ────────────────────────────────────────────────────────────────
struct McpServer {
    QString name;
    QString transport; // "stdio" | "sse"
    QString status;    // "connected" | "disconnected" | "error"
    int tools_count = 0;
    QStringList tool_names;
    QVector<McpTool> tools;
    QJsonObject config;

    static McpServer from_json(const QJsonObject& o) {
        McpServer s;
        s.name = o["name"].toString();
        s.transport = o["transport"].toString();
        s.status = o["status"].toString();
        s.tools_count = o["tools_count"].toInt();
        s.config = o["config"].toObject();
        for (const auto& v : o["tool_names"].toArray())
            s.tool_names.append(v.toString());
        for (const auto& v : o["tools"].toArray())
            s.tools.append(McpTool::from_json(v.toObject()));
        return s;
    }
};

// ── Agent monitor ─────────────────────────────────────────────────────────────
struct AgentMonitor {
    QString monitor_id;
    QString name;
    QString source_type; // "equity" | "macro" | "news"
    QJsonObject source_config;
    QJsonObject trigger_config;
    QString analysis_query;
    int check_interval_seconds = 300;
    QString status; // "active" | "paused"
    QString created_at;
    QString last_check_at;
    QString last_triggered_at;

    static AgentMonitor from_json(const QJsonObject& o) {
        AgentMonitor m;
        m.monitor_id = o["monitor_id"].toString();
        m.name = o["name"].toString();
        m.source_type = o["source_type"].toString();
        m.source_config = o["source_config"].toObject();
        m.trigger_config = o["trigger_config"].toObject();
        m.analysis_query = o["analysis_query"].toString();
        m.check_interval_seconds = o["check_interval_seconds"].toInt(300);
        m.status = o["status"].toString();
        m.created_at = o["created_at"].toString();
        m.last_check_at = o["last_check_at"].toString();
        m.last_triggered_at = o["last_triggered_at"].toString();
        return m;
    }
};

// ── Optimized prompt result ───────────────────────────────────────────────────
struct OptimizedPrompt {
    QString optimized;
    QString original;

    static OptimizedPrompt from_json(const QJsonObject& o) {
        OptimizedPrompt p;
        p.optimized = o["optimized"].toString();
        p.original = o["original"].toString();
        return p;
    }
};

// ── Agent chat (non-streaming) response ───────────────────────────────────────
struct AgentChatResponse {
    QString session_id;
    QString thread_id;
    QString response;
    int thinking_steps = 0;
    int tool_calls = 0;
    int total_tokens = 0;
    QString status;
    QString new_title;
    QJsonArray tool_calls_log;

    static AgentChatResponse from_json(const QJsonObject& o) {
        AgentChatResponse r;
        r.session_id = o["session_id"].toString();
        r.thread_id = o["thread_id"].toString();
        r.response = o["response"].toString();
        r.thinking_steps = o["thinking_steps"].toInt();
        r.tool_calls = o["tool_calls"].toInt();
        r.total_tokens = o["total_tokens"].toInt();
        r.status = o["status"].toString();
        r.new_title = o["new_title"].toString();
        r.tool_calls_log = o["tool_calls_log"].toArray();
        return r;
    }
};

} // namespace fincept::chat_mode
