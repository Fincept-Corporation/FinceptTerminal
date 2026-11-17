// ReportLibrary.cpp — see ReportLibrary.h.

#include "services/report_builder/ReportLibrary.h"

#include "core/config/AppSettings.h"
#include "core/logging/Logger.h"
#include "core/report/ReportBinding.h"
#include "services/cloud/CloudSyncEngine.h"
#include "services/report_builder/ReportAssets.h"
#include "services/report_builder/ReportBuilderTemplates.h"
#include "storage/repositories/ReportRepository.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSettings>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QUuid>

#include <utility>

namespace fincept::services {

namespace {

constexpr const char* kRlTag = "ReportLibrary";
constexpr const char* kRlRecentKey = "report_builder/recent_library";
constexpr int kRlRecentMax = 12;
constexpr int kRlKindsCap = 60;

QSettings rl_settings() {
    return app_settings::open();
}

// Run `fn` on the GUI thread (the library object's thread) unless `ctx` died.
void rl_post(ReportLibrary* self, const QPointer<QObject>& ctx, bool has_ctx, std::function<void()> fn) {
    QMetaObject::invokeMethod(
        self,
        [ctx, has_ctx, fn = std::move(fn)]() {
            if (has_ctx && !ctx)
                return;
            fn();
        },
        Qt::QueuedConnection);
}

QString rl_error(const std::string& e) {
    return QString::fromStdString(e);
}

constexpr const char* kRlInstallKey = "report_builder/install_id";

// The report JSON as it goes into the row: revision stamped, pictures embedded.
QString rl_stamped_json(const QString& json, const report::ReportRevision& rev) {
    QJsonDocument d = QJsonDocument::fromJson(json.toUtf8());
    if (!d.isObject())
        return json;
    QJsonObject root = d.object();
    library_logic::stamp_revision(root, rev);
    report_assets::embed_for_sync(root);
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

report::ReportRevision rl_row_revision(const QString& content_json) {
    return report::ReportDocument::revision_of(QJsonDocument::fromJson(content_json.toUtf8()).object());
}

// Worker (or main thread at shutdown): the checked write itself.
LibrarySaveOutcome rl_checked_write(qint64 id, const QString& title, const QString& json,
                                    const report::ReportRevision& expected, bool force, const QString& by) {
    auto& repo = ReportRepository::instance();
    LibrarySaveOutcome out;
    report::ReportRevision stored;
    bool exists = false;
    if (id > 0) {
        auto row = repo.get(static_cast<int>(id));
        if (row.is_ok()) {
            exists = true;
            stored = rl_row_revision(row.value().content_json);
            if (library_logic::revision_conflicts(stored, expected, force)) {
                out.conflict = true;
                out.id = id;
                out.theirs_title = row.value().title;
                out.theirs_updated = row.value().updated_at;
                auto parsed = report::ReportDocument::from_json(row.value().content_json);
                if (parsed.is_ok()) {
                    out.theirs = parsed.value();
                    report_assets::materialize(out.theirs);
                }
                return out;
            }
        }
    }
    const report::ReportRevision next = library_logic::next_revision(expected, stored, by);
    const QString stamped = rl_stamped_json(json, next);
    if (exists) {
        auto r = repo.update(static_cast<int>(id), title, stamped);
        out.ok = r.is_ok();
        out.id = id;
        if (!out.ok)
            out.error = rl_error(r.error());
    } else {
        auto r = repo.create(title, stamped);
        out.ok = r.is_ok();
        if (out.ok)
            out.id = r.value();
        else
            out.error = rl_error(r.error());
    }
    if (out.ok)
        out.revision = next;
    return out;
}

} // namespace

ReportLibrary& ReportLibrary::instance() {
    static ReportLibrary* s = [] {
        auto* lib = new ReportLibrary();
        // Results are posted to this object; it must live on the GUI thread even if a
        // worker happened to be the first caller.
        if (auto* app = QCoreApplication::instance(); app && lib->thread() != app->thread())
            lib->moveToThread(app->thread());
        return lib;
    }();
    return *s;
}

ReportLibrary::ReportLibrary() {
    pool_ = new QThreadPool(this);
    // One thread: every library read and write is serialised, so a write is never
    // overtaken by a later read of the same row. Kept alive (its SQLite connection is
    // per thread and cheap to keep).
    pool_->setMaxThreadCount(1);
    pool_->setExpiryTimeout(-1);
    (void)installation_id(); // read the settings here, not first on a worker

    if (auto* app = QCoreApplication::instance()) {
        connect(app, &QCoreApplication::aboutToQuit, this, [this]() {
            // A delete the user did not undo still happens.
            const auto ids = pending_removals_.keys();
            for (qint64 id : ids) {
                if (QTimer* t = pending_removals_.take(id))
                    t->deleteLater();
                ReportRepository::instance().remove(static_cast<int>(id));
            }
            pool_->waitForDone(3000);
        });
    }
}

ReportLibrary::~ReportLibrary() = default;

QString ReportLibrary::installation_id() {
    // Read once on the GUI thread (instance() is created there at startup); workers get
    // the cached copy.
    static const QString id = [] {
        QSettings s = rl_settings();
        QString v = s.value(kRlInstallKey).toString();
        if (v.size() != 8) {
            v = QUuid::createUuid().toString(QUuid::Id128).left(8);
            s.setValue(kRlInstallKey, v);
        }
        return v;
    }();
    return id;
}

void ReportLibrary::save_checked(qint64 id, const QString& title, const QString& json,
                                 const report::ReportRevision& expected, bool force, QObject* ctx,
                                 CheckedSaveDone done) {
    const QPointer<QObject> guard(ctx);
    const bool has_ctx = ctx != nullptr;
    const QString by = installation_id();
    on_worker([this, id, title, json, expected, force, by, guard, has_ctx, done = std::move(done)]() {
        const LibrarySaveOutcome out = rl_checked_write(id, title, json, expected, force, by);
        if (out.conflict)
            LOG_INFO(kRlTag, logf("Report changed elsewhere; save held for the user",
                                  {{"report", id}, {"base", expected.text()}, {"stored_by", out.theirs.revision.by}}));
        rl_post(this, guard, has_ctx, [this, done, out]() {
            if (out.ok)
                emit library_changed();
            if (done)
                done(out);
        });
    });
}

LibrarySaveOutcome ReportLibrary::save_checked_blocking(qint64 id, const QString& title, const QString& json,
                                                        const report::ReportRevision& expected, bool force) {
    return rl_checked_write(id, title, json, expected, force, installation_id());
}

void ReportLibrary::run_on_db_worker(std::function<QVariant()> work, QObject* ctx,
                                     std::function<void(const QVariant&)> done) {
    const QPointer<QObject> guard(ctx);
    const bool has_ctx = ctx != nullptr;
    on_worker([this, work = std::move(work), guard, has_ctx, done = std::move(done)]() {
        const QVariant v = work ? work() : QVariant();
        rl_post(this, guard, has_ctx, [done, v]() {
            if (done)
                done(v);
        });
    });
}

template <typename Fn>
void ReportLibrary::on_worker(Fn&& fn) {
    pool_->start(std::function<void()>(std::forward<Fn>(fn)));
}

void ReportLibrary::wait_for_idle(int ms) {
    pool_->waitForDone(ms);
}

// Hooked on first use (a library view asked for rows), not at construction: the report
// service creates the library at startup, before the cloud engine should exist.
void ReportLibrary::ensure_cloud_hook() {
    if (cloud_hooked_)
        return;
    cloud_hooked_ = true;
    connect(&cloud::CloudSyncEngine::instance(), &cloud::CloudSyncEngine::cloud_data_changed, this,
            [this](const QString& entity) {
                if (entity == QLatin1String("report"))
                    emit library_changed();
            });
}

LibraryEntry ReportLibrary::summarise(qint64 id, const QString& title, const report::ReportDocument& doc) {
    return library_logic::summarise(id, title, doc);
}

void ReportLibrary::list(const QString& query, int limit, QObject* ctx, ListDone done) {
    ensure_cloud_hook();
    const QPointer<QObject> guard(ctx);
    const bool has_ctx = ctx != nullptr;
    QSet<qint64> hidden;
    for (auto it = pending_removals_.cbegin(); it != pending_removals_.cend(); ++it)
        hidden.insert(it.key());
    on_worker([this, query, limit, guard, has_ctx, done = std::move(done), hidden]() {
        auto r = ReportRepository::instance().search(query, limit);
        QVector<LibraryEntry> out;
        QString err;
        const bool ok = r.is_ok();
        if (ok) {
            for (const Report& row : r.value()) {
                if (hidden.contains(row.id))
                    continue;
                const auto doc = report::ReportDocument::from_json(row.content_json);
                LibraryEntry e = doc.is_ok() ? summarise(row.id, row.title, doc.value()) : LibraryEntry{};
                e.id = row.id;
                e.title = row.title.isEmpty() ? QStringLiteral("Untitled Report") : row.title;
                e.created_at = row.created_at;
                e.updated_at = row.updated_at;
                e.bytes = static_cast<int>(row.content_json.size());
                out.append(e);
            }
        } else {
            err = rl_error(r.error());
        }
        rl_post(this, guard, has_ctx, [done, ok, out, err]() {
            if (done)
                done(ok, out, err);
        });
    });
}

void ReportLibrary::load(qint64 id, QObject* ctx, LoadDone done) {
    const QPointer<QObject> guard(ctx);
    const bool has_ctx = ctx != nullptr;
    on_worker([this, id, guard, has_ctx, done = std::move(done)]() {
        auto r = ReportRepository::instance().get(static_cast<int>(id));
        report::ReportDocument doc;
        QString title, err;
        bool ok = false;
        if (r.is_ok()) {
            auto parsed = report::ReportDocument::from_json(r.value().content_json);
            if (parsed.is_ok()) {
                doc = parsed.value();
                title = r.value().title;
                // Pictures synced from another computer go back into the assets folder.
                report_assets::materialize(doc);
                ok = true;
            } else {
                err = QCoreApplication::translate("ReportLibrary", "The saved report could not be read: %1")
                          .arg(rl_error(parsed.error()));
            }
        } else {
            err = QCoreApplication::translate("ReportLibrary", "The report is no longer in the library.");
        }
        rl_post(this, guard, has_ctx, [done, ok, doc, title, err]() {
            if (done)
                done(ok, doc, title, err);
        });
    });
}

void ReportLibrary::save(qint64 id, const QString& title, const QString& json, QObject* ctx, SaveDone done) {
    const QPointer<QObject> guard(ctx);
    const bool has_ctx = ctx != nullptr;
    const QString by = installation_id();
    on_worker([this, id, title, json, by, guard, has_ctx, done = std::move(done)]() {
        auto& repo = ReportRepository::instance();
        qint64 out_id = id;
        QString err;
        bool ok = false;
        const report::ReportRevision own = rl_row_revision(json);
        if (id > 0) {
            // update() succeeds on a missing row too; check it is still there (a cloud pull
            // or another window may have deleted it) and recreate it when it is not.
            auto row = repo.get(static_cast<int>(id));
            if (row.is_ok()) {
                const auto next = library_logic::next_revision(own, rl_row_revision(row.value().content_json), by);
                auto r = repo.update(static_cast<int>(id), title, rl_stamped_json(json, next));
                ok = r.is_ok();
                if (!ok)
                    err = rl_error(r.error());
            } else {
                out_id = 0;
            }
        }
        if (out_id <= 0) {
            auto r = repo.create(title, rl_stamped_json(json, library_logic::next_revision({}, {}, by)));
            ok = r.is_ok();
            if (ok)
                out_id = r.value();
            else
                err = rl_error(r.error());
        }
        rl_post(this, guard, has_ctx, [this, done, ok, out_id, err]() {
            if (ok)
                emit library_changed();
            if (done)
                done(ok, out_id, err);
        });
    });
}

qint64 ReportLibrary::save_blocking(qint64 id, const QString& title, const QString& json) {
    const LibrarySaveOutcome out = rl_checked_write(id, title, json, rl_row_revision(json), /*force=*/true,
                                                    installation_id());
    return out.ok ? out.id : 0;
}

void ReportLibrary::rename(qint64 id, const QString& title, QObject* ctx, Done done) {
    const QPointer<QObject> guard(ctx);
    const bool has_ctx = ctx != nullptr;
    on_worker([this, id, title, guard, has_ctx, done = std::move(done)]() {
        auto& repo = ReportRepository::instance();
        auto row = repo.get(static_cast<int>(id));
        QString err;
        bool ok = false;
        if (row.is_ok()) {
            QString json = row.value().content_json;
            auto doc = report::ReportDocument::from_json(json);
            if (doc.is_ok()) {
                auto d = doc.value();
                library_logic::rename(d, title);
                json = d.to_json();
            }
            auto r = repo.update(static_cast<int>(id), title, json);
            ok = r.is_ok();
            if (!ok)
                err = rl_error(r.error());
        } else {
            err = rl_error(row.error());
        }
        rl_post(this, guard, has_ctx, [this, done, ok, err]() {
            if (ok)
                emit library_changed();
            if (done)
                done(ok, err);
        });
    });
}

void ReportLibrary::duplicate(qint64 id, QObject* ctx, SaveDone done) {
    const QPointer<QObject> guard(ctx);
    const bool has_ctx = ctx != nullptr;
    on_worker([this, id, guard, has_ctx, done = std::move(done)]() {
        auto& repo = ReportRepository::instance();
        auto row = repo.get(static_cast<int>(id));
        qint64 new_id = 0;
        QString err;
        bool ok = false;
        if (row.is_ok()) {
            const QString title =
                QCoreApplication::translate("ReportLibrary", "%1 (copy)").arg(row.value().title);
            QString json = row.value().content_json;
            auto doc = report::ReportDocument::from_json(json);
            if (doc.is_ok()) {
                report::ReportDocument d = library_logic::duplicate(doc.value(), title);
                d.revision = library_logic::next_revision({}, {}, installation_id()); // a new report's history
                json = d.to_json();
            }
            auto r = repo.create(title, json);
            ok = r.is_ok();
            if (ok)
                new_id = r.value();
            else
                err = rl_error(r.error());
        } else {
            err = rl_error(row.error());
        }
        rl_post(this, guard, has_ctx, [this, done, ok, new_id, err]() {
            if (ok)
                emit library_changed();
            if (done)
                done(ok, new_id, err);
        });
    });
}

void ReportLibrary::append_to(qint64 id, const QVector<report::ReportComponent>& blocks, const QString& source_label,
                              QObject* ctx, AppendDone done) {
    const QPointer<QObject> guard(ctx);
    const bool has_ctx = ctx != nullptr;
    on_worker([this, id, blocks, source_label, guard, has_ctx, done = std::move(done)]() {
        auto& repo = ReportRepository::instance();
        auto row = repo.get(static_cast<int>(id));
        QVector<int> ids;
        QString title, err;
        bool ok = false;
        if (!row.is_ok()) {
            err = QCoreApplication::translate("ReportLibrary", "The report is no longer in the library.");
        } else {
            auto parsed = report::ReportDocument::from_json(row.value().content_json);
            if (parsed.is_err()) {
                err = QCoreApplication::translate("ReportLibrary", "The saved report could not be read: %1")
                          .arg(rl_error(parsed.error()));
            } else {
                auto d = parsed.value();
                ids = library_logic::append_blocks(d, blocks, source_label);
                title = row.value().title;
                // A changed row is a new revision: a copy open on another computer notices.
                d.revision = library_logic::next_revision(d.revision, d.revision, installation_id());
                QJsonObject root = d.to_json_obj();
                report_assets::embed_for_sync(root);
                auto r = repo.update(static_cast<int>(id), title,
                                     QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented)));
                ok = r.is_ok();
                if (!ok)
                    err = rl_error(r.error());
            }
        }
        rl_post(this, guard, has_ctx, [this, done, ok, ids, title, err]() {
            if (ok)
                emit library_changed();
            if (done)
                done(ok, ids, title, err);
        });
    });
}

