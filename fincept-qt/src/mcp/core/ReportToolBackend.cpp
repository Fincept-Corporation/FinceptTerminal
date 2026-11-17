// ReportToolBackend.cpp — the `report` tool wired to the Report Builder services, and its
// ToolDef. All service calls happen on the GUI thread (the tool posts itself there); the
// caller's thread is never blocked.

#include "core/logging/Logger.h"
#include "mcp/ToolSchemaBuilder.h"
#include "mcp/core/CoreApproval.h"
#include "mcp/core/CoreTools.h"
#include "mcp/core/CoreToolsCommon.h"
#include "mcp/core/ReportTool.h"
#include "mcp/server/CallApproval.h"
#include "services/report_builder/ReportBuilderService.h"
#include "services/report_builder/ReportDataBinder.h"
#include "services/report_builder/ReportInbox.h"
#include "services/report_builder/ReportLibrary.h"
#include "services/report_builder/ReportRegenerator.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QStandardPaths>
#include <QTimer>

namespace fincept::mcp::core {

namespace rep = ::fincept::report;
namespace svc = ::fincept::services;

namespace {

constexpr int kRtbRefreshWaitMs = 100'000; // inside the tool's own budget

class RtbBackend final : public ReportToolBackend {
  public:
    void list_reports(const QString& query, int limit,
                      std::function<void(const QVector<ListEntry>&, const QString&)> done) override {
        auto& s = svc::ReportBuilderService::instance();
        QVector<ListEntry> out;
        const qint64 open_id = s.library_id();
        if (query.trimmed().isEmpty() || s.metadata().title.contains(query, Qt::CaseInsensitive))
            out.append({open_id, s.metadata().title, QString(), static_cast<int>(s.components().size()), true});
        svc::ReportLibrary::instance().list(
            query, limit, qApp,
            [done, out, open_id](bool ok, const QVector<svc::LibraryEntry>& entries, const QString& error) mutable {
                if (ok)
                    for (const svc::LibraryEntry& e : entries)
                        if (e.id != open_id)
                            out.append({e.id, e.title, e.updated_at, e.blocks, false});
                done(out, ok ? QString() : error);
            });
    }

    void load(qint64 id, std::function<void(bool, const rep::ReportDocument&, qint64, bool, const QString&)> done) override {
        auto& s = svc::ReportBuilderService::instance();
        if (id <= 0 || id == s.library_id()) {
            done(true, s.snapshot(), s.library_id(), true, QString());
            return;
        }
        svc::ReportLibrary::instance().load(id, qApp,
                                            [done, id](bool ok, const rep::ReportDocument& doc, const QString&,
                                                       const QString& error) { done(ok, doc, id, false, error); });
    }

    void append(qint64 id, const QVector<rep::ReportComponent>& blocks, const QString& source_label,
                std::function<void(bool, const QVector<int>&, const QString&, const QString&)> done) override {
        auto& s = svc::ReportBuilderService::instance();
        const svc::ReportTarget target =
            id <= 0 || id == s.library_id() ? svc::ReportTarget::current() : svc::ReportTarget::report(id);
        svc::AppendOptions o;
        o.source_label = source_label;
        o.provenance = false; // the agent's text is not a data source
        svc::ReportInbox::instance().append_components(
            target, blocks, o, qApp,
            [done](const svc::AppendResult& r) { done(r.ok, r.block_ids, r.report_title, r.error); });
    }

    void update_text(qint64 id, int block_id, const QString& text,
                     std::function<void(bool, const QString&)> done) override {
        auto& s = svc::ReportBuilderService::instance();
        if (id <= 0 || id == s.library_id()) {
            // One undo step in the open report.
            const bool ok = s.patch_component(block_id, &text, {}, QCoreApplication::translate("ReportTool", "AI agent: edit text"),
                                              /*mergeable=*/false);
            done(ok, ok ? QString() : QStringLiteral("the block no longer exists"));
            return;
        }
        // A saved report: load, change, write back with the revision check.
        svc::ReportLibrary::instance().load(
            id, qApp, [done, id, block_id, text](bool ok, const rep::ReportDocument& doc, const QString& title,
                                                 const QString& error) {
                if (!ok) {
                    done(false, error);
                    return;
                }
                rep::ReportDocument d = doc;
                const int idx = d.index_of(block_id);
                if (idx < 0) {
                    done(false, QStringLiteral("the block no longer exists"));
                    return;
                }
                d.components[idx].content = text;
                svc::ReportLibrary::instance().save_checked(
                    id, title.isEmpty() ? d.metadata.title : title, d.to_json(false), d.revision, false, qApp,
                    [done](const svc::LibrarySaveOutcome& out) {
                        if (out.conflict)
                            done(false, QStringLiteral("the report was changed on another computer meanwhile; read it "
                                                       "again with get_report"));
                        else
                            done(out.ok, out.error);
                    });
            });
    }

