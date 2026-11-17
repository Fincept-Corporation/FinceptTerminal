#pragma once
// ReportTool.h — the `report` core tool: agents read and build Report Builder reports.
//
// One tool with six actions (the terminal keeps a small tool catalogue — see CoreTools.h):
//   list_reports         the open report + saved library reports (title, id, blocks, updated)
//   get_report           a report's outline: info, variables, every block with its id, type,
//                        a short text and its live-data recipe          (read — no approval)
//   append_blocks        add blocks from Markdown ("#" headings, pipe tables, ``` code) —
//                        through ReportInbox, one undo step                         (approval)
//   update_block_text    replace one text-type block's text — one undo step         (approval)
//   refresh_report_data  refresh every live-data block from the terminal's sources  (approval)
//   export_report        write PDF / .odt / Markdown / HTML to a file                (approval)
// Write actions ask the user first through the approval dialog every other write tool uses
// (CoreApproval), with a description of exactly what will change; a refusal changes nothing.
// Results are capped (≈16 KB). report_id 0 / absent = the report open in the Report Builder.
//
// ReportToolHandler holds the logic over an abstract backend (tests use a fake one);
// make_report_tool() wires it to the real services (ReportToolBackend.cpp).

#include "core/report/ReportDocument.h"
#include "mcp/McpTypes.h"
#include "mcp/server/ToolApprovalDialog.h"

#include <QJsonObject>
#include <QString>
#include <QVector>

#include <functional>
#include <memory>

namespace fincept::mcp::core {

inline constexpr const char* kReportToolName = "report";

/// What the tool needs from the terminal. Every callback runs on the GUI thread.
class ReportToolBackend {
  public:
    virtual ~ReportToolBackend() = default;

    struct ListEntry {
        qint64 id = 0;
        QString title;
        QString updated; ///< SQLite UTC text
        int blocks = 0;
        bool open = false;
    };
    virtual void list_reports(const QString& query, int limit,
                              std::function<void(const QVector<ListEntry>& entries, const QString& error)> done) = 0;
    /// `id` 0 = the open report. `resolved_id` = its library id (0 when not saved yet).
    virtual void load(qint64 id, std::function<void(bool ok, const report::ReportDocument& doc, qint64 resolved_id,
                                                    bool is_open, const QString& error)> done) = 0;
    virtual void append(qint64 id, const QVector<report::ReportComponent>& blocks, const QString& source_label,
                        std::function<void(bool ok, const QVector<int>& block_ids, const QString& title,
                                           const QString& error)> done) = 0;
    virtual void update_text(qint64 id, int block_id, const QString& text,
                             std::function<void(bool ok, const QString& error)> done) = 0;
    virtual void refresh(qint64 id, std::function<void(bool ok, int refreshed, int failed, const QString& error)> done) = 0;
    virtual void export_report(qint64 id, const QString& format, const QString& path,
                               std::function<void(bool ok, const QString& written, const QStringList& warnings,
                                                  const QString& error)> done) = 0;
    virtual void ask_approval(const ToolApprovalRequest& request, std::function<void(ToolApprovalVerdict)> done) = 0;
    /// Folder for exports when the agent names no path.
    virtual QString default_export_dir() const = 0;
    virtual bool file_exists(const QString& path) const = 0;
};

class ReportToolHandler {
  public:
    explicit ReportToolHandler(std::shared_ptr<ReportToolBackend> backend) : backend_(std::move(backend)) {}
    /// GUI thread. `origin` = the calling client (shown in approval dialogs). `reply` once.
    void call(const QJsonObject& args, const QString& origin, std::function<void(ToolResult)> reply);

    // Pure helpers (unit-tested).
    static QJsonObject outline_json(const report::ReportDocument& doc, qint64 id, bool is_open, int max_blocks);
    static QString block_summary(const report::ReportComponent& c, int max_chars);
    static QString describe_blocks(const QVector<report::ReportComponent>& blocks);
    static bool is_text_block(const QString& type);
    static QString normalise_export_path(const QString& requested, const QString& default_dir, const QString& title,
                                         const QString& ext, QString* error);

  private:
    std::shared_ptr<ReportToolBackend> backend_;
};

/// The registered tool (CoreTools.cpp adds it to the catalogue).
ToolDef make_report_tool();

} // namespace fincept::mcp::core
