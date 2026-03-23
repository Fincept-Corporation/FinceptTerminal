#pragma once
// McpTypes.h — Core type definitions for the Model Context Protocol system (Qt port)
// Translated from fincept-cpp mcp_types.h — std::string → QString, nlohmann::json → QJsonObject

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

namespace fincept::mcp {

// ============================================================================
// Tool Result — returned by tool handlers
// ============================================================================

struct ToolResult {
    bool success = false;
    QJsonValue data; // arbitrary result data
    QString message; // human-readable message
    QString error;   // error message if !success

    QJsonObject to_json() const {
        QJsonObject j;
        j["success"] = success;
        if (!data.isNull() && !data.isUndefined())
            j["data"] = data;
        if (!message.isEmpty())
            j["message"] = message;
        if (!error.isEmpty())
            j["error"] = error;
        return j;
    }

    static ToolResult ok(const QString& msg, const QJsonValue& data = QJsonValue()) {
        ToolResult r;
        r.success = true;
        r.message = msg;
        r.data = data;
        return r;
    }

    static ToolResult ok_data(const QJsonValue& data) {
        ToolResult r;
        r.success = true;
        r.data = data;
        return r;
    }

    static ToolResult fail(const QString& err) {
        ToolResult r;
        r.success = false;
        r.error = err;
        return r;
    }
};

// ============================================================================
// Tool Handler — function signature for tool execution
// ============================================================================

using ToolHandler = std::function<ToolResult(const QJsonObject& args)>;

// ============================================================================
// Tool Input Schema — JSON Schema for tool input parameters
// ============================================================================

struct ToolSchema {
    QString type = "object";
    QJsonObject properties; // { "param": { "type": "string", "description": "..." } }
    QStringList required;

    QJsonObject to_json() const {
        QJsonObject j;
        j["type"] = type;
        j["properties"] = properties;
        if (!required.isEmpty()) {
            QJsonArray req;
            for (const auto& r : required)
                req.append(r);
            j["required"] = req;
        }
        return j;
    }
};

// ============================================================================
// Tool Definition — a registered MCP tool
// ============================================================================

struct ToolDef {
    QString name;
    QString description;
    QString category; // navigation, trading, portfolio, market-data, analytics, system, exchange
    ToolSchema input_schema;
    ToolHandler handler;
    bool enabled = true;
};

// ============================================================================
// Server Status — for external MCP server management
// ============================================================================

enum class ServerStatus { Stopped, Running, Error };

inline const char* server_status_str(ServerStatus s) {
    switch (s) {
        case ServerStatus::Stopped:
            return "stopped";
        case ServerStatus::Running:
            return "running";
        case ServerStatus::Error:
            return "error";
    }
    return "unknown";
}

// ============================================================================
// External Tool — tool discovered from an external MCP server
// ============================================================================

struct ExternalTool {
    QString server_id;
    QString server_name;
    QString name;
    QString description;
    QJsonObject input_schema;
};

// ============================================================================
// Unified Tool — merged view for consumers (Chat, Agents, Node Editor)
// ============================================================================

struct UnifiedTool {
    QString server_id; // "fincept-terminal" for internal tools
    QString server_name;
    QString name;
    QString description;
    QJsonObject input_schema;
    bool is_internal = false;
};

// ============================================================================
// Constants
// ============================================================================

inline constexpr const char* INTERNAL_SERVER_ID = "fincept-terminal";
inline constexpr const char* INTERNAL_SERVER_NAME = "Fincept Terminal";

} // namespace fincept::mcp