    void refresh(qint64 id, std::function<void(bool, int, int, const QString&)> done) override {
        auto& s = svc::ReportBuilderService::instance();
        if (id <= 0 || id == s.library_id()) {
            auto& binder = svc::ReportDataBinder::instance();
            const int started = binder.refresh_all();
            if (started == 0 && binder.batch_pending() == 0) {
                done(true, 0, 0, QString());
                return;
            }
            // Wait for the batch (bounded); the blocks keep refreshing even if the answer goes out first.
            auto* scope = new QObject(qApp);
            auto answered = std::make_shared<bool>(false);
            QObject::connect(&binder, &svc::ReportDataBinder::batch_finished, scope,
                             [scope, done, answered](int ok, int failed) {
                                 if (*answered)
                                     return;
                                 *answered = true;
                                 scope->deleteLater();
                                 done(true, ok, failed, QString());
                             });
            QTimer::singleShot(kRtbRefreshWaitMs, scope, [scope, done, answered, started]() {
                if (*answered)
                    return;
                *answered = true;
                scope->deleteLater();
                done(true, 0, 0, QStringLiteral("still refreshing %1 block(s) after %2 s").arg(started).arg(kRtbRefreshWaitMs / 1000));
            });
            return;
        }
        svc::RegenerateRequest req;
        req.report_id = id;
        req.refresh_data = true;
        req.trigger = QStringLiteral("mcp");
        svc::ReportRegenerator::run(req, qApp, [done](const rep::schedule::RunRecord& r) {
            int failed = 0;
            for (const QString& w : r.warnings)
                if (w.contains(QStringLiteral("could not be refreshed")))
                    failed = 1; // the count is in the warning text
            done(r.ok, 0, failed, r.ok ? r.warnings.join(QLatin1Char(' ')) : r.error);
        });
    }

    void export_report(qint64 id, const QString& format, const QString& path,
                       std::function<void(bool, const QString&, const QStringList&, const QString&)> done) override {
        svc::RegenerateRequest req;
        req.report_id = id <= 0 ? svc::ReportBuilderService::instance().library_id() : id;
        req.refresh_data = false;
        req.formats = {format};
        req.explicit_path = path;
        req.trigger = QStringLiteral("mcp");
        svc::ReportRegenerator::run(req, qApp, [done](const rep::schedule::RunRecord& r) {
            done(r.ok && !r.files.isEmpty(), r.files.value(0), r.warnings, r.error.isEmpty() ? r.warnings.join(QLatin1Char(' ')) : r.error);
        });
    }

    void ask_approval(const ToolApprovalRequest& request, std::function<void(ToolApprovalVerdict)> done) override {
        ask_user_approval(request, std::move(done));
    }

    QString default_export_dir() const override {
        return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/Fincept Reports");
    }

    bool file_exists(const QString& path) const override { return QFileInfo::exists(path); }
};

} // namespace

ToolDef make_report_tool() {
    ToolDef t;
    t.name = QString::fromLatin1(kReportToolName);
    t.category = QString::fromLatin1(kCategoryUiControl);
    t.description = QStringLiteral(
        "Report Builder reports. action: list_reports | get_report (outline: info, variables, blocks with ids) | "
        "append_blocks (markdown: # headings, pipe tables, ``` code; optional title) | update_block_text (block_id, "
        "text) | refresh_report_data (live-data blocks) | export_report (format pdf|odt|md|html, optional absolute "
        "path). report_id from list_reports; 0 = the report open in the Report Builder. Write actions ask the user.");
    t.input_schema = ToolSchemaBuilder()
                         .string("action", "What to do")
                         .enums({"list_reports", "get_report", "append_blocks", "update_block_text",
                                 "refresh_report_data", "export_report"})
                         .required()
                         .integer("report_id", "Report id (0 = the open report)")
                         .string("query", "list_reports: title or text filter")
                         .integer("limit", "list_reports: at most this many (1-50)")
                         .integer("max_blocks", "get_report: blocks listed (default 120, max 300)")
                         .string("markdown", "append_blocks: content")
                         .string("title", "append_blocks: heading above the content")
                         .integer("block_id", "update_block_text: the block")
                         .string("text", "update_block_text: the new text (Markdown)")
                         .string("format", "export_report: pdf | odt | md | html")
                         .string("path", "export_report: absolute file path")
                         .build();
    t.default_timeout_ms = 170'000; // an approval (<= 50 s) plus a long export
    auto handler = std::make_shared<ReportToolHandler>(std::make_shared<RtbBackend>());
    t.async_handler = [handler](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
        const common::Resolver resolve = common::make_resolver(ctx, promise);
        if (!QCoreApplication::instance()) {
            resolve(ToolResult::fail(QStringLiteral("The terminal UI is not running.")));
            return;
        }
        const QString origin = ScopedCallOrigin::current(); // read on the calling thread
        // The caller's stable ids travel with the call (thread-local on the calling thread).
        const QString client_id = ScopedCallClient::client_id();
        const QString run_id = ScopedCallClient::run_id();
        common::on_gui_thread([handler, args, origin, client_id, run_id, resolve]() {
            ScopedCallClient caller(client_id, run_id);
            handler->call(args, origin, [resolve](ToolResult r) { resolve(std::move(r)); });
        });
    };
    return t;
}

} // namespace fincept::mcp::core
