#include "services/workflow/nodes/IntegrationNodes.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "services/workflow/NodeRegistry.h"

using fincept::python::PythonRunner;

#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>

namespace fincept::workflow {

void register_integration_nodes(NodeRegistry& registry) {
    // ── MCP Tool Call ──────────────────────────────────────────────
    // Single node that routes to any internal Fincept MCP tool.
    // "tool" parameter accepts:
    //   - bare tool name (e.g. "get_quote")        → routed to fincept-terminal server
    //   - "serverId__toolName" OpenAI function form → routed via execute_openai_function
    registry.register_type({
        .type_id = "mcp.tool_call",
        .display_name = "MCP Tool",
        .category = "MCP",
        .description = "Call any Fincept internal MCP tool by name",
        .icon_text = "M",
        .accent_color = "#6366f1",
        .version = 2,
        .inputs = {{"input_0", "Arguments", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Result", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"tool", "Tool", "mcp_tool_select", "", {}, "", true},
            },
        .execute = nullptr,   // wired in ServiceBridges::wire_all_bridges
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

    // ── Spreadsheet (local xlsx/csv + Google Sheets via Python) ───────
    registry.register_type({
        .type_id = "utility.google_sheets",
        .display_name = "Spreadsheet",
        .category = "Utilities",
        .description = "Read/write local .xlsx/.csv or Google Sheets (public/private)",
        .icon_text = "#",
        .accent_color = "#16a34a",
        .version = 3,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"source",           "Source",           "select", "local",  {"local", "google"}, ""},
                {"operation",        "Operation",        "select", "read",   {"read", "write", "append", "clear"}, ""},
                {"file_path",        "File Path",        "file_managed", "", {}, "xlsx,csv"},
                {"spreadsheet_id",   "Spreadsheet ID",   "string",       "", {}, "Google Sheet ID from URL (source=google)"},
                {"sheet_name",       "Sheet Name",       "string",       "Sheet1", {}, ""},
                {"range",            "Range",            "string",       "A1:Z1000", {}, "Cell range e.g. A1:D100"},
                {"credentials_path", "Credentials JSON", "file_managed", "", {}, "json"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {

                // Build args for spreadsheet.py
                QJsonObject args;
                args["source"]           = params.value("source").toString("local");
                args["operation"]        = params.value("operation").toString("read");
                args["file_path"]        = params.value("file_path").toString();
                args["spreadsheet_id"]   = params.value("spreadsheet_id").toString();
                args["sheet_name"]       = params.value("sheet_name").toString("Sheet1");
                args["range"]            = params.value("range").toString("A1:Z1000");
                args["credentials_path"] = params.value("credentials_path").toString();

                // Attach input data for write/append operations
                if (!inputs.isEmpty())
                    args["data"] = inputs[0];

                QString args_json = QString::fromUtf8(
                    QJsonDocument(args).toJson(QJsonDocument::Compact));

                PythonRunner::instance().run(
                    "spreadsheet.py", {"--args", args_json},
                    [cb](const fincept::python::PythonResult& res) {
                        if (!res.success) {
                            cb(false, {}, res.error.isEmpty() ? "spreadsheet.py failed" : res.error);
                            return;
                        }
                        QString out = fincept::python::extract_json(res.output).trimmed();
                        auto doc = QJsonDocument::fromJson(out.toUtf8());
                        if (doc.isNull()) { cb(false, {}, "Invalid JSON from spreadsheet.py: " + res.output.left(200)); return; }
                        if (doc.isObject() && doc.object().contains("error")) {
                            cb(false, {}, doc.object().value("error").toString());
                            return;
                        }
                        cb(true, doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array()), {});
                    });
            },
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
