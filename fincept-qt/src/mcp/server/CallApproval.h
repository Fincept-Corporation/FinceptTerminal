#pragma once
// CallApproval.h — thread-local marker for "the user explicitly approved THIS tool
// call in the terminal UI".
//
// Tools whose auth level is ExplicitConfirm (and live-trading tools requested by
// external agents) must never run on an agent's say-so alone. When the user
// approves such a call in ToolApprovalDialog, the dispatcher wraps the
// execute_tool() call in a ScopedApprovedCall on the same worker thread. The
// McpProvider authorization checker (installed by AgentService) then sees
// `ScopedApprovedCall::active()` and lets that one call through.
//
// The marker is thread-local and RAII-scoped, so it can never leak into another
// call: McpProvider performs its authorization check synchronously on the thread
// that calls execute_tool(), before any hop to another thread.

namespace fincept::mcp {

class ScopedApprovedCall {
  public:
    ScopedApprovedCall() : prev_(flag()) { flag() = true; }
    ~ScopedApprovedCall() { flag() = prev_; }

    ScopedApprovedCall(const ScopedApprovedCall&) = delete;
    ScopedApprovedCall& operator=(const ScopedApprovedCall&) = delete;

    /// True while the current thread is inside a user-approved tool call.
    static bool active() { return flag(); }

  private:
    static bool& flag() {
        thread_local bool approved = false;
        return approved;
    }
    bool prev_;
};

} // namespace fincept::mcp
