// ToolDispatcher.cpp — Multi-round tool-call orchestrator.

#include "mcp/dispatch/ToolDispatcher.h"

#include "core/logging/Logger.h"
#include "mcp/McpService.h"

#include <QtConcurrent/QtConcurrent>

namespace fincept::mcp::dispatch {

static constexpr const char* TAG = "ToolDispatcher";

DispatchOutcome ToolDispatcher::run(
    const QString& user_message,
    const std::vector<fincept::ai_chat::ConversationMessage>& history,
    ProviderAdapter& adapter,
    SendFn send,
    const QJsonArray& openai_tools_array,
    std::shared_ptr<std::atomic<bool>> cancelled) {

    DispatchOutcome out;
    const int max_rounds = adapter.default_max_rounds();

    auto is_cancelled = [cancelled]() { return cancelled && cancelled->load(); };

    // Round 0 — initial request.
    QJsonObject body = adapter.build_initial_request(user_message, history, openai_tools_array);

    QJsonObject response;

    for (int round = 0; round < max_rounds; ++round) {
        ++out.rounds_used;

        if (is_cancelled()) {
            out.error = "cancelled";
            return out;
        }

        // Dispatch the request via the caller-supplied sender. The future
        // resolves with the parsed JSON response on success or an empty
        // object on failure (caller's send() must surface its own errors
        // by populating an error field in the JSON).
        QFuture<QJsonObject> resp_fut = send(body);
        resp_fut.waitForFinished();
        if (resp_fut.resultCount() == 0) {
            out.error = "Empty response from provider";
            return out;
        }
        response = resp_fut.result();

        if (response.contains("__error__")) {
            // Convention: caller uses __error__ key to signal HTTP/parse failure.
            out.error = response["__error__"].toString();
            return out;
        }

        // Detect tool invocations.
        std::vector<ToolInvocation> invocations = adapter.parse_tool_invocations(response);
        if (invocations.empty()) {
            // No tool calls — final text answer.
            out.text = adapter.extract_text(response);
            return out;
        }

        LOG_INFO(TAG, QString("Round %1 (%2): %3 tool call(s)")
                          .arg(round).arg(adapter.id()).arg(invocations.size()));

        // ── Parallel fan-out ──
        // Each invocation dispatches via McpService::execute_openai_function_async
        // (Phase 4 API). We collect futures then wait for all to finish so the
        // round elapses in max(t_i) instead of sum(t_i).
        std::vector<QFuture<ToolResult>> futures;
        futures.reserve(invocations.size());
        for (const auto& inv : invocations) {
            futures.push_back(
                McpService::instance().execute_openai_function_async(inv.function_name, inv.arguments));
        }

        std::vector<ToolResultEnvelope> results;
        results.reserve(invocations.size());
        for (std::size_t i = 0; i < invocations.size(); ++i) {
            futures[i].waitForFinished();
            ToolResult r = (futures[i].resultCount() > 0)
                               ? futures[i].result()
                               : ToolResult::fail("Tool produced no result");
            results.push_back({invocations[i].call_id, invocations[i].function_name, std::move(r)});
        }

        // Build the follow-up body for the next round.
        body = adapter.build_followup_request(response, results, user_message, history);
    }

    // Hit the round cap with tool calls still pending — surface partial text
    // if the last response had any, otherwise note the cap.
    out.max_rounds_hit = true;
    out.text = adapter.extract_text(response);
    if (out.text.isEmpty())
        out.error = QString("Max rounds (%1) reached without final answer").arg(max_rounds);
    return out;
}

} // namespace fincept::mcp::dispatch
