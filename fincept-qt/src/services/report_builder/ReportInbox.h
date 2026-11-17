#pragma once
// ReportInbox — the one way into the Report Builder from anywhere ("Send to report").
//
//   * append_blocks(target, payload, options): builds blocks from a neutral payload
//     (core/report/ReportBlocks.h) and appends them to
//       - the report open in the Report Builder (ReportTarget::current()) — ONE undo step
//         "Add from <source>", whether or not the Report Builder tab is open;
//       - a saved library report (ReportTarget::report(id)) — written to the library in
//         the background; the blocks are marked new and the Report Builder shows
//         "N new blocks" the next time that report is opened;
//       - a new report (blank or from a template) saved to the library.
//   * Thread-safe: call from any thread. It returns at once (never blocks); the work is
//     marshalled to the GUI thread, file / database work runs on a worker, and `done`
//     runs on the GUI thread — dropped if `ctx` was destroyed meanwhile.
//   * open_report(): switch to the Report Builder on a given report (and block).
//
// Qt Core types only, so services and other tabs can include it.

#include "core/report/ReportBlocks.h"
#include "core/report/ReportDocument.h"

#include <QMetaType>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

namespace fincept::services {

struct ReportTarget {
    enum class Kind { Current, Report, NewFromTemplate, NewBlank };
    Kind kind = Kind::Current;
    qint64 report_id = 0;  ///< Kind::Report: the library id (reports.id)
    QString template_name; ///< Kind::NewFromTemplate: a built-in template name, or "user:<id>"
    QString title;         ///< new reports: their title (default: the payload title)

    static ReportTarget current() { return {}; }
    static ReportTarget report(qint64 id) {
        ReportTarget t;
        t.kind = Kind::Report;
        t.report_id = id;
        return t;
    }
    static ReportTarget new_blank(const QString& title = {}) {
        ReportTarget t;
        t.kind = Kind::NewBlank;
        t.title = title;
        return t;
    }
    static ReportTarget new_from_template(const QString& template_name, const QString& title = {}) {
        ReportTarget t;
        t.kind = Kind::NewFromTemplate;
        t.template_name = template_name;
        t.title = title;
        return t;
    }
};

struct AppendOptions {
    QString source_label;        ///< "Derivatives", "Chat Mode" — undo text, provenance, notice
    bool reveal = false;         ///< switch to the Report Builder and select the first new block
    bool heading = true;         ///< payload title as a heading block
    bool provenance = true;      ///< "Source: <label> · as of <time>" caption after the blocks
    bool page_break_before = false;
};

struct AppendResult {
    bool ok = false;
    quint64 request_id = 0;
    qint64 report_id = 0;     ///< library id (0: the open report has not been saved to the library yet)
    QString report_title;
    QVector<int> block_ids;   ///< stable ids of the new blocks in that report
    bool in_open_report = false; ///< landed in the report open in the Report Builder (undoable there)
    QString error;            ///< plain sentence when !ok
};

/// One entry of an "Add to report" target picker.
struct ReportTargetInfo {
    ReportTarget target;
    QString title;
    QString detail;          ///< "open now · 12 blocks" / "saved 17 Nov 14:02"
    bool is_current = false;
};

class ReportInbox : public QObject {
    Q_OBJECT
  public:
    static ReportInbox& instance();

    using Done = std::function<void(const AppendResult&)>;
    using TargetsDone = std::function<void(const QVector<ReportTargetInfo>&)>;

    /// Build blocks from `payload` and append them to `target` (see the file comment).
    /// Returns the request id carried by the result. Any thread; never blocks.
    quint64 append_blocks(const ReportTarget& target, const report::ReportPayload& payload,
                          const AppendOptions& options = {}, QObject* ctx = nullptr, Done done = {});

    /// Same, with blocks the caller built (report::blocks::* builders); ids are ignored.
    quint64 append_components(const ReportTarget& target, const QVector<report::ReportComponent>& blocks,
                              const AppendOptions& options = {}, QObject* ctx = nullptr, Done done = {});

    /// Picker targets: the open report first, then up to `limit` recently saved library
    /// reports. Any thread; `done` runs on the GUI thread.
    void list_targets(int limit, QObject* ctx, TargetsDone done);

    /// Open the Report Builder tab on library report `report_id` (0 = the report that is
    /// open) and select block `select_block_id` (0 = none). Any thread.
    static void open_report(qint64 report_id = 0, int select_block_id = 0);

  signals:
    /// After every successful append (GUI thread).
    void blocks_appended(const fincept::services::AppendResult& result);

  private:
    ReportInbox();
    Q_DISABLE_COPY(ReportInbox)

    void run_append(quint64 id, const ReportTarget& target, const report::BuiltBlocks& built,
                    const AppendOptions& options, QObject* ctx, bool has_ctx, Done done);
};

} // namespace fincept::services

Q_DECLARE_METATYPE(fincept::services::AppendResult)
