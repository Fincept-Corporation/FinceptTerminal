// MCPService — Unified tool interface implementation

#include "mcp_service.h"
#include "mcp_provider.h"
#include "mcp_manager.h"
#include "../core/logger.h"
#include <thread>

namespace fincept::mcp {

static constexpr const char* TAG_SERVICE = "MCPService";

MCPService& MCPService::instance() {
    static MCPService s;
    return s;
}

// ============================================================================
// Lifecycle
// ============================================================================

void MCPService::initialize() {
    // Manager handles external servers — load configs from DB (fast)
    MCPManager::instance().initialize();

    // Start external MCP servers in a background thread to avoid blocking
    // the main/render thread.  Each server start involves spawning a process,
    // running an IPC handshake, and listing tools — all potentially slow.
    std::thread([]() {
        MCPManager::instance().start_auto_servers();
        MCPManager::instance().start_health_check();
        LOG_INFO(TAG_SERVICE, "MCP external servers started in background");
    }).detach();

    LOG_INFO(TAG_SERVICE, "MCPService initialized — %zu internal tools",
             MCPProvider::instance().tool_count());
}

void MCPService::shutdown() {
    MCPManager::instance().shutdown();
    MCPProvider::instance().clear();
    LOG_INFO(TAG_SERVICE, "MCPService shut down");
}

// ============================================================================
// Tool Discovery
// ============================================================================

std::vector<UnifiedTool> MCPService::get_all_tools() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!is_cache_valid()) {
        refresh_cache();
    }
    return cached_tools_;
}

json MCPService::format_tools_for_openai() {
    auto tools = get_all_tools();
    json result = json::array();

    for (const auto& tool : tools) {
        std::string fn_name = tool.server_id + "__" + tool.name;

        result.push_back({
            {"type", "function"},
            {"function", {
                {"name", fn_name},
                {"description", tool.description},
                {"parameters", tool.input_schema.is_null()
                    ? json{{"type", "object"}, {"properties", json::object()}}
                    : tool.input_schema}
            }}
        });
    }

    return result;
}

size_t MCPService::tool_count() {
    auto tools = get_all_tools();
    return tools.size();
}

// ============================================================================
// Tool Execution
// ============================================================================

ToolResult MCPService::execute_tool(const std::string& server_id,
                                     const std::string& tool_name,
                                     const json& args) {
    // Route to internal provider
    if (server_id == INTERNAL_SERVER_ID) {
        return MCPProvider::instance().call_tool(tool_name, args);
    }

    // Route to external server
    auto result = MCPManager::instance().call_external_tool(server_id, tool_name, args);
    if (result.failed()) {
        return ToolResult::fail(result.error().message);
    }

    // Convert external result to ToolResult
    auto& data = result.value();

    // MCP tools/call returns { content: [{type, text}], isError? }
    bool is_error = data.value("isError", false);
    json content = data.value("content", json::array());

    std::string text;
    json result_data;

    for (const auto& item : content) {
        if (item.value("type", "") == "text") {
            text += item.value("text", "");
        }
    }

    if (is_error) {
        return ToolResult::fail(text.empty() ? "External tool error" : text);
    }

    // Try to parse text as JSON data
    try {
        result_data = json::parse(text);
    } catch (...) {
        result_data = text;
    }

    return ToolResult::ok(text, result_data);
}

ToolResult MCPService::execute_openai_function(const std::string& function_name,
                                                const json& args) {
    auto [server_id, tool_name] = MCPProvider::parse_openai_function_name(function_name);

    if (server_id.empty() || tool_name.empty()) {
        return ToolResult::fail("Invalid function name format: " + function_name);
    }

    return execute_tool(server_id, tool_name, args);
}

// ============================================================================
// Validation
// ============================================================================

Result<void> MCPService::validate_params(const std::string& tool_name, const json& args) {
    // Find the tool
    auto tools = get_all_tools();
    for (const auto& tool : tools) {
        if (tool.name == tool_name) {
            // Basic required field validation
            if (tool.input_schema.contains("required")) {
                for (const auto& req : tool.input_schema["required"]) {
                    std::string field = req.get<std::string>();
                    if (!args.contains(field)) {
                        return Error("Missing required parameter: " + field);
                    }
                }
            }

            // Type validation for properties
            if (tool.input_schema.contains("properties") && tool.input_schema["properties"].is_object()) {
                for (auto& [key, schema] : tool.input_schema["properties"].items()) {
                    if (!args.contains(key)) continue;

                    std::string expected_type = schema.value("type", "");
                    const auto& val = args[key];

                    bool valid = true;
                    if (expected_type == "string") valid = val.is_string();
                    else if (expected_type == "number") valid = val.is_number();
                    else if (expected_type == "integer") valid = val.is_number_integer();
                    else if (expected_type == "boolean") valid = val.is_boolean();
                    else if (expected_type == "array") valid = val.is_array();
                    else if (expected_type == "object") valid = val.is_object();

                    if (!valid) {
                        return Error("Parameter '" + key + "' should be " + expected_type);
                    }
                }
            }

            return {};
        }
    }

    return Error("Tool not found: " + tool_name);
}

// ============================================================================
// Cache
// ============================================================================

bool MCPService::is_cache_valid() const {
    // Invalidate if the provider's tool set has changed (generation mismatch)
    if (MCPProvider::instance().generation() != cached_generation_) return false;

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - cache_time_).count();
    return elapsed < CACHE_TTL_MS && !cached_tools_.empty();
}

void MCPService::refresh_cache() {
    cached_tools_.clear();

    // Internal tools
    auto internal = MCPProvider::instance().list_tools();
    cached_tools_.insert(cached_tools_.end(), internal.begin(), internal.end());

    // External tools
    auto external = MCPManager::instance().get_all_external_tools();
    for (auto& ext : external) {
        cached_tools_.push_back({
            ext.server_id,
            ext.server_name,
            ext.name,
            ext.description,
            ext.input_schema,
            false
        });
    }

    cache_time_ = std::chrono::steady_clock::now();
    cached_generation_ = MCPProvider::instance().generation();
    LOG_DEBUG(TAG_SERVICE, "Refreshed tool cache: %zu total (%zu internal, %zu external)",
              cached_tools_.size(), internal.size(), external.size());
}

} // namespace fincept::mcp
