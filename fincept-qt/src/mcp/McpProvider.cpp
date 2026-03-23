// McpProvider.cpp — Internal tool registry and executor (Qt port)

#include "mcp/McpProvider.h"

#include "core/logging/Logger.h"

namespace fincept::mcp {

static constexpr const char* TAG = "McpProvider";

McpProvider& McpProvider::instance() {
    static McpProvider s;
    return s;
}

// ============================================================================
// Registration
// ============================================================================

void McpProvider::register_tool(ToolDef tool) {
    QMutexLocker lock(&mutex_);
    QString name = tool.name;
    tools_.insert(name, std::move(tool));
    ++generation_;
}

void McpProvider::register_tools(std::vector<ToolDef> tools) {
    QMutexLocker lock(&mutex_);
    for (auto& t : tools) {
        QString name = t.name;
        tools_.insert(name, std::move(t));
    }
    ++generation_;
}

void McpProvider::unregister_tool(const QString& name) {
    QMutexLocker lock(&mutex_);
    tools_.remove(name);
    ++generation_;
}

// ============================================================================
// Enable / Disable
// ============================================================================

void McpProvider::set_tool_enabled(const QString& name, bool enabled) {
    QMutexLocker lock(&mutex_);
    if (enabled)
        disabled_tools_.remove(name);
    else
        disabled_tools_.insert(name);
    ++generation_;
}

bool McpProvider::is_tool_enabled(const QString& name) const {
    QMutexLocker lock(&mutex_);
    return !disabled_tools_.contains(name);
}

// ============================================================================
// Discovery
// ============================================================================

std::vector<UnifiedTool> McpProvider::list_tools() const {
    QMutexLocker lock(&mutex_);
    std::vector<UnifiedTool> result;
    result.reserve(static_cast<std::size_t>(tools_.size()));

    for (auto it = tools_.cbegin(); it != tools_.cend(); ++it) {
        if (disabled_tools_.contains(it.key()))
            continue;
        const auto& t = it.value();
        result.push_back({QString(INTERNAL_SERVER_ID), QString(INTERNAL_SERVER_NAME), t.name, t.description,
                          t.input_schema.to_json(), true});
    }
    return result;
}

std::vector<UnifiedTool> McpProvider::list_all_tools() const {
    QMutexLocker lock(&mutex_);
    std::vector<UnifiedTool> result;
    result.reserve(static_cast<std::size_t>(tools_.size()));

    for (auto it = tools_.cbegin(); it != tools_.cend(); ++it) {
        const auto& t = it.value();
        result.push_back({QString(INTERNAL_SERVER_ID), QString(INTERNAL_SERVER_NAME), t.name, t.description,
                          t.input_schema.to_json(), true});
    }
    return result;
}

std::size_t McpProvider::tool_count() const {
    QMutexLocker lock(&mutex_);
    std::size_t count = 0;
    for (auto it = tools_.cbegin(); it != tools_.cend(); ++it) {
        if (!disabled_tools_.contains(it.key()))
            ++count;
    }
    return count;
}

bool McpProvider::has_tool(const QString& name) const {
    QMutexLocker lock(&mutex_);
    return tools_.contains(name);
}

// ============================================================================
// Execution
// ============================================================================

ToolResult McpProvider::call_tool(const QString& name, const QJsonObject& args) {
    ToolHandler handler;

    {
        QMutexLocker lock(&mutex_);

        if (!tools_.contains(name))
            return ToolResult::fail("Tool not found: " + name);

        if (disabled_tools_.contains(name))
            return ToolResult::fail("Tool is disabled: " + name);

        handler = tools_[name].handler;
    }

    // Normalize: handlers expect a JSON object, never null/array
    const QJsonObject& safe_args = args;

    try {
        LOG_DEBUG(TAG, "Calling tool: " + name);
        return handler(safe_args);
    } catch (const std::exception& e) {
        LOG_ERROR(TAG, QString("Tool '%1' threw exception: %2").arg(name, e.what()));
        return ToolResult::fail(QString("Tool execution error: ") + e.what());
    } catch (...) {
        LOG_ERROR(TAG, QString("Tool '%1' threw unknown exception").arg(name));
        return ToolResult::fail("Unknown error during tool execution");
    }
}

// ============================================================================
// LLM Integration
// ============================================================================

QJsonArray McpProvider::format_tools_for_openai() const {
    auto tools = list_tools();
    QJsonArray result;

    for (const auto& tool : tools) {
        QJsonObject schema = tool.input_schema;
        if (schema.isEmpty()) {
            schema["type"] = "object";
            schema["properties"] = QJsonObject();
        }

        QJsonObject fn;
        fn["name"] = QString(INTERNAL_SERVER_ID) + "__" + tool.name;
        fn["description"] = tool.description;
        fn["parameters"] = schema;

        QJsonObject entry;
        entry["type"] = "function";
        entry["function"] = fn;
        result.append(entry);
    }

    return result;
}

QPair<QString, QString> McpProvider::parse_openai_function_name(const QString& fn_name) {
    int pos = fn_name.indexOf("__");
    if (pos <= 0 || pos >= fn_name.length() - 2)
        return {"", ""};
    return {fn_name.left(pos), fn_name.mid(pos + 2)};
}

// ============================================================================
// Lifecycle
// ============================================================================

void McpProvider::clear() {
    QMutexLocker lock(&mutex_);
    tools_.clear();
    disabled_tools_.clear();
    ++generation_;
}

// ============================================================================
// Generation Counter
// ============================================================================

quint64 McpProvider::generation() const {
    QMutexLocker lock(&mutex_);
    return generation_;
}

} // namespace fincept::mcp
