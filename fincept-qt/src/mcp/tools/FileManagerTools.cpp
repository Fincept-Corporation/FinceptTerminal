// FileManagerTools.cpp — MCP tools for File Manager tab

#include "mcp/tools/FileManagerTools.h"

#include "core/logging/Logger.h"
#include "services/file_manager/FileManagerService.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace fincept::mcp::tools {

std::vector<ToolDef> get_file_manager_tools() {
    std::vector<ToolDef> tools;

    // ── list_files ──────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "list_files";
        t.description = "List all files managed by the File Manager. "
                        "Optionally filter by source screen (e.g. 'excel', 'code_editor', "
                        "'report_builder', 'notes') or MIME type fragment (e.g. 'pdf', 'csv').";
        t.category = "file_manager";
        t.input_schema.properties = QJsonObject{
            {"screen",
             QJsonObject{{"type", "string"},
                         {"description", "Filter by source screen: excel, code_editor, report_builder, notes"}}},
            {"mime", QJsonObject{{"type", "string"},
                                 {"description", "Filter by MIME type fragment, e.g. 'pdf', 'csv', 'ipynb'"}}}};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            auto& svc = services::FileManagerService::instance();
            QString screen = args["screen"].toString().trimmed();
            QString mime = args["mime"].toString().trimmed();

            QJsonArray files;
            if (!screen.isEmpty())
                files = svc.files_for_screen(screen);
            else if (!mime.isEmpty())
                files = svc.files_by_mime(mime);
            else
                files = svc.all_files();

            // Return a trimmed view — omit internal stored name, expose useful fields only
            QJsonArray result;
            for (const auto& v : files) {
                if (!v.isObject())
                    continue;
                QJsonObject obj = v.toObject();
                result.append(QJsonObject{{"id", obj["id"]},
                                          {"name", obj["originalName"]},
                                          {"size", obj["size"]},
                                          {"type", obj["type"]},
                                          {"uploadedAt", obj["uploadedAt"]},
                                          {"sourceScreen", obj["sourceScreen"]}});
            }
            return ToolResult::ok_data(result);
        };
        tools.push_back(std::move(t));
    }

    // ── get_file_info ───────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_file_info";
        t.description = "Get detailed info about a specific managed file by its ID.";
        t.category = "file_manager";
        t.input_schema.properties =
            QJsonObject{{"id", QJsonObject{{"type", "string"}, {"description", "File ID returned by list_files"}}}};
        t.input_schema.required = {"id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString id = args["id"].toString().trimmed();
            if (id.isEmpty())
                return ToolResult::fail("Missing 'id'");

            auto f = services::FileManagerService::instance().find_by_id(id);
            if (f.id.isEmpty())
                return ToolResult::fail("File not found: " + id);

            return ToolResult::ok_data(
                QJsonObject{{"id", f.id},
                            {"name", f.original_name},
                            {"storedName", f.name},
                            {"size", f.size},
                            {"type", f.mime_type},
                            {"uploadedAt", f.uploaded_at},
                            {"sourceScreen", f.source_screen},
                            {"fullPath", services::FileManagerService::instance().full_path(f.name)}});
        };
        tools.push_back(std::move(t));
    }

    // ── delete_file ─────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "delete_managed_file";
        t.description = "Delete a managed file by its ID. This removes it from the File Manager "
                        "and deletes the stored copy from disk.";
        t.category = "file_manager";
        t.input_schema.properties =
            QJsonObject{{"id", QJsonObject{{"type", "string"}, {"description", "File ID to delete"}}}};
        t.input_schema.required = {"id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString id = args["id"].toString().trimmed();
            if (id.isEmpty())
                return ToolResult::fail("Missing 'id'");

            bool ok = services::FileManagerService::instance().remove_file(id);
            if (!ok)
                return ToolResult::fail("File not found or could not be deleted: " + id);

            return ToolResult::ok("File deleted: " + id);
        };
        tools.push_back(std::move(t));
    }

    // ── read_file_contents ──────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "read_file_contents";
        t.description = "Read the text contents of a managed file by its ID. Supports plain text, CSV, JSON, "
                        "Markdown, and any other UTF-8 text format. Binary files (images, zip, xlsx) are not "
                        "readable and will return an error. Use list_files first to find the file ID. "
                        "For large files, use the 'max_chars' parameter to limit the returned content.";
        t.category = "file_manager";
        t.input_schema.properties = QJsonObject{
            {"id", QJsonObject{{"type", "string"}, {"description", "File ID returned by list_files"}}},
            {"max_chars", QJsonObject{{"type", "integer"},
                                      {"description", "Maximum characters to return (default 32000, max 128000)"}}}};
        t.input_schema.required = {"id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString id = args["id"].toString().trimmed();
            if (id.isEmpty())
                return ToolResult::fail("Missing 'id'");

            auto f = services::FileManagerService::instance().find_by_id(id);
            if (f.id.isEmpty())
                return ToolResult::fail("File not found: " + id);

            // Reject known binary MIME types
            const QString& mime = f.mime_type;
            if (mime.contains("image") || mime.contains("video") || mime.contains("audio") || mime.contains("zip") ||
                mime.contains("archive") || mime.contains("compressed") || mime.contains("octet-stream") ||
                mime.contains("spreadsheet") || mime.contains("excel") || mime.contains("vnd.openxmlformats")) {
                return ToolResult::fail("File type '" + mime + "' is binary and cannot be read as text.");
            }

            QString path = services::FileManagerService::instance().full_path(f.name);
            QFile file(path);
            if (!file.exists())
                return ToolResult::fail("File not found on disk: " + path);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                return ToolResult::fail("Cannot open file: " + path);

            QTextStream in(&file);
            in.setEncoding(QStringConverter::Utf8);

            int max_chars = args["max_chars"].toInt(32000);
            if (max_chars <= 0 || max_chars > 128000)
                max_chars = 32000;

            QString content = in.read(max_chars);
            file.close();

            bool truncated = (content.length() == max_chars);

            LOG_INFO("FileManagerTools",
                     "read_file_contents: " + f.original_name + " (" + QString::number(content.length()) + " chars)");

            return ToolResult::ok_data(QJsonObject{{"id", f.id},
                                                   {"name", f.original_name},
                                                   {"type", f.mime_type},
                                                   {"sourceScreen", f.source_screen},
                                                   {"uploadedAt", f.uploaded_at},
                                                   {"sizeBytes", f.size},
                                                   {"truncated", truncated},
                                                   {"content", content}});
        };
        tools.push_back(std::move(t));
    }

    // ── search_files ────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "search_files";
        t.description = "Search managed files by name keyword, source screen, MIME type, or date range. "
                        "Returns matching file metadata (not contents — use read_file_contents for that). "
                        "All parameters are optional and combined with AND logic.";
        t.category = "file_manager";
        t.input_schema.properties = QJsonObject{
            {"query",
             QJsonObject{{"type", "string"}, {"description", "Keyword to match against file name (case-insensitive)"}}},
            {"screen", QJsonObject{{"type", "string"},
                                   {"description", "Source screen: excel, portfolio, backtesting, news, "
                                                   "equity_research, algo_trading, ai_quant_lab, notes, "
                                                   "code_editor, report_builder, data_sources"}}},
            {"mime", QJsonObject{{"type", "string"},
                                 {"description", "MIME type fragment to match, e.g. 'csv', 'json', 'pdf'"}}},
            {"after", QJsonObject{{"type", "string"},
                                  {"description", "ISO 8601 date — only return files uploaded after this date"}}},
            {"before", QJsonObject{{"type", "string"},
                                   {"description", "ISO 8601 date — only return files uploaded before this date"}}}};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            auto& svc = services::FileManagerService::instance();
            QJsonArray all = svc.all_files();

            QString query = args["query"].toString().trimmed().toLower();
            QString screen = args["screen"].toString().trimmed().toLower();
            QString mime = args["mime"].toString().trimmed().toLower();
            QString after = args["after"].toString().trimmed();
            QString before = args["before"].toString().trimmed();

            QDateTime dt_after = after.isEmpty() ? QDateTime() : QDateTime::fromString(after, Qt::ISODate);
            QDateTime dt_before = before.isEmpty() ? QDateTime() : QDateTime::fromString(before, Qt::ISODate);

            QJsonArray result;
            for (const auto& v : all) {
                if (!v.isObject())
                    continue;
                QJsonObject obj = v.toObject();

                if (!query.isEmpty() && !obj["originalName"].toString().toLower().contains(query))
                    continue;
                if (!screen.isEmpty() && !obj["sourceScreen"].toString().toLower().contains(screen))
                    continue;
                if (!mime.isEmpty() && !obj["type"].toString().toLower().contains(mime))
                    continue;
                if (dt_after.isValid()) {
                    QDateTime uploaded = QDateTime::fromString(obj["uploadedAt"].toString(), Qt::ISODate);
                    if (uploaded < dt_after)
                        continue;
                }
                if (dt_before.isValid()) {
                    QDateTime uploaded = QDateTime::fromString(obj["uploadedAt"].toString(), Qt::ISODate);
                    if (uploaded > dt_before)
                        continue;
                }

                result.append(QJsonObject{{"id", obj["id"]},
                                          {"name", obj["originalName"]},
                                          {"size", obj["size"]},
                                          {"type", obj["type"]},
                                          {"uploadedAt", obj["uploadedAt"]},
                                          {"sourceScreen", obj["sourceScreen"]}});
            }

            if (result.isEmpty())
                return ToolResult::ok_data(QJsonObject{{"matches", 0}, {"files", QJsonArray{}}});

            return ToolResult::ok_data(QJsonObject{{"matches", result.size()}, {"files", result}});
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
