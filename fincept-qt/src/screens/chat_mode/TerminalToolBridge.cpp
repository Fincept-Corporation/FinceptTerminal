#include "screens/chat_mode/TerminalToolBridge.h"
#include "screens/chat_mode/ChatModeService.h"
#include "mcp/McpProvider.h"
#include "core/logging/Logger.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QtConcurrent>

namespace fincept::chat_mode {

// Categories that are UI-only and should not be exposed to Finagent
const QStringList TerminalToolBridge::EXCLUDED_CATEGORIES = {
    "navigation", "system", "settings"
};

TerminalToolBridge::TerminalToolBridge(QObject* parent) : QObject(parent) {
    poll_timer_ = new QTimer(this);
    poll_timer_->setInterval(3000); // 3 seconds
    connect(poll_timer_, &QTimer::timeout, this, &TerminalToolBridge::on_poll_tick);
}

void TerminalToolBridge::start() {
    if (active_) return;
    active_ = true;
    LOG_INFO("TerminalToolBridge", "Starting tool bridge");
    register_tools();
    poll_timer_->start();
}

void TerminalToolBridge::stop() {
    if (!active_) return;
    active_ = false;
    poll_timer_->stop();
    LOG_INFO("TerminalToolBridge", "Stopped tool bridge");
}

// ── Register tools ───────────────────────────────────────────────────────────

void TerminalToolBridge::register_tools() {
    auto& provider = mcp::McpProvider::instance();
    const quint64 gen = provider.generation();

    // Skip if generation hasn't changed
    if (gen == last_gen_ && last_gen_ != 0) return;

    auto all_tools = provider.list_tools();
    QJsonArray tools_json;

    for (const auto& tool : all_tools) {
        // Skip UI-only tools
        bool skip = false;
        // Check if tool name starts with excluded category prefixes
        for (const auto& cat : EXCLUDED_CATEGORIES) {
            if (tool.name.startsWith(cat + "_") || tool.name.startsWith(cat + "-")) {
                skip = true;
                break;
            }
        }
        // Also skip AiChat tools (recursive)
        if (tool.name.startsWith("ai_chat") || tool.name.startsWith("aichat"))
            skip = true;
        if (skip) continue;

        QJsonObject schema = tool.input_schema;
        if (schema.isEmpty()) {
            schema["type"] = "object";
            schema["properties"] = QJsonObject();
        }

        QJsonObject t;
        t["name"]         = tool.name;
        t["description"]  = tool.description;
        t["input_schema"] = schema;
        tools_json.append(t);
    }

    const int count = tools_json.size();
    LOG_INFO("TerminalToolBridge",
             QString("Registering %1 tools (gen %2)").arg(count).arg(gen));

    QPointer<TerminalToolBridge> self = this;
    ChatModeService::instance().register_terminal_tools(
        tools_json, "4.0.0", count,
        [self, gen, count](bool ok, int registered, QString err) {
            if (!self) return;
            if (!ok) {
                LOG_WARN("TerminalToolBridge", "Registration failed: " + err);
                emit self->bridge_error("Tool registration failed: " + err);
                return;
            }
            self->last_gen_ = gen;
            LOG_INFO("TerminalToolBridge",
                     QString("Registered %1 tools successfully").arg(registered));
            emit self->tools_registered(registered);
        });
}

// ── Poll for pending calls ───────────────────────────────────────────────────

void TerminalToolBridge::on_poll_tick() {
    if (!active_) return;

    // Check if tools changed since last registration
    const quint64 gen = mcp::McpProvider::instance().generation();
    if (gen != last_gen_)
        register_tools();

    QPointer<TerminalToolBridge> self = this;
    ChatModeService::instance().poll_pending_calls(
        [self](bool ok, QJsonArray calls, QString err) {
            if (!self || !self->active_) return;
            if (!ok) {
                // Silently ignore poll failures — they're expected when endpoint isn't live
                LOG_DEBUG("TerminalToolBridge", "Poll failed: " + err);
                return;
            }
            for (const auto& v : calls) {
                const QJsonObject call = v.toObject();
                self->execute_call(
                    call["call_id"].toString(),
                    call["tool_name"].toString(),
                    call["arguments"].toObject());
            }
        });
}

// ── Execute a tool call locally ──────────────────────────────────────────────

void TerminalToolBridge::execute_call(const QString& call_id,
                                      const QString& tool_name,
                                      const QJsonObject& arguments)
{
    LOG_INFO("TerminalToolBridge",
             QString("Executing tool: %1 (call %2)").arg(tool_name, call_id));

    // Strip "fincept-terminal__" prefix if present
    QString local_name = tool_name;
    if (local_name.startsWith("fincept-terminal__"))
        local_name = local_name.mid(18);

    // Execute on background thread to avoid blocking UI
    QPointer<TerminalToolBridge> self = this;
    QtConcurrent::run([self, call_id, local_name, arguments]() {
        // Call the tool via McpProvider
        auto result = mcp::McpProvider::instance().call_tool(local_name, arguments);

        if (!self) return;
        QMetaObject::invokeMethod(self, [self, call_id, local_name, result]() {
            if (!self || !self->active_) return;

            // Submit result back to Finagent
            ChatModeService::instance().submit_tool_result(
                call_id, result.to_json(),
                [self, local_name, success = result.success](bool ok, QString err) {
                    if (!self) return;
                    if (!ok) {
                        LOG_WARN("TerminalToolBridge",
                                 QString("Failed to submit result for %1: %2")
                                     .arg(local_name, err));
                    }
                    emit self->tool_executed(local_name, success);
                });
        }, Qt::QueuedConnection);
    });
}

} // namespace fincept::chat_mode
