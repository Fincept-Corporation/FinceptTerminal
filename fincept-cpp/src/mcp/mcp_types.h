#pragma once
// MCP Types — Core type definitions for the Model Context Protocol system
// Mirrors the TypeScript InternalTool / MCPTool / MCPToolResult types

#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <nlohmann/json.hpp>

namespace fincept::mcp {

using json = nlohmann::json;

// ============================================================================
// Tool Schema — JSON Schema for tool input parameters
// ============================================================================

struct ToolSchema {
    std::string type = "object";
    json properties = json::object();       // { "param": { "type": "string", "description": "..." } }
    std::vector<std::string> required;      // required parameter names

    json to_json() const {
        json j;
        j["type"] = type;
        j["properties"] = properties;
        if (!required.empty()) j["required"] = required;
        return j;
    }
};

// ============================================================================
// Tool Result — returned by tool handlers
// ============================================================================

struct ToolResult {
    bool success = false;
    json data;                              // arbitrary result data
    std::string message;                    // human-readable message
    std::string error;                      // error message if !success

    json to_json() const {
        json j;
        j["success"] = success;
        if (!data.is_null()) j["data"] = data;
        if (!message.empty()) j["message"] = message;
        if (!error.empty()) j["error"] = error;
        return j;
    }

    static ToolResult ok(const std::string& msg, const json& data = nullptr) {
        return {true, data, msg, ""};
    }

    static ToolResult ok_data(const json& data) {
        return {true, data, "", ""};
    }

    static ToolResult fail(const std::string& err) {
        return {false, nullptr, "", err};
    }
};

// ============================================================================
// Tool Handler — function signature for tool execution
// ============================================================================

using ToolHandler = std::function<ToolResult(const json& args)>;

// ============================================================================
// Tool Definition — a registered MCP tool
// ============================================================================

struct ToolDef {
    std::string name;
    std::string description;
    std::string category;                   // navigation, trading, portfolio, market-data, etc.
    ToolSchema input_schema;
    ToolHandler handler;
    bool enabled = true;
};

// ============================================================================
// Server Status — for external MCP server management
// ============================================================================

enum class ServerStatus {
    Stopped,
    Running,
    Error
};

inline const char* server_status_str(ServerStatus s) {
    switch (s) {
        case ServerStatus::Stopped: return "stopped";
        case ServerStatus::Running: return "running";
        case ServerStatus::Error:   return "error";
    }
    return "unknown";
}

// ============================================================================
// MCP Server Config — persisted in SQLite
// ============================================================================

struct MCPServerConfig {
    std::string id;
    std::string name;
    std::string description;
    std::string command;                    // e.g., "npx", "uvx", "node"
    std::vector<std::string> args;
    std::map<std::string, std::string> env;
    std::string category;
    bool enabled = true;
    bool auto_start = false;
    ServerStatus status = ServerStatus::Stopped;
    std::string created_at;
    std::string updated_at;
};

// ============================================================================
// External Tool — tool discovered from an external MCP server
// ============================================================================

struct ExternalTool {
    std::string server_id;
    std::string server_name;
    std::string name;
    std::string description;
    json input_schema;
};

// ============================================================================
// Unified Tool — merged view for consumers (Chat, Agents, etc.)
// ============================================================================

struct UnifiedTool {
    std::string server_id;                  // "fincept-terminal" for internal
    std::string server_name;
    std::string name;
    std::string description;
    json input_schema;
    bool is_internal = false;
};

// ============================================================================
// Constants
// ============================================================================

inline constexpr const char* INTERNAL_SERVER_ID   = "fincept-terminal";
inline constexpr const char* INTERNAL_SERVER_NAME = "Fincept Terminal";

} // namespace fincept::mcp
