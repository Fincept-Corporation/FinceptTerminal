#pragma once
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>
#include <QString>
#include <QTimeZone>
#include <QVector>

namespace fincept::chat_mode {

// ── Stream mode ───────────────────────────────────────────────────────────────
enum class StreamMode { Lite, Deep };

// ── Default session title ─────────────────────────────────────────────────────
// The title a new conversation is created with. Sent untranslated: the backend
// auto-titles a conversation from its first message only while the stored title
// contains this exact English text (apiback chat.py _persist_user_message), so a
// translated title ("Neue Unterhaltung") kept the placeholder forever. Translate
// it only for display — see chat_display_session_title().
inline constexpr const char* kChatDefaultSessionTitle = "New Conversation";

/// A session title as shown to the user: the canonical default title in the UI
/// language (the ChatMessagePanel translation is reused), anything else on one line
/// (a title the server cut from a message of several lines must not grow the header).
inline QString chat_display_session_title(const QString& title) {
    if (title == QLatin1String(kChatDefaultSessionTitle))
        return QCoreApplication::translate("fincept::chat_mode::ChatMessagePanel", "New Conversation");
    return title.simplified();
}

/// A server timestamp (ISO 8601, UTC; with or without fraction / offset) in the
/// user's local time and short locale format, or "-" when empty / unparseable.
/// The raw text used to be shown ("2026-11-16T09:41", UTC, with the literal T).
inline QString chat_local_time(const QString& iso) {
    const QString s = iso.trimmed();
    if (s.isEmpty())
        return QStringLiteral("-");
    QDateTime dt = QDateTime::fromString(s, Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = QDateTime::fromString(s, Qt::ISODate);
    if (!dt.isValid()) {
        // Python isoformat() writes microseconds (6 digits); keep milliseconds.
        static const QRegularExpression kMicros(QStringLiteral("(\\.\\d{3})\\d+"));
        QString trimmed = s;
        trimmed.replace(kMicros, QStringLiteral("\\1"));
        dt = QDateTime::fromString(trimmed, Qt::ISODateWithMs);
    }
    if (!dt.isValid())
        return QStringLiteral("-");
    if (dt.timeSpec() == Qt::LocalTime) // no offset in the text: the server writes UTC
        dt.setTimeZone(QTimeZone::utc());
    return QLocale().toString(dt.toLocalTime(), QLocale::ShortFormat);
}

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

/// One page of GET /chat/sessions. `has_more_known` is false on servers that do
/// not page (no `has_more` in the reply): the caller then grows `limit` instead.
struct ChatSessionsPage {
    QVector<ChatSession> sessions;
    bool has_more_known = false;
    bool has_more = false;
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
    QString session_uuid; // owning session — present on /chat/search hits
    /// metadata_json of an assistant message (apiback chat.py): what the user saw
    /// besides text — {"interrupt": {"question"}}, {"proposals": [{…, "status"}]},
    /// {"proposal_decision": {…}}. Empty for plain answers and on older servers.
    QJsonObject metadata;

    /// The question the agent paused on in this turn, or empty.
    QString interrupt_question() const {
        return metadata.value(QLatin1String("interrupt")).toObject().value(QLatin1String("question")).toString().trimmed();
    }
    /// Trade proposals shown in this turn, each with its "status" (pending until decided).
    QJsonArray proposals() const { return metadata.value(QLatin1String("proposals")).toArray(); }

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
        m.metadata = o["metadata_json"].toObject(); // null / absent -> empty
        return m;
    }
};

// ── Trade proposal decision (POST /chat/agent/proposals/{id}) ─────────────────
// Also the payload of the "proposal-decided" stream event (a decision made during
// a run, e.g. the agent's portfolio_confirm after an "Approved: …" message):
// {proposal_id, status, approved, message, decided_by, decided_at}.
// Card statuses in history (metadata_json.proposals[].status):
// pending | executed | rejected | failed | expired | unavailable.
struct ProposalDecision {
    QString proposal_id;
    QString status; // "executed" | "rejected" | "failed"
    bool approved = false;
    QString message; // one readable line written by the server
    QJsonObject proposal;
    QString decided_at;
    QString decided_by;
    bool replayed = false; // an earlier decision's outcome (never executed twice)
    QString session_id;

