#include "screens/chat_mode/TerminalToolBridge.h"

#include "core/logging/Logger.h"
#include "mcp/McpProvider.h"
#include "mcp/TerminalMcpBridge.h"
#include "screens/chat_mode/ChatModeService.h"

#include <QFuture>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QtConcurrent>

namespace fincept::chat_mode {

// Categories that are UI-only and should not be exposed to Finagent
const QStringList TerminalToolBridge::EXCLUDED_CATEGORIES = {"navigation", "system", "settings"};

// Real money (live broker order placement/cancel/close) and local process
// control (install/start external MCP servers). See the header for why these
// fail closed on this path but not on the local-agent path.
const QStringList TerminalToolBridge::BLOCKED_CATEGORIES = {"live-trading", "mcp-servers"};

// Arbitrary local code execution + irreversible file deletion. Their categories
// ("analytics", "file_manager") also contain read-only tools worth keeping.
const QStringList TerminalToolBridge::BLOCKED_TOOLS = {"run_python_script", "delete_managed_file",
                                                       "bulk_delete_managed_files"};

bool TerminalToolBridge::is_remote_blocked(const QString& name, const QString& category) {
    return BLOCKED_CATEGORIES.contains(category) || BLOCKED_TOOLS.contains(name);
}

TerminalToolBridge::TerminalToolBridge(QObject* parent) : QObject(parent) {
    poll_timer_ = new QTimer(this);
    poll_timer_->setInterval(3000); // 3 seconds
    connect(poll_timer_, &QTimer::timeout, this, &TerminalToolBridge::on_poll_tick);
}

void TerminalToolBridge::start() {
    if (active_)
        return;
    active_ = true;
    LOG_INFO("TerminalToolBridge", "Starting tool bridge");
    register_tools();
    poll_timer_->start();
}

void TerminalToolBridge::stop() {
    if (!active_)
        return;
    active_ = false;
    poll_timer_->stop();
    LOG_INFO("TerminalToolBridge", "Stopped tool bridge");
}

// ── Register tools ───────────────────────────────────────────────────────────

void TerminalToolBridge::register_tools() {
    auto& provider = mcp::McpProvider::instance();
    const quint64 gen = provider.generation();

    // Skip if generation hasn't changed
    if (gen == last_gen_ && last_gen_ != 0)
        return;

    auto all_tools = provider.list_tools();
    QJsonArray tools_json;
    int blocked = 0;

    for (const auto& tool : all_tools) {
        // Phase 6.5: filter by ToolDef.category, not by name-prefix.
        // Pre-Phase-6 the check was tool.name.startsWith("navigation_") etc.
        // — but tool names don't follow that prefix convention, so the
        // filter never matched and every navigation/system/settings tool
        // got uploaded to the cloud. UnifiedTool.category (populated by
        // McpProvider::list_tools) is the source of truth.
        if (EXCLUDED_CATEGORIES.contains(tool.category))
            continue;
        // Recursive AiChat tools — skip by category, not by name prefix.
        if (tool.category == "ai-chat" || tool.category == "ai_chat")
            continue;
        // Fail closed on real-money / arbitrary-code tools — the cloud agent
        // has no equivalent of the local agent's destructive opt-in token.
        if (is_remote_blocked(tool.name, tool.category)) {
            ++blocked;
            continue;
        }

        QJsonObject schema = tool.input_schema;
        if (schema.isEmpty()) {
            schema["type"] = "object";
            schema["properties"] = QJsonObject();
        }

        QJsonObject t;
        t["name"] = tool.name;
        t["description"] = tool.description;
        t["input_schema"] = schema;
        tools_json.append(t);
    }

    const int count = tools_json.size();
    LOG_INFO("TerminalToolBridge",
             QString("Registering %1 tools (gen %2, %3 withheld as unsafe for remote execution)")
                 .arg(count)
                 .arg(gen)
                 .arg(blocked));

    QPointer<TerminalToolBridge> self = this;
    ChatModeService::instance().register_terminal_tools(
        tools_json, "4.0.0", count, [self, gen](bool ok, int registered, QString err) {
            if (!self)
                return;
            if (!ok) {
                LOG_WARN("TerminalToolBridge", "Registration failed: " + err);
                emit self->bridge_error("Tool registration failed: " + err);
                return;
            }
            self->last_gen_ = gen;
            LOG_INFO("TerminalToolBridge", QString("Registered %1 tools successfully").arg(registered));
            emit self->tools_registered(registered);
        });
}

// ── Poll for pending calls ───────────────────────────────────────────────────

void TerminalToolBridge::on_poll_tick() {
    if (!active_)
        return;

    // Check if tools changed since last registration
    const quint64 gen = mcp::McpProvider::instance().generation();
    if (gen != last_gen_)
        register_tools();

    QPointer<TerminalToolBridge> self = this;
    ChatModeService::instance().poll_pending_calls([self](bool ok, QJsonArray calls, QString err) {
        if (!self || !self->active_)
            return;
        if (!ok) {
            // Silently ignore poll failures — they're expected when endpoint isn't live
            LOG_DEBUG("TerminalToolBridge", "Poll failed: " + err);
            return;
        }
        for (const auto& v : calls) {
            const QJsonObject call = v.toObject();
            self->execute_call(call["call_id"].toString(), call["tool_name"].toString(), call["arguments"].toObject());
        }
    });
}

// ── Execute a tool call locally ──────────────────────────────────────────────

void TerminalToolBridge::execute_call(const QString& call_id, const QString& tool_name, const QJsonObject& arguments) {
    LOG_INFO("TerminalToolBridge", QString("Executing tool: %1 (call %2)").arg(tool_name, call_id));

    // Phase 5.10: centralised __ parsing. Was previously hardcoded as
    // mid(18) which silently breaks if the server-id prefix ever changes.
    auto [server_id, parsed_name] = mcp::McpProvider::parse_openai_function_name(tool_name);
    const QString local_name = parsed_name.isEmpty() ? tool_name : parsed_name;

    // Defence in depth: the backend can hand back any tool name — a stale
    // catalog from an earlier generation, or one the model invented. Re-check
    // the deny list at execution time so a blocked tool can never run even if
    // it was somehow registered.
    const auto tool_info = mcp::McpProvider::instance().find_tool(local_name);
    const QString tool_category = tool_info ? tool_info->category : QString();
    if (is_remote_blocked(local_name, tool_category)) {
        LOG_WARN("TerminalToolBridge",
                 QString("Refused remote tool '%1' (category '%2') — real-money / arbitrary-code tools are not "
                         "available to the cloud agent")
                     .arg(local_name, tool_category));
        const auto refusal = mcp::ToolResult::fail(
            QString("Tool '%1' is not available to the remote agent. It moves real funds or executes local "
                    "code and must be run by the user from the terminal UI.")
                .arg(local_name));
        ChatModeService::instance().submit_tool_result(call_id, refusal.to_json(), [](bool, QString) {});
        emit bridge_error(tr("Blocked unsafe remote tool call: %1").arg(local_name));
        return;
    }
    // Audit trail for every mutating tool the remote agent runs. Not a gate —
    // just a record, so a surprising state change can be traced back.
    if (tool_info && tool_info->is_destructive)
        LOG_WARN("TerminalToolBridge",
                 QString("Remote agent executing destructive tool '%1' (call %2)").arg(local_name, call_id));

    // Phase 4: dispatch via call_tool_async so sync handlers don't block
    // a worker thread for the duration of long-running tools, and so
    // multiple in-flight tool calls fan out concurrently. A QFutureWatcher
    // delivers the result back to this bridge's thread when complete.
    QPointer<TerminalToolBridge> self = this;

    // Mark this as an AGENT-originated call so the process-wide AuthChecker
    // installed by AgentService actually applies. That checker denies
    // `is_destructive` tools only when TerminalMcpBridge::is_call_in_progress()
    // is true — a thread_local flag previously set ONLY by TerminalMcpBridge.
    // This bridge (the cloud-agent poll path) never set it, so every tool it
    // dispatched was classified as a user-driven chat call and skipped the
    // destructive gate entirely. The deny list above is defence in depth; this
    // is the actual gate. `destructive_allowed=false` because a poll-driven
    // remote agent can never show a confirmation modal.
    //
    // Scoped around call_tool_async specifically: McpProvider runs
    // check_authorization() synchronously in that function's prologue
    // (McpProvider.cpp) before dispatching to a pool thread, so the flag is
    // read on THIS thread while the guard is alive.
    QFuture<mcp::ToolResult> future;
    {
        mcp::TerminalMcpBridge::ScopedCallFlags gate(/*call_in_progress=*/true, /*destructive_allowed=*/false);
        future = mcp::McpProvider::instance().call_tool_async(local_name, arguments);
    }

    auto* watcher = new QFutureWatcher<mcp::ToolResult>(this);
    QObject::connect(watcher, &QFutureWatcher<mcp::ToolResult>::finished, this, [self, call_id, local_name, watcher]() {
        // resultCount() lives on QFuture, not QFutureWatcher —
        // use future() to reach it.
        const auto fut = watcher->future();
        mcp::ToolResult result =
            (fut.resultCount() > 0) ? fut.result() : mcp::ToolResult::fail("Tool produced no result");
        watcher->deleteLater();

        if (!self || !self->active_)
            return;

        // Capture result by value into the submit callback —
        // avoids the init-capture pattern (which Clang on
        // Windows was rejecting for nested-lambda scoping).
        ChatModeService::instance().submit_tool_result(
            call_id, result.to_json(), [self, local_name, result](bool ok, QString err) {
                if (!self)
                    return;
                if (!ok) {
                    LOG_WARN("TerminalToolBridge", QString("Failed to submit result for %1: %2").arg(local_name, err));
                }
                emit self->tool_executed(local_name, result.success);
            });
    });
    watcher->setFuture(future);
}

} // namespace fincept::chat_mode
