#pragma once
// HarnessChatTypes.h — value types of a terminal-AI call answered by an agent
// harness (Claude Code, Codex): AI Chat, the Quick Chat bubble, Quick Ask, the
// workflow tool picker, Alpha Arena decisions and Chat Mode tasks.
//
// QtCore only (no widgets, logger, settings or network) so the process runner
// (HarnessChatRun.cpp) can be unit-tested against a fake CLI.
//
// A call is one harness process: a fresh empty working folder, no built-in
// tools, read-only, and — when tools are allowed — the terminal's own MCP
// server through a run-scoped bearer token that dies with the call.

#include "services/coding_agents/CodingAgentTypes.h"
#include "services/harness/HarnessTypes.h"

#include <QString>
#include <QVector>

#include <functional>
#include <optional>

namespace fincept::harness {

/// The terminal tools a call may use (mirrors ai_chat::LlmService::ToolPolicy).
/// NoNavigation hides the tools that move the user to another screen
/// (ui_open, ui_workspace) — enforced by the MCP server for the call's token.
enum class ChatTools { None, NoNavigation, All };

struct ChatTurn {
    QString role; // system | user | assistant
    QString content;
};

struct HarnessChatRequest {
    QString harness_id;
    QString model;  // empty = the CLI's default
    QString effort; // empty = the CLI's default
    QString system_prompt;
    QString user_message;
    QVector<ChatTurn> history; // oldest first; flattened into the prompt (stateless)
    QString json_schema;       // non-empty = ask for a JSON reply that follows this schema
    QString chat_session_id;   // logs only
    QString feature;           // ModelRouter feature key (logs, the MCP token's label)
    ChatTools tools = ChatTools::None;
    /// MCP calls of this run may never place, change or cancel a real order
    /// (ScopedPaperRun on the server side).
    bool paper_only = false;
    int first_event_timeout_ms = 60 * 1000;
    int timeout_ms = 10 * 60 * 1000;
    /// Claude Code: abort before any output when the CLI reports that it is not
    /// using an API key (a personal subscription sign-in). Off for cloud keys
    /// (Bedrock / Vertex / Foundry), where no Anthropic key is involved.
    bool require_api_key_source = true;
    /// Chat Mode tasks: keep the harness session (resume_session_id continues it).
    bool persist_session = false;
    QString resume_session_id;
};

struct HarnessChatResult {
    bool success = false;
    bool cancelled = false;
    /// Something reached the caller (text, thinking or a tool card). A call
    /// that failed after this point is never answered again by another model.
    bool output_started = false;
    FailureKind failure = FailureKind::None;
    QString error;   // one plain sentence for the user (+ detail)
    QString content; // the answer
    QString model;   // as the harness reported it
    QString session_id;
    qint64 input_tokens = 0;
    qint64 output_tokens = 0;
    qint64 cached_tokens = 0;
    std::optional<double> cost_usd;
    qint64 retry_at_ms = 0;            // rate-limit reset when the harness reported one
    bool subscription_detected = false; // the CLI said it was not using an API key
    int exit_code = -1;
    qint64 duration_ms = 0;
};

/// Streaming callbacks. All are optional and run on the thread that runs the call.
struct HarnessChatSink {
    std::function<void(const QString& delta)> text;
    std::function<void(const QString& delta)> thinking;
    /// The answer text streamed so far was narration before a tool call: a
    /// consumer that shows only the answer drops it.
    std::function<void()> text_reset;
    std::function<void(const QString& call_id, const QString& tool, const QString& args_json)> tool_call;
    std::function<void(const QString& call_id, const QString& tool, bool ok, const QString& body)> tool_result;
    /// Every parsed transcript event, unfiltered (Chat Mode tasks).
    std::function<void(const services::coding_agents::AgentEvent& ev)> event;
    /// Polled about every 100 ms; true = stop the call (process tree killed).
    std::function<bool()> cancelled;
};

/// The bare terminal tool name of a harness tool name: "mcp__fincept__ui_open",
/// "fincept.ui_open", "fincept_terminal.ui_open" → "ui_open".
inline QString chat_tool_name(const QString& name) {
    QString n = name;
    if (n.startsWith(QLatin1String("mcp__"))) {
        const qsizetype sep = n.indexOf(QLatin1String("__"), 5);
        if (sep > 0)
            return n.mid(sep + 2);
    }
    const qsizetype dot = n.lastIndexOf(QLatin1Char('.'));
    return dot >= 0 ? n.mid(dot + 1) : n;
}

} // namespace fincept::harness