void ReportLibrary::remove_later(qint64 id, int grace_ms) {
    if (id <= 0 || pending_removals_.contains(id))
        return;
    auto* t = new QTimer(this);
    t->setSingleShot(true);
    t->setInterval(qMax(0, grace_ms));
    connect(t, &QTimer::timeout, this, [this, id]() { commit_removal(id); });
    pending_removals_.insert(id, t);
    t->start();
    emit library_changed();
}

bool ReportLibrary::undo_remove(qint64 id) {
    QTimer* t = pending_removals_.take(id);
    if (!t)
        return false;
    t->stop();
    t->deleteLater();
    emit library_changed();
    return true;
}

void ReportLibrary::commit_removal(qint64 id) {
    QTimer* t = pending_removals_.take(id);
    if (!t)
        return;
    t->deleteLater();
    on_worker([this, id]() {
        auto r = ReportRepository::instance().remove(static_cast<int>(id));
        if (r.is_err())
            LOG_WARN(kRlTag, QStringLiteral("Could not delete report %1: %2").arg(id).arg(rl_error(r.error())));
        rl_post(this, QPointer<QObject>(), false, [this, id]() {
            emit report_removed(id);
            emit library_changed();
        });
    });
    // Forget it in "recent".
    QSettings s = rl_settings();
    QStringList ids = s.value(kRlRecentKey).toStringList();
    ids.removeAll(QString::number(id));
    s.setValue(kRlRecentKey, ids);
}

