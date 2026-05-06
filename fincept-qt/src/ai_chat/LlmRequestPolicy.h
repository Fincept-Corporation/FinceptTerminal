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

#include "ai_chat/LlmService.h"
#include "mcp/McpTypes.h"

#include <QString>

namespace fincept::ai_chat::detail {

inline thread_local LlmService::ToolPolicy t_request_policy = LlmService::ToolPolicy::All;

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
