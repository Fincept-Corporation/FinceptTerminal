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

#include <QString>

namespace fincept::mcp {

/// Thread-local label naming WHO is calling a tool ("claude-code", "AI Chat",
/// "agent: Macro Analyst"), shown in approval dialogs and activity logs. Set by
/// each dispatcher (McpHttpServer, the agent bridge, the chat tool loop) around
/// the synchronous part of a tool call; a tool handler reads it at entry, on the
/// calling thread, before it hops anywhere else.
class ScopedCallOrigin {
  public:
    explicit ScopedCallOrigin(const QString& origin) : prev_(slot()) { slot() = origin; }
    ~ScopedCallOrigin() { slot() = prev_; }

    ScopedCallOrigin(const ScopedCallOrigin&) = delete;
    ScopedCallOrigin& operator=(const ScopedCallOrigin&) = delete;

    /// The current thread's caller label, or "AI assistant" when none was set.
    static QString current() {
        const QString& s = slot();
        return s.isEmpty() ? QStringLiteral("AI assistant") : s;
    }

  private:
    static QString& slot() {
        thread_local QString origin;
        return origin;
    }
    QString prev_;
};

/// Thread-local STABLE identity of the caller, next to the free-text label of
/// ScopedCallOrigin: the client profile id the MCP server identified
/// ("claude_code", "codex", "gemini", "qwen", "cursor", "copilot", "grok",
/// "claude_desktop", "vscode", or "external" for an unknown client;
/// McpClientProfile.h actor_id) and the run it belongs to (a run-scoped token's
/// run id; empty outside a run). Set by the dispatchers around the synchronous
/// part of a tool call, read on the calling thread at handler entry and carried
/// to the GUI thread by mcp/core/UiDispatch.h — the order path stamps what an
/// agent causes as "agent:<client id>" (core/agent/AgentActionScope.h).
class ScopedCallClient {
  public:
    /// The terminal's own agents (not MCP clients): the in-app AI assistant
    /// (TerminalMcpBridge) and the Fincept cloud agent runs (TerminalToolBridge).
    static constexpr char kTerminalAgent[] = "terminal_agent";
    static constexpr char kFinceptAgent[] = "fincept_agent";


    ScopedCallClient(const QString& client_id, const QString& run_id)
        : prev_client_(client_slot()), prev_run_(run_slot()) {
        client_slot() = client_id;
        run_slot() = run_id;
    }
    ~ScopedCallClient() {
        client_slot() = prev_client_;
        run_slot() = prev_run_;
    }

    ScopedCallClient(const ScopedCallClient&) = delete;
    ScopedCallClient& operator=(const ScopedCallClient&) = delete;

    /// The current thread's stable client id; "" when none was set.
    static QString client_id() { return client_slot(); }
    /// The current thread's run id; "" outside a run.
    static QString run_id() { return run_slot(); }

  private:
    static QString& client_slot() {
        thread_local QString client;
        return client;
    }
    static QString& run_slot() {
        thread_local QString run;
        return run;
    }
    QString prev_client_;
    QString prev_run_;
};

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

/// Set by the workflow engine around an MCP node's tool call while the run is in
/// PAPER mode. Nothing such a call does may place, change or cancel a real order or
/// move money: the core tools refuse it outright instead of putting an approval
/// prompt in front of the user, so a PAPER run stays paper whoever is at the screen.
class ScopedPaperRun {
  public:
    ScopedPaperRun() : prev_(flag()) { flag() = true; }
    ~ScopedPaperRun() { flag() = prev_; }

    ScopedPaperRun(const ScopedPaperRun&) = delete;
    ScopedPaperRun& operator=(const ScopedPaperRun&) = delete;

    /// True while the current thread is inside a PAPER-run tool call.
    static bool active() { return flag(); }

  private:
    static bool& flag() {
        thread_local bool paper = false;
        return paper;
    }
    bool prev_;
};

/// Appended to a PAPER run's caller label by the core tools: the label travels with
/// the call onto the GUI thread, where the thread-local flag above is not visible.
inline constexpr const char* kPaperRunOriginSuffix = " (paper run)";

inline bool is_paper_run_origin(const QString& origin) {
    return origin.endsWith(QLatin1String(kPaperRunOriginSuffix));
}

} // namespace fincept::mcp
