#pragma once
// ReportRegenerator — refresh a library report's data and export it, without the user at
// the keyboard (scheduled runs, "Run now", the MCP report tool's export).
//
//   * The report open in the Report Builder: its bound blocks are refreshed through the
//     data binder (each refresh is an undoable step of the open report, as when the user
//     presses Refresh Data), then a snapshot is exported.
//   * Any other library report: it is loaded on the library worker, every bound block is
//     fetched with the binder's one-shot fetch (same sources, limits and time-outs) and the
//     new numbers are written into a copy, which is saved back to its row with the revision
//     check (a report changed on another computer meanwhile is not overwritten — the run
//     says so) and exported.
// Exports go through screens::ReportExportJob (sliced on the GUI thread for PDF / HTML,
// a worker for .odt / Markdown). Nothing here blocks the GUI thread. AI drafting is never
// part of a run.

#include "core/report/ReportDocument.h"
#include "core/report/ReportSchedule.h"
#include "services/report_builder/ReportDataBinder.h"

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <functional>

namespace fincept::services {

struct RegenerateRequest {
    qint64 report_id = 0;    ///< library id; 0 = the report open in the Report Builder
    bool refresh_data = true;
    QStringList formats;     ///< "pdf" | "odt" | "md" | "html"
    QString folder;          ///< files are named "<title> <yyyy-MM-dd>.<ext>" in it …
    QString explicit_path;   ///< … unless one exact path is given (single format)
    QString trigger;         ///< scheduled | missed | start | manual | mcp
};

class ReportRegenerator : public QObject {
    Q_OBJECT
  public:
    using Done = std::function<void(const report::schedule::RunRecord& record)>;

    /// Start a run; `done` runs once on the GUI thread (dropped when `ctx` died). The job
    /// deletes itself.
    static void run(const RegenerateRequest& request, QObject* ctx, Done done);

  private:
    ReportRegenerator(const RegenerateRequest& request, QObject* ctx, Done done);
    void start();
    void use_open_report();
    void refresh_copy();
    void apply_fetch(int component_id, const BindFetchResult& r, const report::Binding& b);
    void save_copy_then_export();
    void export_next();
    void complete(bool ok, const QString& error);

    RegenerateRequest req_;
    QPointer<QObject> ctx_;
    bool has_ctx_ = false;
    Done done_;
    report::ReportDocument doc_;
    report::schedule::RunRecord record_;
    int pending_fetches_ = 0;
    int failed_fetches_ = 0;
    int export_index_ = 0;
    bool open_report_ = false;
    bool finished_ = false;
};

} // namespace fincept::services