    static ProposalDecision from_json(const QJsonObject& o) {
        ProposalDecision d;
        d.proposal_id = o["proposal_id"].toString();
        d.status = o["status"].toString();
        d.approved = o["approved"].toBool();
        d.message = o["message"].toString().trimmed();
        d.proposal = o["proposal"].toObject();
        d.decided_at = o["decided_at"].toString();
        d.decided_by = o["decided_by"].toString();
        d.replayed = o["replayed"].toBool();
        d.session_id = o["session_id"].toString();
        return d;
    }
};

/// How a decision request ended. RouteMissing = the server has no proposals
/// route yet (an older deployment): the caller falls back to the text approval.
enum class ProposalOutcome { Decided, Gone, Expired, InProgress, RouteMissing, Failed };

// ── Terminal tool bridge ──────────────────────────────────────────────────────
struct RejectedTool {
    QString name;
    QString reason;
};

/// Reply of POST /chat/agent/terminal-tools/register.
struct ToolRegistration {
    int registered = 0;           // tools the server accepted
    bool visibility_known = false; // the reply carries agent_visible (newer servers)
    bool agent_visible = false;   // stored AND the agent process is alive
    QVector<RejectedTool> rejected;
    int ttl_seconds = 0; // 0 = not reported

    static ToolRegistration from_json(const QJsonObject& o) {
        ToolRegistration r;
        r.registered = o["registered"].toInt();
        r.visibility_known = o.contains(QLatin1String("agent_visible"));
        r.agent_visible = o["agent_visible"].toBool();
        for (const auto& v : o["rejected"].toArray()) {
            const QJsonObject e = v.toObject();
            r.rejected.append({e["name"].toString(), e["reason"].toString()});
        }
        r.ttl_seconds = o["ttl_seconds"].toInt();
        return r;
    }
};

/// Reply of GET /chat/agent/terminal-tools/pending.
///   available: false                   -> the server's bridge is down: do not re-register
///   available: true, registered: false -> the registration lapsed: register again
/// Older servers send neither `available` nor `registered`.
struct PendingToolCalls {
    QJsonArray calls;
    bool registered_known = false; // the reply carries `registered` (newer servers)
    bool registered = true;
    bool visibility_known = false;
    bool agent_visible = false;
    bool available_known = false; // the reply carries `available` (newest servers)
    bool available = true;

    static PendingToolCalls from_json(const QJsonObject& o) {
        PendingToolCalls p;
        p.calls = o["calls"].toArray();
        p.registered_known = o.contains(QLatin1String("registered"));
        p.registered = o["registered"].toBool(true);
        p.visibility_known = o.contains(QLatin1String("agent_visible"));
        p.agent_visible = o["agent_visible"].toBool();
        p.available_known = o.contains(QLatin1String("available"));
        p.available = o["available"].toBool(true);
        return p;
    }
};

/// Outcome of POST /chat/agent/terminal-tools/result.
enum class ToolResultDelivery {
    Delivered,
    AlreadyDelivered, // 409: a result for this call is already in
    Expired,          // 404: the call is unknown / expired — the agent stopped waiting
    Failed
};

/// What the chat header shows about the terminal tools offered to the cloud agent.
struct ToolBridgeStatus {
    enum class State {
        Off,              // nothing registered (yet) — no label
        Unconfirmed,      // older server: accepted, but it cannot say whether the agent sees them
        Usable,           // the agent can call them
        AgentUnreachable, // registered, but the agent service is not running
        NotRegistered,    // the registration failed, lapsed, or no tool was accepted
        Unavailable       // the server's tool bridge is temporarily down (pending available:false,
                          // 503 terminal_bridge_unavailable): nothing to do but wait
    };
    State state = State::Off;
    int offered = 0; // tools in the last accepted registration
    QVector<RejectedTool> rejected;
    QString detail; // why NotRegistered (server text), may be empty
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

// ── Agent investment profile (GET /profile/profiles) ──────────────────────────
struct AgentProfile {
    int id = 0;
    QString name;
    bool is_default = false;

    static AgentProfile from_json(const QJsonObject& o) {
        AgentProfile p;
        p.id = o["id"].toInt();
        p.name = o["name"].toString();
        p.is_default = o["is_default"].toBool();
        return p;
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
    QString transport; // "sse" | "streamable_http" (private); global servers may also be "stdio"
    QString status;    // "connected" | "disconnected" | "error"
    QString error;     // backend's connection error (only sent for your own / admin servers)
    int tools_count = 0;
    QStringList tool_names;
    QVector<McpTool> tools;
    QJsonObject config;
    QString scope; // "global" (shared, admin-managed — the user cannot remove it) | "private"

    bool is_shared() const { return scope == QLatin1String("global"); }

    static McpServer from_json(const QJsonObject& o) {
        McpServer s;
        s.name = o["name"].toString();
        s.transport = o["transport"].toString();
        s.status = o["status"].toString();
        s.error = o["error"].toString();
        s.scope = o["scope"].toString();
        s.tools_count = o["tools_count"].toInt();
        s.config = o["config"].toObject();
        for (const auto& v : o["tool_names"].toArray())
            s.tool_names.append(v.toString());
        for (const auto& v : o["tools"].toArray())
            s.tools.append(McpTool::from_json(v.toObject()));
        return s;
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
