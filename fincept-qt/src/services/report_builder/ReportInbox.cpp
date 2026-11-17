// ReportInbox.cpp — see ReportInbox.h.

#include "services/report_builder/ReportInbox.h"

#include "core/config/AppPaths.h"
#include "core/events/EventBus.h"
#include "core/report/ReportVariables.h"
#include "core/logging/Logger.h"
#include "services/report_builder/ReportBuilderService.h"
#include "services/report_builder/ReportBuilderTemplates.h"
#include "services/report_builder/ReportDataBinder.h"
#include "services/report_builder/ReportLibrary.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPointer>
#include <QSaveFile>
#include <QThread>
#include <QTimeZone>
#include <QtConcurrent/QtConcurrent>

#include <atomic>
#include <memory>

namespace fincept::services {

namespace rep = ::fincept::report;

namespace {

constexpr const char* kRiTag = "ReportInbox";

QString ri_tr(const char* text) {
    return QCoreApplication::translate("fincept::services::ReportInbox", text);
}

std::atomic<quint64> g_ri_next_id{1};

/// Folder of images other tabs sent into reports (content-addressed: the same chart
/// sent twice is one file).
QString ri_assets_dir() {
    return AppPaths::data() + QStringLiteral("/report_assets");
}

struct RiStoredImage {
    int block_index = -1;
    QString path; ///< empty = could not be stored / not an image
};

// Worker: verify each image decodes, write it once under its content hash.
QVector<RiStoredImage> ri_store_images(const QVector<rep::PendingImage>& images) {
    QVector<RiStoredImage> out;
    QDir().mkpath(ri_assets_dir());
    for (const rep::PendingImage& img : images) {
        RiStoredImage s;
        s.block_index = img.block_index;
        const QString ext = img.format == QLatin1String("jpg") || img.format == QLatin1String("jpeg")
                                ? QStringLiteral("jpg")
                                : QStringLiteral("png");
        if (QImage::fromData(img.bytes).isNull()) {
            out.append(s);
            continue;
        }
        const QString name = QString::fromLatin1(QCryptographicHash::hash(img.bytes, QCryptographicHash::Sha1).toHex()) +
                             QLatin1Char('.') + ext;
        const QString path = ri_assets_dir() + QLatin1Char('/') + name;
        if (!QFileInfo::exists(path)) {
            QSaveFile f(path);
            if (!f.open(QIODevice::WriteOnly) || f.write(img.bytes) != img.bytes.size() || !f.commit()) {
                out.append(s);
                continue;
            }
        }
        s.path = path;
        out.append(s);
    }
    return out;
}

void ri_finish(ReportInbox* inbox, const QPointer<QObject>& ctx, bool has_ctx, const ReportInbox::Done& done,
               const AppendResult& r) {
    if (r.ok)
        emit inbox->blocks_appended(r);
    else
        LOG_WARN(kRiTag, QStringLiteral("append failed: %1").arg(r.error));
    if (done && (!has_ctx || ctx))
        done(r);
}

} // namespace

ReportInbox& ReportInbox::instance() {
    static ReportInbox* s = [] {
        auto* inbox = new ReportInbox();
        // Every append is marshalled to this object; it must live on the GUI thread even
        // when a worker is the first to call in.
        if (auto* app = QCoreApplication::instance(); app && inbox->thread() != app->thread())
            inbox->moveToThread(app->thread());
        return inbox;
    }();
    return *s;
}

ReportInbox::ReportInbox() {
    qRegisterMetaType<fincept::services::AppendResult>("fincept::services::AppendResult");
}

quint64 ReportInbox::append_blocks(const ReportTarget& target, const rep::ReportPayload& payload,
                                   const AppendOptions& options, QObject* ctx, Done done) {
    rep::BuildOptions bo;
    bo.heading = options.heading;
    bo.provenance = options.provenance;
    bo.page_break_before = options.page_break_before;
    rep::ReportPayload p = payload;
    if (p.source.trimmed().isEmpty())
        p.source = options.source_label;
    const rep::BuiltBlocks built = rep::build_blocks(p, bo); // pure: fine on any thread
    AppendOptions o = options;
    if (o.source_label.isEmpty())
        o.source_label = p.source;
    ReportTarget t = target;
    if (t.title.isEmpty())
        t.title = p.title;

    const quint64 id = g_ri_next_id.fetch_add(1);
    const QPointer<QObject> guard(ctx);
    const bool has_ctx = ctx != nullptr;
    QMetaObject::invokeMethod(
        this,
        [this, id, t, built, o, guard, has_ctx, done = std::move(done)]() {
            run_append(id, t, built, o, guard.data(), has_ctx, done);
        },
        Qt::QueuedConnection);
    return id;
}

quint64 ReportInbox::append_components(const ReportTarget& target, const QVector<rep::ReportComponent>& blocks,
                                       const AppendOptions& options, QObject* ctx, Done done) {
    rep::BuiltBlocks built;
    for (rep::ReportComponent c : blocks) {
        c.id = 0;
        built.blocks.append(c);
    }
    const quint64 id = g_ri_next_id.fetch_add(1);
    const QPointer<QObject> guard(ctx);
    const bool has_ctx = ctx != nullptr;
    QMetaObject::invokeMethod(
        this,
        [this, id, target, built, options, guard, has_ctx, done = std::move(done)]() {
            run_append(id, target, built, options, guard.data(), has_ctx, done);
        },
        Qt::QueuedConnection);
    return id;
}

void ReportInbox::run_append(quint64 id, const ReportTarget& target, const rep::BuiltBlocks& built_in,
                             const AppendOptions& options, QObject* ctx_raw, bool has_ctx, Done done) {
    const QPointer<QObject> ctx(ctx_raw);
    AppendResult result;
    result.request_id = id;
    if (built_in.blocks.isEmpty()) {
        result.error = ri_tr("There is nothing to add: the result is empty.");
        ri_finish(this, ctx, has_ctx, done, result);
        return;
    }

    // Step 2 (GUI thread): put the blocks where they belong.
    auto place = [this, id, target, options, ctx, has_ctx, done](QVector<rep::ReportComponent> blocks) {
        AppendResult r;
        r.request_id = id;
        auto& svc = ReportBuilderService::instance();
        const QString label = options.source_label;
        const bool to_open_report =
            target.kind == ReportTarget::Kind::Current ||
            (target.kind == ReportTarget::Kind::Report && target.report_id > 0 && target.report_id == svc.library_id());

        if (to_open_report) {
            r.block_ids = svc.append_components(blocks, label, -1, /*mark_unseen=*/true);
            r.ok = !r.block_ids.isEmpty();
            r.in_open_report = true;
            r.report_id = svc.library_id();
            r.report_title = rep::vars::display_title(svc.metadata());
            if (!r.ok)
                r.error = ri_tr("The blocks could not be added.");
            // Bound blocks that arrived without numbers fill themselves.
            QVector<int> unfilled;
            for (int i = 0; i < blocks.size() && i < r.block_ids.size(); ++i)
                if (rep::is_bound(blocks[i]) && !rep::as_of_of(blocks[i]).isValid())
                    unfilled << r.block_ids[i];
            if (!unfilled.isEmpty())
                ReportDataBinder::instance().refresh_all(unfilled);
            if (r.ok && options.reveal)
                open_report(0, r.block_ids.value(0));
            ri_finish(this, ctx, has_ctx, done, r);
            return;
        }

        if (target.kind == ReportTarget::Kind::Report) {
            ReportLibrary::instance().append_to(
                target.report_id, blocks, label, this,
                [this, id, target, options, ctx, has_ctx, done](bool ok, const QVector<int>& ids, const QString& title,
                                                                const QString& error) {
                    AppendResult r2;
                    r2.request_id = id;
                    r2.ok = ok;
                    r2.report_id = target.report_id;
                    r2.report_title = title;
                    r2.block_ids = ids;
                    r2.error = error;
                    if (ok && options.reveal)
                        open_report(target.report_id, ids.value(0));
                    ri_finish(this, ctx, has_ctx, done, r2);
                });
            return;
        }

        // A new report (blank or from a template), stored in the library.
        auto create = [this, id, target, options, ctx, has_ctx, done, blocks](rep::ReportDocument doc) {
            const QString label = options.source_label;
            doc.metadata.date = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd"));
            ReportBuilderService::apply_identity_defaults(doc.metadata);
            if (!target.title.trimmed().isEmpty())
                doc.metadata.title = target.title.trimmed();
            QVector<int> ids;
            for (rep::ReportComponent c : blocks) {
                c.id = doc.allocate_id();
                doc.components.append(c);
                ids.append(c.id);
            }
            doc.unseen_ids = ids;
            if (!label.isEmpty())
                doc.unseen_sources = {label};
            ReportLibrary::instance().save(
                0, doc.metadata.title, doc.to_json(), this,
                [this, id, options, ctx, has_ctx, done, doc, ids](bool ok, qint64 new_id, const QString& error) {
                    AppendResult r2;
                    r2.request_id = id;
                    r2.ok = ok;
                    r2.report_id = new_id;
                    r2.report_title = doc.metadata.title;
                    r2.block_ids = ids;
                    r2.error = error;
                    if (ok && options.reveal)
                        open_report(new_id, ids.value(0));
                    ri_finish(this, ctx, has_ctx, done, r2);
                });
        };
        const rep::ReportTheme theme = svc.theme();
        if (target.kind == ReportTarget::Kind::NewFromTemplate &&
            target.template_name.startsWith(QLatin1String("user:"))) {
            ReportLibrary::instance().load_user_template(
                target.template_name, this,
                [this, id, ctx, has_ctx, done, create](bool ok, const rep::ReportDocument& doc, const QString&,
                                                       const QString& error) {
                    if (!ok) {
                        AppendResult r2;
                        r2.request_id = id;
                        r2.error = error;
                        ri_finish(this, ctx, has_ctx, done, r2);
                        return;
                    }
                    create(doc);
                });
            return;
        }
        rep::ReportDocument doc;
        doc.theme = theme;
        if (target.kind == ReportTarget::Kind::NewFromTemplate && report_templates::is_builtin(target.template_name))
            doc = report_templates::build(target.template_name, theme, {});
        create(doc);
    };

    if (built_in.images.isEmpty()) {
        place(built_in.blocks);
        return;
    }

    // Step 1 (worker): image bytes become files; then the blocks point at them.
    const rep::BuiltBlocks built = built_in;
    const QPointer<ReportInbox> self(this);
    (void)QtConcurrent::run([self, built, place]() {
        const QVector<RiStoredImage> stored = ri_store_images(built.images);
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [built, stored, place]() {
                QVector<rep::ReportComponent> blocks = built.blocks;
                QVector<int> drop;
                for (const RiStoredImage& s : stored) {
                    if (s.block_index < 0 || s.block_index >= blocks.size())
                        continue;
                    rep::ReportComponent& c = blocks[s.block_index];
                    c.config.remove(QStringLiteral("pending_image"));
                    if (s.path.isEmpty()) {
                        // Say so rather than leave an empty frame.
                        c = rep::blocks::caption(ri_tr("[An image sent to this report could not be read.]"));
                        continue;
                    }
                    c.config.insert(QStringLiteral("path"), s.path);
                    c.config.insert(QStringLiteral("asset"), QFileInfo(s.path).fileName());
                }
                place(blocks);
            },
            Qt::QueuedConnection);
    });
}

