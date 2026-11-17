#pragma once
// AgentActionScope.h — "an AI agent is operating the terminal right now".
//
// Screen code cannot tell whether a person or an agent pressed its button: the
// MCP UI engine (src/mcp/core/UiAct*.cpp) presses it the way a person would.
// While the engine performs an agent's action it marks that on the GUI thread,
// so code that needs to know — the order path stamping UnifiedOrder::source —
// can ask:
//
//     if (order.source.isEmpty())
//         order.source = core::agent::current_agent_order_source(); // "agent:claude_code" or ""
//
// Two ways to mark:
//   * AgentActionScope (RAII) — everything that runs synchronously inside it,
//     at the same event-loop level, is the agent's. Nestable.
//   * AgentActionHold — keeps the agent current across event-loop turns for the
//     deferred work an action triggers (a QTimer::singleShot(0) from a click
//     handler, a reply that lands a moment later) until the engine considers
//     the action settled and releases it. Never leaked: released on
//     destruction, ignored after its deadline (hard cap kHardCapMs), at most
//     kMaxHolds at once.
//
// A person's own input ends the marking: a spontaneous mouse press, key press
// or touch from the window system drops every hold, and a scope opened before
// that input no longer counts — a click a person makes is never an agent's.
// Code running in a nested event loop the action started (a modal dialog's
// exec()) is not the action's synchronous work, so an outer scope does not
// cover it (deferred holds still do, until settled).
//
// GUI THREAD ONLY. Off the GUI thread a scope / hold is inert and
// current_agent_actor() answers nullopt: an order path that hops threads
// before its choke point must capture current_agent_order_source() at the
// screen's submit call. Qt Core only.

#include <QString>

#include <optional>

namespace fincept::core::agent {

struct AgentActor {
    QString client_id; ///< who: a client / harness id ("claude-code", "codex"); free text, sanitised for sources
    QString run_id;    ///< the agent run, when known
};

/// RAII marking of the synchronous work of one agent action.
class AgentActionScope {
  public:
    explicit AgentActionScope(const AgentActor& actor);
    ~AgentActionScope();
    AgentActionScope(const AgentActionScope&) = delete;
    AgentActionScope& operator=(const AgentActionScope&) = delete;

  private:
    quint64 id_ = 0; ///< 0 = inert (off the GUI thread, or nested too deep)
};

/// Keeps `actor` current across event-loop turns until release(), destruction
/// or `max_ms` (clamped to [1, kHardCapMs]) — whichever comes first.
class AgentActionHold {
  public:
    static constexpr int kDefaultMaxMs = 10000;
    static constexpr int kHardCapMs = 30000;
    static constexpr int kMaxHolds = 32;

    explicit AgentActionHold(const AgentActor& actor, int max_ms = kDefaultMaxMs);
    ~AgentActionHold();
    AgentActionHold(const AgentActionHold&) = delete;
    AgentActionHold& operator=(const AgentActionHold&) = delete;

    /// End the hold now (idempotent).
    void release();
    /// Still counted (not released, not expired, not dropped by a person's input).
    bool active() const;

  private:
    quint64 id_ = 0;
};

/// The agent whose action is in progress on the GUI thread now: the innermost
/// live scope at this event-loop level, else the newest live hold. nullopt when
/// none, after a person's input, or off the GUI thread.
std::optional<AgentActor> current_agent_actor();

/// "agent:<client id>" for current_agent_actor(), or "" when no agent action is
/// in progress. See agent_order_source().
QString current_agent_order_source();

/// "agent:<id>": `client_id` lower-cased, every run of characters outside
/// [a-z0-9_] turned into one '_', trimmed of '_', at most 40 characters;
/// "agent:unknown" when nothing is left.
QString agent_order_source(const QString& client_id);

/// Live holds (diagnostics and tests).
int active_agent_holds();

} // namespace fincept::core::agent