// ── Templates ────────────────────────────────────────────────────────────────

QVector<TemplateEntry> ReportLibrary::builtin_templates() {
    QVector<TemplateEntry> out;
    const report::ReportTheme paper = report::themes::light_professional();
    for (const auto& t : report_templates::catalogue()) {
        TemplateEntry e;
        e.id = t.name;
        e.name = t.name;
        e.description = t.description;
        e.category = t.category;
        e.needs = t.needs;
        e.live = t.live;
        const auto doc = report_templates::build(t.name, paper, {});
        e.theme = doc.theme.name;
        for (const auto& c : doc.components) {
            if (e.kinds.size() >= kRlKindsCap)
                break;
            e.kinds << c.type;
        }
        out.append(e);
    }
    return out;
}

void ReportLibrary::list_user_templates(QObject* ctx, TemplatesDone done) {
    const QPointer<QObject> guard(ctx);
    const bool has_ctx = ctx != nullptr;
    on_worker([this, guard, has_ctx, done = std::move(done)]() {
        auto r = ReportRepository::instance().list_templates();
        QVector<TemplateEntry> out;
        QString err;
        const bool ok = r.is_ok();
        if (ok) {
            for (const ReportTemplate& t : r.value()) {
                TemplateEntry e;
                e.id = QStringLiteral("user:") + t.id;
                e.name = t.name;
                e.description = t.description;
                e.category = QCoreApplication::translate("ReportLibrary", "My Templates");
                e.user = true;
                auto doc = report::ReportDocument::from_json(t.template_data);
                if (doc.is_ok()) {
                    e.theme = doc.value().theme.name;
                    for (const auto& c : doc.value().components) {
                        if (e.kinds.size() >= kRlKindsCap)
                            break;
                        e.kinds << c.type;
                        e.live = e.live || report::is_bound(c);
                    }
                }
                out.append(e);
            }
        } else {
            err = rl_error(r.error());
        }
        rl_post(this, guard, has_ctx, [done, ok, out, err]() {
            if (done)
                done(ok, out, err);
        });
    });
}

