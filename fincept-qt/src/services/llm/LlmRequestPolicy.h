#pragma once
// LlmRequestPolicy.h — internal header shared across LlmService TUs.
//
// `t_request_policy` is a thread_local override scoping per-request tool
// behaviour. Set by chat() / chat_streaming() workers via ToolPolicyGuard
// before invoking the request-builder helpers; restored on scope exit.
//
// Replaces the legacy pattern of mutating tools_enabled_ on the shared
// instance, which raced when the floating AiChatBubble and the AI Chat tab
// ran concurrently and the bubble's "restore" leaked tools=false back to
// the tab.
//
//   ToolPolicy::All           — attach the global tool catalog as-is.
//   ToolPolicy::NoNavigation  — attach tools but exclude `navigation` (floating
//                               bubble: model can call benign tools without
//                               redirecting the user's active screen).
//   ToolPolicy::None          — attach no tools at all (legacy use_tools=false).

#include "services/llm/LlmService.h"
#include "mcp/McpTypes.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <functional>

namespace fincept::ai_chat::detail {

inline thread_local LlmService::ToolPolicy t_request_policy = LlmService::ToolPolicy::All;

// Optional progress emitter — when set, the tool loop pushes per-round status
// ("Using tool X...") through this so the chat bubble paints in real-time
// during the otherwise-silent multi-second tool execution phase. Set by
// do_streaming_request before falling back into the tool loop, cleared on exit.
using ProgressEmitter = std::function<void(const QString&)>;
inline thread_local ProgressEmitter t_progress_emitter;

struct ProgressEmitterGuard {
    ProgressEmitter prev;
    explicit ProgressEmitterGuard(ProgressEmitter p) : prev(std::move(t_progress_emitter)) {
        t_progress_emitter = std::move(p);
    }
    ~ProgressEmitterGuard() { t_progress_emitter = std::move(prev); }
    ProgressEmitterGuard(const ProgressEmitterGuard&) = delete;
    ProgressEmitterGuard& operator=(const ProgressEmitterGuard&) = delete;
};

inline void emit_progress(const QString& text) {
    if (t_progress_emitter)
        t_progress_emitter(text);
}

// Active chat session id for the in-flight LLM request. Set by the chat screen
// before calling chat_streaming(), read by MCP tools (e.g. report_session_context)
// to scope per-chat state — for example, "did this chat session already start a
// report?" so the model knows whether to continue or start fresh.
inline thread_local QString t_chat_session_id;

struct ChatSessionGuard {
    QString prev;
    explicit ChatSessionGuard(QString id) : prev(std::move(t_chat_session_id)) {
        t_chat_session_id = std::move(id);
    }
    ~ChatSessionGuard() { t_chat_session_id = std::move(prev); }
    ChatSessionGuard(const ChatSessionGuard&) = delete;
    ChatSessionGuard& operator=(const ChatSessionGuard&) = delete;
};

struct ToolPolicyGuard {
    LlmService::ToolPolicy prev;
    explicit ToolPolicyGuard(LlmService::ToolPolicy p) : prev(t_request_policy) { t_request_policy = p; }
    ~ToolPolicyGuard() { t_request_policy = prev; }
    ToolPolicyGuard(const ToolPolicyGuard&) = delete;
    ToolPolicyGuard& operator=(const ToolPolicyGuard&) = delete;
};

inline bool effective_tools_enabled(bool global_tools_enabled) {
    return global_tools_enabled && t_request_policy != LlmService::ToolPolicy::None;
}

inline bool should_hide_navigation() {
    return t_request_policy == LlmService::ToolPolicy::NoNavigation;
}

inline mcp::ToolFilter apply_request_policy(const mcp::ToolFilter& base) {
    mcp::ToolFilter out = base;
    if (should_hide_navigation() && !out.exclude_categories.contains(QStringLiteral("navigation")))
        out.exclude_categories.append(QStringLiteral("navigation"));
    return out;
}

// True for OpenAI-family chat model ids (gpt-*, chatgpt-*, o1/o3/o4 reasoning
// series). Used by pass-through aggregators (AIHubMix) that forward parameters
// unchanged: these models reject the legacy `max_tokens` field and require
// `max_completion_tokens` (o-series / gpt-5 return a 400 otherwise), whereas
// non-OpenAI models (claude-*, gemini-*, deepseek-*, qwen-*, …) keep max_tokens.
// `model_lower` must already be lower-cased by the caller.
inline bool is_openai_family_model(const QString& model_lower) {
    return model_lower.startsWith(QLatin1String("gpt-")) ||
           model_lower.startsWith(QLatin1String("chatgpt")) ||
           model_lower.startsWith(QLatin1String("o1")) ||
           model_lower.startsWith(QLatin1String("o3")) ||
           model_lower.startsWith(QLatin1String("o4"));
}

// ── Tool RAG activation ──────────────────────────────────────────────────
//
// When Tool RAG is on, the model is only sent the ~7 Tier-0 tools each turn and
// discovers the rest via tool_list / tool_describe. Those meta-tools hand back
// the discovered tool as *text* — but a structured function-calling model
// (Kimi, OpenAI, Groq, …) can only emit a tool_call for a function actually
// DECLARED in the request's `tools` array. Without re-declaring what it found,
// the model stalls right after tool_describe ("I don't have that tool loaded"),
// which is exactly the failure mode that breaks agentic actions like
// create_portfolio.
//
// This mirrors Anthropic's Tool Search Tool: the discovered tool definition
// must be injected into the active tool set. We accumulate the bare names the
// model surfaces (via tool_list) or commits to (via tool_describe) across the
// turn, and feed them back into format_tools_for_openai so they become real,
// callable functions on the next round.
inline void note_tool_activations(const QString& bare_tool_name,
                                  const QJsonObject& args,
                                  const mcp::ToolResult& result,
                                  QSet<QString>& activated) {
    if (!result.success)
        return;
    if (bare_tool_name == QLatin1String("tool_list")) {
        // Activate every candidate the search surfaced — the model may pick any.
        const QJsonArray rows = result.data.toObject().value("tools").toArray();
        for (const auto& row : rows) {
            const QString name = row.toObject().value("name").toString();
            if (!name.isEmpty())
                activated.insert(name);
        }
    } else if (bare_tool_name == QLatin1String("tool_describe")) {
        // The model committed to one tool — prefer the canonical name echoed in
        // the result, fall back to the name it asked about.
        QString name = result.data.toObject().value("name").toString();
        if (name.isEmpty())
            name = args.value("name").toString();
        if (!name.isEmpty())
            activated.insert(name);
    }
}

} // namespace fincept::ai_chat::detail
