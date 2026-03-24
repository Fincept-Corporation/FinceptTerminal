#include "services/workflow/nodes/IntegrationNodes.h"

#include "services/workflow/NodeRegistry.h"

namespace fincept::workflow {

void register_integration_nodes(NodeRegistry& registry) {
    // ── MCP Tool Call ──────────────────────────────────────────────
    registry.register_type({
        .type_id = "mcp.tool_call",
        .display_name = "MCP Tool",
        .category = "MCP Tools",
        .description = "Call any registered MCP server tool",
        .icon_text = "M",
        .accent_color = "#6366f1",
        .version = 1,
        .inputs = {{"input_0", "Context", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Result", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"server", "MCP Server", "string", "", {}, "Server name or URL", true},
                {"tool", "Tool Name", "string", "", {}, "Tool to invoke", true},
                {"arguments", "Arguments", "json", QJsonObject{}, {}, "Tool arguments as JSON"},
                {"timeout_sec", "Timeout (sec)", "number", 30, {}, ""},
            },
        .execute = nullptr,
    });

    // ── Python Script ──────────────────────────────────────────────
    registry.register_type({
        .type_id = "mcp.python_script",
        .display_name = "Python Script",
        .category = "MCP Tools",
        .description = "Run a specific Python script from scripts/ directory",
        .icon_text = "M",
        .accent_color = "#6366f1",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Result", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"script", "Script Path", "string", "", {}, "e.g. equity_talipp.py", true},
                {"function", "Function", "string", "main", {}, "Function to call"},
                {"args", "Arguments", "json", QJsonObject{}, {}, "Script arguments"},
            },
        .execute = nullptr,
    });

    // ── Push Notification ──────────────────────────────────────────
    registry.register_type({
        .type_id = "notify.push",
        .display_name = "Push Notification",
        .category = "Notifications",
        .description = "Send mobile push notification",
        .icon_text = "@",
        .accent_color = "#ca8a04",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"title", "Title", "string", "", {}, "Notification title", true},
                {"message", "Message", "string", "", {}, "Notification body", true},
                {"priority", "Priority", "select", "normal", {"low", "normal", "high", "urgent"}, ""},
                {"service", "Service", "select", "pushover", {"pushover", "ntfy", "pushbullet"}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QJsonObject out;
                out["title"] = params.value("title").toString();
                out["message"] = params.value("message").toString();
                out["sent"] = true;
                if (!inputs.isEmpty())
                    out["input"] = inputs[0];
                cb(true, out, {});
            },
    });

    // ── Audio Alert ────────────────────────────────────────────────
    registry.register_type({
        .type_id = "notify.audio_alert",
        .display_name = "Audio Alert",
        .category = "Notifications",
        .description = "Play a sound alert on the local machine",
        .icon_text = "@",
        .accent_color = "#ca8a04",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"sound", "Sound", "select", "alert", {"alert", "success", "error", "notification", "custom"}, ""},
                {"repeat", "Repeat", "number", 1, {}, "Times to play"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QJsonObject out;
                out["sound"] = params.value("sound").toString();
                out["played"] = true;
                if (!inputs.isEmpty())
                    out["input"] = inputs[0];
                cb(true, out, {});
            },
    });

    // ── PDF Generate ───────────────────────────────────────────────
    registry.register_type({
        .type_id = "file.pdf_generate",
        .display_name = "PDF Report",
        .category = "Files",
        .description = "Generate PDF report from data",
        .icon_text = "F",
        .accent_color = "#525252",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"title", "Report Title", "string", "Report", {}, "", true},
                {"template", "Template", "select", "default", {"default", "financial", "research", "summary"}, ""},
                {"path", "Output Path", "string", "output/report.pdf", {}, "", true},
                {"include_charts", "Include Charts", "boolean", true, {}, ""},
            },
        .execute = nullptr,
    });

    // ── Image Chart ────────────────────────────────────────────────
    registry.register_type({
        .type_id = "file.image_chart",
        .display_name = "Chart Image",
        .category = "Files",
        .description = "Generate chart as PNG image",
        .icon_text = "F",
        .accent_color = "#525252",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"chart_type",
                 "Chart Type",
                 "select",
                 "line",
                 {"line", "bar", "candlestick", "scatter", "pie", "heatmap"},
                 ""},
                {"title", "Title", "string", "", {}, "Chart title"},
                {"width", "Width", "number", 800, {}, "Pixels"},
                {"height", "Height", "number", 600, {}, "Pixels"},
                {"path", "Output Path", "string", "output/chart.png", {}, ""},
            },
        .execute = nullptr,
    });

    // ── Google Sheets ──────────────────────────────────────────────
    registry.register_type({
        .type_id = "utility.google_sheets",
        .display_name = "Google Sheets",
        .category = "Utilities",
        .description = "Read/write Google Sheets",
        .icon_text = "#",
        .accent_color = "#808080",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"operation", "Operation", "select", "read", {"read", "write", "append", "clear"}, ""},
                {"spreadsheet_id", "Spreadsheet ID", "string", "", {}, "Google Sheet ID", true},
                {"sheet_name", "Sheet Name", "string", "Sheet1", {}, ""},
                {"range", "Range", "string", "A1:Z1000", {}, "Cell range"},
            },
        .execute = nullptr,
    });

    // ── Generic API Call ───────────────────────────────────────────
    registry.register_type({
        .type_id = "utility.api_call",
        .display_name = "API Call",
        .category = "Utilities",
        .description = "REST API call with authentication",
        .icon_text = "#",
        .accent_color = "#808080",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"url", "URL", "string", "", {}, "Full API endpoint URL", true},
                {"method", "Method", "select", "GET", {"GET", "POST", "PUT", "PATCH", "DELETE"}, ""},
                {"auth_type", "Auth Type", "select", "none", {"none", "bearer", "api_key", "basic", "oauth2"}, ""},
                {"auth_value", "Auth Value", "string", "", {}, "Token / API key / username:password"},
                {"headers", "Headers", "json", QJsonObject{}, {}, "Custom headers"},
                {"body", "Body", "json", QJsonObject{}, {}, "Request body"},
                {"timeout_sec", "Timeout (sec)", "number", 30, {}, ""},
                {"retry_count", "Retries", "number", 0, {}, "Auto-retry on failure"},
            },
        .execute = nullptr,
    });
}

} // namespace fincept::workflow