void ReportLibrary::save_user_template(const QString& name, const QString& description, const QString& json,
                                       QObject* ctx, Done done) {
    const QPointer<QObject> guard(ctx);
    const bool has_ctx = ctx != nullptr;
    on_worker([this, name, description, json, guard, has_ctx, done = std::move(done)]() {
        // A template is a starting point: no "new blocks" notice, no stale as-of state.
        QString data = json;
        auto doc = report::ReportDocument::from_json(json);
        if (doc.is_ok()) {
            auto d = doc.value();
            d.unseen_ids.clear();
            d.unseen_sources.clear();
            data = d.to_json();
        }
        ReportTemplate t;
        t.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        t.name = name;
        t.description = description;
        t.template_data = data;
        auto r = ReportRepository::instance().save_template(t);
        const bool ok = r.is_ok();
        const QString err = ok ? QString() : rl_error(r.error());
        rl_post(this, guard, has_ctx, [this, done, ok, err]() {
            if (ok)
                emit library_changed();
            if (done)
                done(ok, err);
        });
    });
}

void ReportLibrary::rename_user_template(const QString& id, const QString& name, QObject* ctx, Done done) {
    const QPointer<QObject> guard(ctx);
    const bool has_ctx = ctx != nullptr;
    const QString row_id = id.startsWith(QLatin1String("user:")) ? id.mid(5) : id;
    on_worker([this, row_id, name, guard, has_ctx, done = std::move(done)]() {
        auto& repo = ReportRepository::instance();
        auto t = repo.get_template(row_id);
        bool ok = false;
        QString err;
        if (t.is_ok()) {
            ReportTemplate nt = t.value();
            nt.name = name;
            auto r = repo.save_template(nt);
            ok = r.is_ok();
            if (!ok)
                err = rl_error(r.error());
        } else {
            err = rl_error(t.error());
        }
        rl_post(this, guard, has_ctx, [this, done, ok, err]() {
            if (ok)
                emit library_changed();
            if (done)
                done(ok, err);
        });
    });
}

