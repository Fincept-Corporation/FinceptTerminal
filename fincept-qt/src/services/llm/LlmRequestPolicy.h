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

} // namespace fincept::ai_chat::detail