void ReportInbox::list_targets(int limit, QObject* ctx, TargetsDone done) {
    const QPointer<QObject> guard(ctx);
    const bool has_ctx = ctx != nullptr;
    QMetaObject::invokeMethod(
        this,
        [this, limit, guard, has_ctx, done = std::move(done)]() {
            auto& svc = ReportBuilderService::instance();
            QVector<ReportTargetInfo> out;
            ReportTargetInfo cur;
            cur.target = ReportTarget::current();
            cur.title = rep::vars::display_title(svc.metadata());
            cur.detail = QCoreApplication::translate("fincept::services::ReportInbox",
                                                     "open in Report Builder · %n block(s)", nullptr,
                                                     static_cast<int>(svc.components().size()));
            cur.is_current = true;
            out.append(cur);
            const qint64 current_id = svc.library_id();
            ReportLibrary::instance().list(
                QString(), qMax(1, limit) + 1, this,
                [guard, has_ctx, done, out, current_id, limit](bool ok, const QVector<LibraryEntry>& entries,
                                                                const QString&) mutable {
                    if (ok) {
                        for (const LibraryEntry& e : entries) {
                            if (e.id == current_id)
                                continue;
                            if (out.size() > limit)
                                break;
                            ReportTargetInfo t;
                            t.target = ReportTarget::report(e.id);
                            t.title = e.title;
                            const QDateTime when = QDateTime::fromString(e.updated_at, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
                            QDateTime utc = when;
                            utc.setTimeZone(QTimeZone::UTC);
                            t.detail = ri_tr("saved %1").arg(rep::as_of_display(utc));
                            out.append(t);
                        }
                    }
                    if (done && (!has_ctx || guard))
                        done(out);
                });
        },
        Qt::QueuedConnection);
}

void ReportInbox::open_report(qint64 report_id, int select_block_id) {
    auto& inbox = instance();
    QMetaObject::invokeMethod(
        &inbox,
        [report_id, select_block_id]() {
            ReportBuilderService::OpenRequest req;
            req.library_id = report_id;
            req.select_block_id = select_block_id;
            ReportBuilderService::instance().request_open(req);
            EventBus::instance().publish(QStringLiteral("nav.switch_screen"),
                                         {{QStringLiteral("screen_id"), QStringLiteral("report_builder")}});
        },
        Qt::QueuedConnection);
}

} // namespace fincept::services
