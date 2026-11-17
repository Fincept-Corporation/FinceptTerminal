// ReportRegenerator.cpp — see ReportRegenerator.h.

#include "services/report_builder/ReportRegenerator.h"

#include "core/logging/Logger.h"
#include "core/report/ReportBinding.h"
#include "core/report/ReportVariables.h"
#include "screens/report_builder/ReportExportJob.h"
#include "services/report_builder/ReportBuilderService.h"
#include "services/report_builder/ReportDataBinder.h"
#include "services/report_builder/ReportLibrary.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace fincept::services {

namespace rep = ::fincept::report;
using ExportJob = ::fincept::screens::ReportExportJob;

namespace {

constexpr const char* kRgTag = "Research"; // the log tag of report_builder (logging policy)

QString rg_tr(const char* text, int n = -1) {
    return QCoreApplication::translate("fincept::services::ReportRegenerator", text, nullptr, n);
}

} // namespace

void ReportRegenerator::run(const RegenerateRequest& request, QObject* ctx, Done done) {
    auto* job = new ReportRegenerator(request, ctx, std::move(done));
    QMetaObject::invokeMethod(job, [job]() { job->start(); }, Qt::QueuedConnection);
}

ReportRegenerator::ReportRegenerator(const RegenerateRequest& request, QObject* ctx, Done done)
    : QObject(QCoreApplication::instance()), req_(request), ctx_(ctx), has_ctx_(ctx != nullptr), done_(std::move(done)) {
    record_.report_id = request.report_id;
    record_.trigger = request.trigger;
    record_.started = QDateTime::currentDateTimeUtc();
}

void ReportRegenerator::start() {
    auto& svc = ReportBuilderService::instance();
    open_report_ = req_.report_id <= 0 || req_.report_id == svc.library_id();
    if (open_report_) {
        use_open_report();
        return;
    }
    ReportLibrary::instance().load(req_.report_id, this,
                                   [this](bool ok, const rep::ReportDocument& doc, const QString& title,
                                          const QString& error) {
                                       if (!ok) {
                                           complete(false, error);
                                           return;
                                       }
                                       doc_ = doc;
                                       record_.title = title.isEmpty() ? rep::vars::display_title(doc.metadata) : title;
                                       if (req_.refresh_data)
                                           refresh_copy();
                                       else
                                           export_next();
                                   });
}

void ReportRegenerator::use_open_report() {
    auto& svc = ReportBuilderService::instance();
    auto& binder = ReportDataBinder::instance();
    record_.report_id = svc.library_id();
    record_.title = rep::vars::display_title(svc.metadata());
    if (!req_.refresh_data) {
        doc_ = svc.snapshot();
        export_next();
        return;
    }
    // The open report refreshes as if the user pressed Refresh Data; export when the batch ends.
    const int started = binder.refresh_all();
    if (started == 0 && binder.batch_pending() == 0) {
        doc_ = svc.snapshot();
        export_next();
        return;
    }
    auto* once = new QObject(this);
    connect(&binder, &ReportDataBinder::batch_finished, once, [this, once](int, int failed) {
        once->deleteLater();
        if (failed > 0)
            record_.warnings << rg_tr("%n data block(s) could not be refreshed and kept their last data.", failed);
        doc_ = ReportBuilderService::instance().snapshot();
        export_next();
    });
}

void ReportRegenerator::refresh_copy() {
    auto& binder = ReportDataBinder::instance();
    QVector<QPair<int, rep::Binding>> todo;
    for (const rep::ReportComponent& c : doc_.components) {
        const rep::Binding b = ReportDataBinder::effective_binding(c, doc_.metadata);
        if (b.is_refreshable())
            todo.append({c.id, b});
    }
    if (todo.isEmpty()) {
        export_next();
        return;
    }
    pending_fetches_ = static_cast<int>(todo.size());
    for (const auto& [id, b] : todo) {
        const int idx = doc_.index_of(id);
        const QString type = idx >= 0 ? doc_.components[idx].type : QString();
        const quint64 rid = binder.fetch(b, type, this, [this, id, b](const BindFetchResult& r) { apply_fetch(id, r, b); });
        if (rid == 0) {
            ++failed_fetches_;
            if (--pending_fetches_ == 0)
                save_copy_then_export();
        }
    }
}

