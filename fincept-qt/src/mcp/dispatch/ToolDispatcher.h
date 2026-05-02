#pragma once
// ToolDispatcher.h — Single multi-round tool-call orchestrator.
//
// Phase 5 of the MCP refactor. Replaces the 5 parallel tool-call loops in
// LlmService.cpp with a single state machine that drives any provider
// through its ProviderAdapter. The dispatcher fans tool calls out via
// McpService::execute_openai_function_async (Phase 4 API) so multi-tool
// rounds run in parallel instead of sequentially.
//
// Lifecycle of one user turn:
//
//   1. dispatcher.run() takes the user message, history, and adapter.
//   2. adapter->build_initial_request() produces the first request body.
//   3. The dispatcher dispatches the request (via the adapter's send hook,
//      injected by the caller — see `Sender` below).
//   4. adapter->parse_tool_invocations() extracts any tool calls.
//   5. If empty, dispatcher returns adapter->extract_text() and exits.
//   6. Otherwise, all invocations dispatched in parallel via
//      McpService::execute_openai_function_async; QtFuture::whenAll joins.
//   7. adapter->build_followup_request() builds the next body.
//   8. Loop back to step 3 up to adapter->default_max_rounds() rounds.
//
// Cancellation: caller sets the `cancelled` flag to abort after the current
// round (in-flight tool calls are not killed; their futures are awaited).

#include "mcp/dispatch/ProviderAdapter.h"

#include <QFuture>
#include <QJsonObject>
#include <QString>

#include <atomic>
#include <functional>
#include <memory>

namespace fincept::mcp::dispatch {

/// Function the caller supplies to send a request body to the LLM provider.
/// Returns a future resolving with the parsed JSON response.
/// The dispatcher does NOT know about HTTP, auth, or streaming — those stay
/// in LlmService where they belong; the dispatcher only orchestrates rounds.
using SendFn = std::function<QFuture<QJsonObject>(QJsonObject body)>;

/// Result of one full dispatch run.
struct DispatchOutcome {
    QString text;            ///< Final assistant text, ready to display.
    QString error;           ///< Non-empty on dispatch failure.
    int rounds_used = 0;     ///< How many request/response rounds ran.
    bool max_rounds_hit = false; ///< True if loop terminated by round cap.
};

class ToolDispatcher {
  public:
    /// Run the full dispatch loop for one user turn.
    ///
    /// `cancelled` is polled between rounds; set it from another thread to
    /// abort. The dispatcher will not start a new round once it observes
    /// the flag, but in-flight tool calls and HTTP requests run to
    /// completion (their cancellation is owned by the caller).
    DispatchOutcome run(
        const QString& user_message,
        const std::vector<fincept::ai_chat::ConversationMessage>& history,
        ProviderAdapter& adapter,
        SendFn send,
        const QJsonArray& openai_tools_array,
        std::shared_ptr<std::atomic<bool>> cancelled = {});
};

} // namespace fincept::mcp::dispatch
