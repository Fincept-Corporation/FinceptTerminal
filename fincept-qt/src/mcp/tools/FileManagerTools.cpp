// FileManagerTools.cpp — MCP tools for File Manager tab

#include "mcp/tools/FileManagerTools.h"

#include "core/logging/Logger.h"
#include "services/file_manager/FileManagerService.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
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

    // ── import_file_into_manager ────────────────────────────────────────
    {
        ToolDef t;
        t.name = "import_file_into_manager";
        t.description = "Copy an external file (any path on disk) into managed File Manager storage. "
                        "Returns the new file id, original name, size, and mime type.";
        t.category = "file_manager";
        t.is_destructive = true;
        t.input_schema.properties = QJsonObject{
            {"source_path", QJsonObject{{"type", "string"}, {"description", "Absolute path to the source file"}}},
            {"source_screen", QJsonObject{{"type", "string"},
                                          {"description", "Optional tag identifying which screen owns this file"}}}};
        t.input_schema.required = {"source_path"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString src = args["source_path"].toString().trimmed();
            const QString screen = args["source_screen"].toString().trimmed();
            if (src.isEmpty()) return ToolResult::fail("Missing 'source_path'");
            QFileInfo fi(src);
            if (!fi.exists() || !fi.isFile()) return ToolResult::fail("File not found at source_path");
            const QString id = services::FileManagerService::instance().import_file(src, screen);
            if (id.isEmpty()) return ToolResult::fail("Import failed");
            auto f = services::FileManagerService::instance().find_by_id(id);
            return ToolResult::ok("File imported", QJsonObject{
                {"id", id}, {"original_name", f.original_name},
                {"size", f.size}, {"mime_type", f.mime_type},
                {"source_screen", f.source_screen},
            });
        };
        tools.push_back(std::move(t));
    }

    // ── write_managed_text_file ─────────────────────────────────────────
    {
        ToolDef t;
        t.name = "write_managed_text_file";
        t.description = "Create a new managed text file with the given content (LLM-generated artifacts: reports, CSVs, code). "
                        "Pass an optional 'extension' to suggest the mime type (e.g. 'csv', 'md', 'json', 'py'). Returns the new file id.";
        t.category = "file_manager";
        t.is_destructive = true;
        t.input_schema.properties = QJsonObject{
            {"name", QJsonObject{{"type", "string"}, {"description", "Display name for the file (e.g. 'report.md')"}}},
            {"content", QJsonObject{{"type", "string"}, {"description", "UTF-8 text content to write"}}},
            {"source_screen", QJsonObject{{"type", "string"},
                                          {"description", "Optional source screen tag (e.g. 'report_builder')"}}}};
        t.input_schema.required = {"name", "content"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString name = args["name"].toString().trimmed();
            const QString content = args["content"].toString();
            const QString screen = args["source_screen"].toString().trimmed();
            if (name.isEmpty()) return ToolResult::fail("Missing 'name'");

            auto& svc = services::FileManagerService::instance();
            // Stage the content in a temp file inside storage_dir, then import.
            QDir dir(svc.storage_dir());
            if (!dir.exists() && !QDir().mkpath(dir.absolutePath()))
                return ToolResult::fail("Cannot create storage dir");

            const QString tmp_path = dir.filePath(QStringLiteral(".incoming_%1_%2")
                                                       .arg(QDateTime::currentMSecsSinceEpoch())
                                                       .arg(name));
            QFile f(tmp_path);
            if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
                return ToolResult::fail("Cannot open temp file for writing");
            {
                QTextStream out(&f);
                out.setEncoding(QStringConverter::Utf8);
                out << content;
            }
            f.close();

            // Rename to the desired display name so import sees the right
            // original-name (FileManagerService uses QFileInfo::fileName()).
            const QString staged = dir.filePath(name);
            QFile::remove(staged);
            QFile::rename(tmp_path, staged);

            const QString id = svc.import_file(staged, screen);
            QFile::remove(staged); // import_file copies; clean up staging
            if (id.isEmpty()) return ToolResult::fail("Import failed");
            auto rec = svc.find_by_id(id);
            return ToolResult::ok("File created", QJsonObject{
                {"id", id}, {"name", rec.original_name},
                {"size", rec.size}, {"mime_type", rec.mime_type},
                {"source_screen", rec.source_screen},
            });
        };
        tools.push_back(std::move(t));
    }

    // ── register_external_file ──────────────────────────────────────────
    {
        ToolDef t;
        t.name = "register_external_file";
        t.description = "Register a file already living inside the File Manager storage dir (advanced — most callers want import_file_into_manager).";
        t.category = "file_manager";
        t.is_destructive = true;
        t.input_schema.properties = QJsonObject{
            {"stored_name", QJsonObject{{"type", "string"}, {"description", "Filename inside storage_dir"}}},
            {"original_name", QJsonObject{{"type", "string"}, {"description", "Display name to show the user"}}},
            {"size", QJsonObject{{"type", "integer"}, {"description", "File size in bytes"}}},
            {"mime_type", QJsonObject{{"type", "string"}, {"description", "MIME type"}}},
            {"source_screen", QJsonObject{{"type", "string"}, {"description", "Optional source screen tag"}}}};
        t.input_schema.required = {"stored_name", "original_name", "size", "mime_type"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString sn = args["stored_name"].toString().trimmed();
            const QString on = args["original_name"].toString().trimmed();
            const qint64 sz = static_cast<qint64>(args["size"].toDouble());
            const QString mt = args["mime_type"].toString().trimmed();
            const QString screen = args["source_screen"].toString().trimmed();
            if (sn.isEmpty() || on.isEmpty() || mt.isEmpty())
                return ToolResult::fail("Missing required fields");
            const QString id = services::FileManagerService::instance().register_file(sn, on, sz, mt, screen);
            if (id.isEmpty()) return ToolResult::fail("Register failed");
            return ToolResult::ok("File registered", QJsonObject{{"id", id}});
        };
        tools.push_back(std::move(t));
    }

    // ── download_managed_file ───────────────────────────────────────────
    {
        ToolDef t;
        t.name = "download_managed_file";
        t.description = "Copy a managed file from internal storage to an external path on disk.";
        t.category = "file_manager";
        t.is_destructive = true;
        t.input_schema.properties = QJsonObject{
            {"id", QJsonObject{{"type", "string"}, {"description", "Managed file id"}}},
            {"destination_path", QJsonObject{{"type", "string"},
                                             {"description", "Absolute output path. Parent must exist."}}}};
        t.input_schema.required = {"id", "destination_path"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString id = args["id"].toString().trimmed();
            const QString dest = args["destination_path"].toString().trimmed();
            if (id.isEmpty() || dest.isEmpty()) return ToolResult::fail("Missing 'id' or 'destination_path'");

            auto& svc = services::FileManagerService::instance();
            auto f = svc.find_by_id(id);
            if (f.id.isEmpty()) return ToolResult::fail("File not found: " + id);
            const QString src = svc.full_path(f.name);
            QFile::remove(dest);
            if (!QFile::copy(src, dest)) return ToolResult::fail("Copy failed");
            return ToolResult::ok("File downloaded", QJsonObject{
                {"id", id}, {"destination_path", dest},
                {"size", f.size},
            });
        };
        tools.push_back(std::move(t));
    }

    // ── bulk_delete_managed_files ───────────────────────────────────────
    {
        ToolDef t;
        t.name = "bulk_delete_managed_files";
        t.description = "Delete multiple managed files in one call. Returns counts of deleted and failed ids.";
        t.category = "file_manager";
        t.is_destructive = true;
        t.input_schema.properties = QJsonObject{
            {"ids", QJsonObject{{"type", "array"},
                                {"items", QJsonObject{{"type", "string"}}},
                                {"description", "Array of managed file ids to delete"}}}};
        t.input_schema.required = {"ids"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QJsonArray ids = args["ids"].toArray();
            if (ids.isEmpty()) return ToolResult::fail("'ids' is empty");
            auto& svc = services::FileManagerService::instance();
            QJsonArray deleted, failed;
            for (const auto& v : ids) {
                const QString id = v.toString().trimmed();
                if (id.isEmpty()) continue;
                if (svc.remove_file(id)) deleted.append(id);
                else failed.append(id);
            }
            return ToolResult::ok(QString("Deleted %1 of %2 files").arg(deleted.size()).arg(ids.size()),
                                   QJsonObject{
                                       {"deleted_count", deleted.size()},
                                       {"failed_count", failed.size()},
                                       {"deleted_ids", deleted},
                                       {"failed_ids", failed},
                                   });
        };
        tools.push_back(std::move(t));
    }

    // ── get_file_manager_stats ──────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_file_manager_stats";
        t.description = "Get File Manager runtime stats — total file count, total bytes used, 500 MB quota usage percentage, and counts grouped by mime/source_screen.";
        t.category = "file_manager";
        t.handler = [](const QJsonObject&) -> ToolResult {
            auto& svc = services::FileManagerService::instance();
            const QJsonArray all = svc.all_files();
            qint64 total_bytes = 0;
            QHash<QString, int> by_mime;
            QHash<QString, int> by_screen;
            for (const auto& v : all) {
                const QJsonObject o = v.toObject();
                total_bytes += static_cast<qint64>(o["size"].toDouble());
                const QString mime = o["type"].toString();
                if (!mime.isEmpty()) by_mime[mime]++;
                const QString s = o["sourceScreen"].toString();
                if (!s.isEmpty()) by_screen[s]++;
            }
            constexpr qint64 kQuotaBytes = 500LL * 1024 * 1024;
            QJsonObject mime_obj;
            for (auto it = by_mime.constBegin(); it != by_mime.constEnd(); ++it) mime_obj[it.key()] = it.value();
            QJsonObject screen_obj;
            for (auto it = by_screen.constBegin(); it != by_screen.constEnd(); ++it) screen_obj[it.key()] = it.value();
            return ToolResult::ok_data(QJsonObject{
                {"file_count", all.size()},
                {"total_bytes", total_bytes},
                {"quota_bytes", static_cast<qint64>(kQuotaBytes)},
                {"quota_used_pct", kQuotaBytes > 0
                                       ? (double)total_bytes / (double)kQuotaBytes * 100.0
                                       : 0.0},
                {"by_mime", mime_obj},
                {"by_source_screen", screen_obj},
            });
        };
        tools.push_back(std::move(t));
    }

    // ── get_managed_file_path ───────────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_managed_file_path";
        t.description = "Get the full disk path for a managed file id. Useful for tools (e.g. Python scripts) that need to read raw bytes.";
        t.category = "file_manager";
        t.input_schema.properties = QJsonObject{
            {"id", QJsonObject{{"type", "string"}, {"description", "Managed file id"}}}};
        t.input_schema.required = {"id"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString id = args["id"].toString().trimmed();
            if (id.isEmpty()) return ToolResult::fail("Missing 'id'");
            auto& svc = services::FileManagerService::instance();
            auto f = svc.find_by_id(id);
            if (f.id.isEmpty()) return ToolResult::fail("File not found: " + id);
            const QString full = svc.full_path(f.name);
            QFileInfo fi(full);
            return ToolResult::ok_data(QJsonObject{
                {"id", id}, {"full_path", full},
                {"exists_on_disk", fi.exists()},
            });
        };
        tools.push_back(std::move(t));
    }

    // ── get_file_manager_storage_dir ────────────────────────────────────
    {
        ToolDef t;
        t.name = "get_file_manager_storage_dir";
        t.description = "Get the absolute path of the File Manager storage root directory.";
        t.category = "file_manager";
        t.handler = [](const QJsonObject&) -> ToolResult {
            return ToolResult::ok_data(QJsonObject{
                {"storage_dir", services::FileManagerService::instance().storage_dir()},
            });
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