void ReportRegenerator::apply_fetch(int component_id, const BindFetchResult& r, const rep::Binding& b) {
    const int idx = doc_.index_of(component_id);
    if (r.ok && idx >= 0) {
        rep::ReportComponent& c = doc_.components[idx];
        const ReportDataBinder::BindPatch p = ReportDataBinder::make_patch(c, b, r);
        for (auto it = p.config.cbegin(); it != p.config.cend(); ++it) {
            if (it.value().isEmpty())
                c.config.remove(it.key());
            else
                c.config.insert(it.key(), it.value());
        }
        if (p.has_content)
            c.content = p.content;
    } else if (!r.ok && !r.cancelled) {
        ++failed_fetches_;
    }
    if (--pending_fetches_ == 0)
        save_copy_then_export();
}

void ReportRegenerator::save_copy_then_export() {
    if (failed_fetches_ > 0)
        record_.warnings << rg_tr("%n data block(s) could not be refreshed and kept their last data.",
                                  failed_fetches_);
    const QString title = rep::vars::display_title(doc_.metadata);
    ReportLibrary::instance().save_checked(
        req_.report_id, title, doc_.to_json(false), doc_.revision, /*force=*/false, this,
        [this](const LibrarySaveOutcome& out) {
            if (out.conflict)
                record_.warnings << rg_tr("The refreshed numbers were not saved into the library: the report was "
                                          "changed on another computer meanwhile. The exported files have them.");
            else if (!out.ok)
                record_.warnings << rg_tr("The refreshed numbers could not be saved into the library: %1").arg(out.error);
            else
                doc_.revision = out.revision;
            export_next();
        });
}

void ReportRegenerator::export_next() {
    if (export_index_ == 0) {
        const QStringList unresolved = rep::vars::unresolved_in(doc_);
        if (!unresolved.isEmpty()) {
            QStringList refs;
            for (const QString& n : unresolved)
                refs << QStringLiteral("{{%1}}").arg(n);
            record_.warnings << rg_tr("Variables without a value were printed as typed: %1").arg(refs.join(QStringLiteral(", ")));
        }
        if (req_.explicit_path.isEmpty() && !req_.folder.isEmpty() && !QDir().mkpath(req_.folder)) {
            complete(false, rg_tr("The folder %1 cannot be created.").arg(req_.folder));
            return;
        }
    }
    if (export_index_ >= req_.formats.size()) {
        complete(!record_.files.isEmpty() || req_.formats.isEmpty(), record_.files.isEmpty() && !req_.formats.isEmpty()
                                                                         ? rg_tr("No file could be written.")
                                                                         : QString());
        return;
    }
    const QString fmt_name = req_.formats[export_index_++];
    ExportJob::Format format;
    if (!ExportJob::format_from_name(fmt_name, &format)) {
        record_.warnings << rg_tr("Unknown format “%1” skipped.").arg(fmt_name);
        export_next();
        return;
    }
    const QString path = !req_.explicit_path.isEmpty()
                             ? req_.explicit_path
                             : rep::schedule::output_path(req_.folder, rep::vars::display_title(doc_.metadata), QDate::currentDate(),
                                                          ExportJob::extension(format));
    ExportJob::run(format, path, doc_, this, [this, fmt_name](bool ok, bool cancelled, const QString& out, const QString& error) {
        if (ok)
            record_.files << out;
        else if (!cancelled)
            record_.warnings << rg_tr("%1 export failed: %2").arg(fmt_name.toUpper(), error);
        export_next();
    });
}

void ReportRegenerator::complete(bool ok, const QString& error) {
    if (finished_)
        return;
    finished_ = true;
    record_.ok = ok;
    record_.error = error;
    record_.finished = QDateTime::currentDateTimeUtc();
    LOG_INFO(kRgTag, logf(ok ? "Report regenerated" : "Report regeneration failed",
                          {{"report", record_.report_id},
                           {"trigger", record_.trigger},
                           {"files", record_.files.size()},
                           {"warnings", record_.warnings.size()},
                           {"duration_ms", record_.started.msecsTo(record_.finished)}}));
    if (done_ && (!has_ctx_ || ctx_))
        done_(record_);
    deleteLater();
}

} // namespace fincept::services
