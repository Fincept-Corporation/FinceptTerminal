// MCPProvider — Internal tool registry and executor implementation

#include "mcp_provider.h"
#include "../core/logger.h"

namespace fincept::mcp {

static constexpr const char* TAG_PROVIDER = "MCP";

MCPProvider& MCPProvider::instance() {
    static MCPProvider s;
    return s;
}

// ============================================================================
// Registration
// ============================================================================

void MCPProvider::register_tool(ToolDef tool) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto name = tool.name;
    tools_.emplace(std::move(name), std::move(tool));
    ++generation_;
}

void MCPProvider::register_tools(std::vector<ToolDef> tools) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& t : tools) {
        auto name = t.name;
        tools_.emplace(std::move(name), std::move(t));
    }
    ++generation_;
}

void MCPProvider::unregister_tool(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    tools_.erase(name);
    ++generation_;
}

// ============================================================================
// Enable / Disable
// ============================================================================

void MCPProvider::set_tool_enabled(const std::string& name, bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (enabled)
        disabled_tools_.erase(name);
    else
        disabled_tools_.insert(name);
    ++generation_;
}

bool MCPProvider::is_tool_enabled(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return disabled_tools_.find(name) == disabled_tools_.end();
}

// ============================================================================
// Discovery
// ============================================================================

std::vector<UnifiedTool> MCPProvider::list_tools() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<UnifiedTool> result;
    result.reserve(tools_.size());

    for (const auto& [name, tool] : tools_) {
        if (disabled_tools_.count(name)) continue;

        result.push_back({
            INTERNAL_SERVER_ID,
            INTERNAL_SERVER_NAME,
            tool.name,
            tool.description,
            tool.input_schema.to_json(),
            true
        });
    }
    return result;
}

std::vector<UnifiedTool> MCPProvider::list_all_tools() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<UnifiedTool> result;
    result.reserve(tools_.size());

    for (const auto& [name, tool] : tools_) {
        result.push_back({
            INTERNAL_SERVER_ID,
            INTERNAL_SERVER_NAME,
            tool.name,
            tool.description,
            tool.input_schema.to_json(),
            true
        });
    }
    return result;
}

size_t MCPProvider::tool_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (const auto& [name, _] : tools_) {
        if (!disabled_tools_.count(name)) ++count;
    }
    return count;
}

bool MCPProvider::has_tool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tools_.find(name) != tools_.end();
}

// ============================================================================
// Execution
// ============================================================================

ToolResult MCPProvider::call_tool(const std::string& name, const json& args) {
    ToolHandler handler;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = tools_.find(name);
        if (it == tools_.end()) {
            return ToolResult::fail("Tool not found: " + name);
        }

        if (disabled_tools_.count(name)) {
            return ToolResult::fail("Tool is disabled: " + name);
        }

        handler = it->second.handler;
    }

    // Normalize args: handlers expect a JSON object, never null/array/etc.
    // This prevents crashes in handlers that call args.value() on non-objects.
    const json& safe_args = args.is_object() ? args : json::object();

    // Execute outside the lock to avoid deadlocks
    try {
        LOG_DEBUG(TAG_PROVIDER, "Calling tool: %s", name.c_str());
        return handler(safe_args);
    } catch (const std::exception& e) {
        LOG_ERROR(TAG_PROVIDER, "Tool '%s' threw exception: %s", name.c_str(), e.what());
        return ToolResult::fail(std::string("Tool execution error: ") + e.what());
    } catch (...) {
        LOG_ERROR(TAG_PROVIDER, "Tool '%s' threw unknown exception", name.c_str());
        return ToolResult::fail("Unknown error during tool execution");
    }
}

// ============================================================================
// LLM Integration
// ============================================================================

json MCPProvider::format_tools_for_openai() const {
    auto tools = list_tools();
    json result = json::array();

    for (const auto& tool : tools) {
        result.push_back({
            {"type", "function"},
            {"function", {
                {"name", std::string(INTERNAL_SERVER_ID) + "__" + tool.name},
                {"description", tool.description},
                {"parameters", tool.input_schema.is_null() ? json{{"type", "object"}, {"properties", json::object()}} : tool.input_schema}
            }}
        });
    }

    return result;
}

std::pair<std::string, std::string> MCPProvider::parse_openai_function_name(const std::string& fn_name) {
    auto pos = fn_name.find("__");
    if (pos == std::string::npos || pos == 0 || pos == fn_name.size() - 2) {
        return {"", ""};
    }
    return {fn_name.substr(0, pos), fn_name.substr(pos + 2)};
}

// ============================================================================
// Lifecycle
// ============================================================================

void MCPProvider::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    tools_.clear();
    disabled_tools_.clear();
    ++generation_;
}

// ============================================================================
// Generation Counter
// ============================================================================

uint64_t MCPProvider::generation() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return generation_;
}

} // namespace fincept::mcp
