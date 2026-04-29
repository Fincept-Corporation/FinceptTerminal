// McpService.cpp — Unified tool interface (Qt port)

#include "mcp/McpService.h"

#include "core/logging/Logger.h"
#include "mcp/McpManager.h"
#include "mcp/McpProvider.h"

#include <QJsonDocument>
#include <QThread>
#include <QTimer>

namespace fincept::mcp {

static constexpr const char* TAG = "McpService";

McpService& McpService::instance() {
    static McpService s;
    return s;
}

// ============================================================================
// Lifecycle
// ============================================================================

void McpService::initialize() {
    // Load external server configs from DB (fast, synchronous)
    McpManager::instance().initialize();

    // Invalidate tool cache whenever external servers change (start/stop/add/remove)
    QObject::connect(&McpManager::instance(), &McpManager::servers_changed, [this]() {
        QMutexLocker lock(&mutex_);
        cache_time_ = QDateTime(); // force refresh on next get_all_tools()
        LOG_INFO(TAG, "Tool cache invalidated — external servers changed");
    });

    // Auto-start enabled servers in a background thread so the UI never
    // freezes. Only servers with both enabled AND auto_start flags are
    // started at launch. Users can manually enable others from the MCP tab.
    const auto servers = McpManager::instance().get_servers();
    QStringList to_start;
    for (const auto& srv : servers) {
        if (srv.enabled && srv.auto_start)
            to_start.append(srv.id);
    }

    if (!to_start.isEmpty()) {
        QThread* t = QThread::create([to_start]() {
            for (const auto& id : to_start) {
                LOG_INFO("McpService", "Auto-starting MCP server: " + id);
                auto r = McpManager::instance().start_server(id);
                if (r.is_err())
                    LOG_WARN("McpService", "Auto-start failed for " + id + ": " + QString::fromStdString(r.error()));
            }
            LOG_INFO("McpService", "All external MCP servers started");
        });
        t->setObjectName("mcp-autostart");
        t->start();
        QObject::connect(t, &QThread::finished, t, &QObject::deleteLater);
    }

    McpManager::instance().start_health_check();

    LOG_INFO(TAG, QString("McpService initialized — %1 internal tools, %2 external servers queued")
                      .arg(McpProvider::instance().tool_count())
                      .arg(to_start.size()));
}

void McpService::shutdown() {
    McpManager::instance().shutdown();
    McpProvider::instance().clear();
    LOG_INFO(TAG, "McpService shut down");
}

// ============================================================================
// Tool Discovery
// ============================================================================

std::vector<UnifiedTool> McpService::get_all_tools() {
    QMutexLocker lock(&mutex_);
    if (!is_cache_valid())
        refresh_cache();
    return cached_tools_;
}

QJsonArray McpService::format_tools_for_openai() {
    auto tools = get_all_tools();
    QJsonArray result;

    for (const auto& tool : tools) {
        QString fn_name = tool.server_id + "__" + tool.name;

        QJsonObject schema = tool.input_schema;
        if (schema.isEmpty()) {
            schema["type"] = "object";
            schema["properties"] = QJsonObject();
        }

        QJsonObject fn;
        fn["name"] = fn_name;
        fn["description"] = tool.description;
        fn["parameters"] = schema;

        QJsonObject entry;
        entry["type"] = "function";
        entry["function"] = fn;
        result.append(entry);
    }

    LOG_INFO(TAG, QString("format_tools_for_openai: %1 tools sent to LLM").arg(result.size()));
    return result;
}

std::size_t McpService::tool_count() {
    return get_all_tools().size();
}

// ============================================================================
// Tool Execution
// ============================================================================

ToolResult McpService::execute_tool(const QString& server_id, const QString& tool_name, const QJsonObject& args) {
    // Route to internal provider
    if (server_id == INTERNAL_SERVER_ID)
        return McpProvider::instance().call_tool(tool_name, args);

    // Route to external server
    auto result = McpManager::instance().call_external_tool(server_id, tool_name, args);
    if (result.is_err())
        return ToolResult::fail(QString::fromStdString(result.error()));

    const QJsonObject& data = result.value();

    bool is_error = data["isError"].toBool(false);
    QJsonArray content = data["content"].toArray();

    QString text;
    for (const auto& item : content) {
        QJsonObject obj = item.toObject();
        if (obj["type"].toString() == "text")
            text += obj["text"].toString();
    }

    if (is_error)
        return ToolResult::fail(text.isEmpty() ? "External tool error" : text);

    // Try to parse text as JSON data
    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    if (!doc.isNull()) {
        if (doc.isObject())
            return ToolResult::ok(text, doc.object());
        if (doc.isArray())
            return ToolResult::ok(text, doc.array());
    }

    return ToolResult::ok(text);
}

ToolResult McpService::execute_openai_function(const QString& function_name, const QJsonObject& args) {
    auto [server_id, tool_name] = McpProvider::parse_openai_function_name(function_name);

    if (server_id.isEmpty() || tool_name.isEmpty()) {
        LOG_WARN(TAG, "Invalid function name format: " + function_name);
        return ToolResult::fail("Invalid function name format: " + function_name);
    }

    LOG_INFO(TAG, QString("Dispatch: %1 -> server=%2 tool=%3").arg(function_name, server_id, tool_name));
    auto result = execute_tool(server_id, tool_name, args);
    LOG_INFO(TAG, QString("Dispatch result: %1 success=%2").arg(tool_name, result.success ? "true" : "false"));
    return result;
}

// ============================================================================
// Validation
// ============================================================================

Result<void> McpService::validate_params(const QString& tool_name, const QJsonObject& args) {
    auto tools = get_all_tools();
    for (const auto& tool : tools) {
        if (tool.name != tool_name)
            continue;

        const QJsonObject& schema = tool.input_schema;

        // Required field validation
        if (schema.contains("required")) {
            for (const auto& req : schema["required"].toArray()) {
                QString field = req.toString();
                if (!args.contains(field))
                    return Result<void>::err("Missing required parameter: " + field.toStdString());
            }
        }

        // Type validation
        if (schema.contains("properties")) {
            QJsonObject props = schema["properties"].toObject();
            for (auto it = props.constBegin(); it != props.constEnd(); ++it) {
                const QString& key = it.key();
                if (!args.contains(key))
                    continue;

                QString expected = it.value().toObject()["type"].toString();
                QJsonValue val = args[key];

                bool valid = true;
                if (expected == "string")
                    valid = val.isString();
                else if (expected == "number")
                    valid = val.isDouble();
                else if (expected == "integer")
                    valid = val.isDouble();
                else if (expected == "boolean")
                    valid = val.isBool();
                else if (expected == "array")
                    valid = val.isArray();
                else if (expected == "object")
                    valid = val.isObject();

                if (!valid)
                    return Result<void>::err(("Parameter '" + key + "' should be " + expected).toStdString());
            }
        }

        return Result<void>::ok();
    }

    return Result<void>::err("Tool not found: " + tool_name.toStdString());
}

// ============================================================================
// Cache
// ============================================================================

bool McpService::is_cache_valid() const {
    if (McpProvider::instance().generation() != cached_generation_)
        return false;
    if (cache_time_.isNull() || cached_tools_.empty())
        return false;
    return cache_time_.msecsTo(QDateTime::currentDateTime()) < CACHE_TTL_MS;
}

void McpService::refresh_cache() {
    cached_tools_.clear();

    // Internal tools
    auto internal = McpProvider::instance().list_tools();
    cached_tools_.insert(cached_tools_.end(), internal.begin(), internal.end());

    // External tools
    auto external = McpManager::instance().get_all_external_tools();
    for (auto& ext : external) {
        cached_tools_.push_back({ext.server_id, ext.server_name, ext.name, ext.description, ext.input_schema, false});
    }

    cache_time_ = QDateTime::currentDateTime();
    cached_generation_ = McpProvider::instance().generation();

    LOG_INFO(TAG, QString("Refreshed tool cache: %1 total (%2 internal, %3 external)")
                      .arg(cached_tools_.size())
                      .arg(internal.size())
                      .arg(external.size()));
}

} // namespace fincept::mcp