void ReportLibrary::remove_user_template(const QString& id, QObject* ctx, Done done) {
    const QPointer<QObject> guard(ctx);
    const bool has_ctx = ctx != nullptr;
    const QString row_id = id.startsWith(QLatin1String("user:")) ? id.mid(5) : id;
    on_worker([this, row_id, guard, has_ctx, done = std::move(done)]() {
        auto r = ReportRepository::instance().remove_template(row_id);
        const bool ok = r.is_ok();
        const QString err = ok ? QString() : rl_error(r.error());
        rl_post(this, guard, has_ctx, [this, done, ok, err]() {
            if (ok)
                emit library_changed();
            if (done)
                done(ok, err);
        });
    });
}

void ReportLibrary::load_user_template(const QString& id, QObject* ctx, LoadDone done) {
    const QPointer<QObject> guard(ctx);
    const bool has_ctx = ctx != nullptr;
    const QString row_id = id.startsWith(QLatin1String("user:")) ? id.mid(5) : id;
    on_worker([this, row_id, guard, has_ctx, done = std::move(done)]() {
        auto t = ReportRepository::instance().get_template(row_id);
        report::ReportDocument doc;
        QString title, err;
        bool ok = false;
        if (t.is_ok()) {
            auto parsed = report::ReportDocument::from_json(t.value().template_data);
            ok = parsed.is_ok();
            if (ok) {
                doc = parsed.value();
                title = t.value().name;
            } else {
                err = rl_error(parsed.error());
            }
        } else {
            err = QCoreApplication::translate("ReportLibrary", "The template no longer exists.");
        }
        rl_post(this, guard, has_ctx, [done, ok, doc, title, err]() {
            if (done)
                done(ok, doc, title, err);
        });
    });
}

// ── Recent ──────────────────────────────────────────────────────────────────

QVector<qint64> ReportLibrary::recent_ids() const {
    QVector<qint64> out;
    for (const QString& s : rl_settings().value(kRlRecentKey).toStringList()) {
        const qint64 id = s.toLongLong();
        if (id > 0 && !pending_removals_.contains(id))
            out.append(id);
    }
    return out;
}

void ReportLibrary::note_opened(qint64 id) {
    if (id <= 0)
        return;
    QSettings s = rl_settings();
    QStringList ids = s.value(kRlRecentKey).toStringList();
    ids.removeAll(QString::number(id));
    ids.prepend(QString::number(id));
    while (ids.size() > kRlRecentMax)
        ids.removeLast();
    s.setValue(kRlRecentKey, ids);
}

} // namespace fincept::services
