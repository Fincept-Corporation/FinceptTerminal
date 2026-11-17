// ArtefactService.cpp — run artefacts: the index, atomic file writes on a worker
// thread, extraction from runs, retention, export, the Notes hand-off (WP5;
// "Add to report" builds its payload from the loaded content: ReportHandoff.h).
// See the header for the rules.

#include "services/agent_tasks/ArtefactService.h"

#include "core/config/AppPaths.h"
#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "services/agent_tasks/TaskRunService.h"
#include "services/agent_tasks/TaskRunStore.h"
#include "services/file_manager/FileManagerService.h"
#include "storage/repositories/NoteBoardRepository.h"
#include "storage/repositories/NotesRepository.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QThreadPool>
#include <QTimer>
#include <QUuid>

#ifdef FINCEPT_HAS_QXLSX
#    include <xlsxdocument.h>
#endif

#include <algorithm>
#include <memory>
#include <utility>

namespace fincept::agent_tasks {

namespace al = artefact_logic;

namespace artefact_service_detail {

constexpr const char* kArtefactTag = "Artefacts";
constexpr int kAsvQuotaCheckDelayMs = 5000;
/// A file this young may belong to a save still in flight: the sweep leaves it.
constexpr qint64 kAsvSweepMinAgeMs = 10LL * 60 * 1000;

QString asv_tr(const char* text) {
    return QCoreApplication::translate("fincept::agent_tasks::ArtefactService", text);
}

qint64 asv_now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}

/// Deliver `fn` on the GUI thread, unless `ctx` (when given) died meanwhile.
void asv_post(QObject* ctx, std::function<void()> fn) {
    QPointer<QObject> guard(ctx);
    const bool has_ctx = ctx != nullptr;
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        [guard, has_ctx, fn = std::move(fn)]() {
            if (has_ctx && !guard)
                return;
            fn();
        },
        Qt::QueuedConnection);
}

/// Write `bytes` to `path` atomically: readers see the old file or the whole new
/// one, never half of it (QSaveFile writes a temporary and renames it).
bool asv_write_atomic(const QString& path, const QByteArray& bytes, QString* error) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error)
            *error = f.errorString();
        return false;
    }
    if (f.write(bytes) != bytes.size()) {
        if (error)
            *error = f.errorString();
        f.cancelWriting();
        return false;
    }
    if (!f.commit()) {
        if (error)
            *error = f.errorString();
        return false;
    }
    return true;
}

QByteArray asv_read(const QString& path, qint64 max_bytes, QString* error) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = asv_tr("The file could not be read: %1").arg(f.errorString());
        return {};
    }
    if (f.size() > max_bytes) {
        if (error)
            *error = asv_tr("The file is larger than %1 MB.").arg(max_bytes / (1024 * 1024));
        return {};
    }
    return f.readAll();
}

bool asv_is_tabular_kind(const QString& kind) {
    return kind == QLatin1String(kArtefactTable) || kind == QLatin1String(kArtefactSeries);
}

/// A plain number for a spreadsheet cell, when the text is one.
bool asv_number(const QString& text, double* out) {
    QString t = text.trimmed();
    if (t.isEmpty() || !al::looks_numeric(t))
        return false;
    bool percent = false;
    t.remove(QLatin1Char(','));
    for (const QChar c : {QChar(u'$'), QChar(u'€'), QChar(u'£'), QChar(u'¥'), QChar(u'₹')})
        t.remove(c);
    if (t.endsWith(QLatin1Char('%'))) {
        percent = true;
        t.chop(1);
    }
    bool ok = false;
    const double v = QLocale::c().toDouble(t.trimmed(), &ok);
    if (!ok)
        return false;
    *out = percent ? v / 100.0 : v;
    return true;
}

} // namespace artefact_service_detail

using namespace artefact_service_detail;

// ── Impl ─────────────────────────────────────────────────────────────────────

struct ArtefactService::Impl {
    ArtefactService* q = nullptr;
    QThreadPool* worker = nullptr;
    bool initialized = false;
    bool loaded = false;
    QHash<QString, RunArtefact> by_id;
    QHash<QString, QStringList> by_run; ///< run id -> artefact ids, oldest first
    QSet<QString> pending_keys;         ///< "<run>|<source key>" being written
    QSet<QString> deferred_extract;     ///< runs to extract once the index is loaded ("" = whole run, "<run>#n" = step)
    QTimer* quota_timer = nullptr;

    void insert(const RunArtefact& a) {
        const bool known = by_id.contains(a.id);
        by_id.insert(a.id, a);
        if (!known)
            by_run[a.run_id].append(a.id);
    }

    void remove(const QString& id) {
        const auto it = by_id.constFind(id);
        if (it == by_id.cend())
            return;
        const QString run = it->run_id;
        by_id.erase(it);
        auto r = by_run.find(run);
        if (r != by_run.end()) {
            r->removeAll(id);
            if (r->isEmpty())
                by_run.erase(r);
        }
    }

    void submit(std::function<void()> job) { worker->start(std::move(job)); }

    void persist_row(const RunArtefact& a) {
        submit([a]() {
            auto r = TaskRunStore::instance().upsert_artefact(a);
            if (r.is_err())
                LOG_WARN(kArtefactTag, QStringLiteral("upsert failed: %1").arg(QString::fromStdString(r.error())));
        });
    }

    /// Remove rows and files on the worker (the index is updated by the caller).
    void erase_on_disk(const QVector<RunArtefact>& gone, const QStringList& folders_to_tidy = {}) {
        if (gone.isEmpty() && folders_to_tidy.isEmpty())
            return;
        submit([gone, folders_to_tidy]() {
            QStringList ids;
            ids.reserve(gone.size());
            for (const RunArtefact& a : gone) {
                ids << a.id;
                const QString path = file_path(a);
                if (!path.isEmpty())
                    QFile::remove(path);
            }
            auto r = TaskRunStore::instance().delete_artefacts(ids);
            if (r.is_err())
                LOG_WARN(kArtefactTag, QStringLiteral("delete failed: %1").arg(QString::fromStdString(r.error())));
            for (const QString& folder : folders_to_tidy)
                QDir().rmdir(folder); // only when empty: a pinned artefact keeps it
        });
    }

    /// The one save path: write the file atomically, then the row, then index it
    /// and tell TaskRunService. `bytes` empty = an inline-only row.
    void write_and_record(RunArtefact a, const QByteArray& bytes, QObject* ctx, DoneCallback cb);
    void schedule_quota_check();
    void run_retention(bool sweep_files);
    void extract_now(const QString& run_id, int only_step);
    QSet<QString> keys_of(const QString& run_id) const;
};

QSet<QString> ArtefactService::Impl::keys_of(const QString& run_id) const {
    QSet<QString> keys;
    for (const QString& id : by_run.value(run_id)) {
        const QString k = al::artefact_key(by_id.value(id));
        if (!k.isEmpty())
            keys.insert(k);
    }
    for (const QString& p : pending_keys)
        if (p.startsWith(run_id + QLatin1Char('|')))
            keys.insert(p.mid(run_id.size() + 1));
    return keys;
}

void ArtefactService::Impl::write_and_record(RunArtefact a, const QByteArray& bytes, QObject* ctx, DoneCallback cb) {
    const auto fail = [ctx, cb](const QString& message) {
        if (cb)
            asv_post(ctx, [cb, message]() { cb(false, {}, message); });
    };
    const TaskRun* run = TaskRunService::instance().find_run(a.run_id);
    if (!run) {
        fail(asv_tr("This run no longer exists."));
        return;
    }
    if (bytes.size() > al::kArtefactMaxBytes) {
        fail(asv_tr("This artefact is larger than %1 MB and was not kept.").arg(al::kArtefactMaxBytes / (1024 * 1024)));
        return;
    }
    if (a.id.isEmpty())
        a.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (a.created_at_ms <= 0)
        a.created_at_ms = asv_now_ms();
    if (a.mime.isEmpty())
        a.mime = al::mime_for_kind(a.kind);
    a.run_title = run->title;
    a.title = a.title.trimmed().isEmpty() ? asv_tr("Untitled artefact") : a.title.trimmed().left(200);
    if (!bytes.isEmpty() && a.stored_name.isEmpty())
        a.stored_name = al::stored_name_for(a.id, a.title, al::extension_for_kind(a.kind));
    if (!bytes.isEmpty())
        a.bytes = bytes.size();
    const QString key = al::artefact_key(a);
    const QString pending = key.isEmpty() ? QString() : a.run_id + QLatin1Char('|') + key;
    if (!pending.isEmpty())
        pending_keys.insert(pending);

    QPointer<ArtefactService> self(q);
    submit([self, a, bytes, ctx, cb, pending]() {
        QString err;
        bool ok = true;
        if (!bytes.isEmpty())
            ok = asv_write_atomic(file_path(a), bytes, &err);
        if (ok) {
            auto r = TaskRunStore::instance().upsert_artefact(a);
            if (r.is_err()) {
                ok = false;
                err = QString::fromStdString(r.error());
                if (!bytes.isEmpty())
                    QFile::remove(file_path(a)); // a file without its row would only be swept later
            }
        }
        if (!ok)
            LOG_WARN(kArtefactTag, QStringLiteral("Artefact %1 of run %2 not saved: %3").arg(a.id.left(8), a.run_id.left(8), err));
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, a, ok, err, ctx, cb, pending]() {
                if (!self)
                    return;
                Impl* d = self->d_.get();
                if (!pending.isEmpty())
                    d->pending_keys.remove(pending);
                QPointer<QObject> guard(ctx);
                const auto reply = [&](bool success, const QString& id, const QString& message) {
                    if (cb && (ctx == nullptr || guard))
                        cb(success, id, message);
                };
                if (!ok) {
                    reply(false, {}, asv_tr("The artefact could not be saved: %1").arg(err));
                    return;
                }
                if (!TaskRunService::instance().find_run(a.run_id)) {
                    // Its run was deleted while the file was written: it goes too.
                    d->erase_on_disk({a});
                    reply(false, {}, asv_tr("The run was deleted meanwhile."));
                    return;
                }
                d->insert(a);
                TaskRunService::instance().note_artefact(a);
                emit self->artefact_saved(a.run_id, a.id);
                emit self->artefacts_changed(a.run_id);
                d->schedule_quota_check();
                reply(true, a.id, {});
            },
            Qt::QueuedConnection);
    });
}

void ArtefactService::Impl::schedule_quota_check() {
    if (!quota_timer->isActive())
        quota_timer->start();
}

void ArtefactService::Impl::run_retention(bool sweep_files) {
    auto& runs = TaskRunService::instance();
    al::RetentionInput in;
    in.artefacts.reserve(by_id.size());
    for (auto it = by_id.cbegin(); it != by_id.cend(); ++it)
        in.artefacts.append(it.value());
    for (const QString& id : runs.run_ids()) {
        in.live_run_ids.insert(id);
        if (const TaskRun* r = runs.find_run(id); r && !is_terminal(r->status))
            in.active_run_ids.insert(id);
    }
    const al::RetentionPlan plan = al::plan_retention(in);
    QVector<RunArtefact> gone;
    QSet<QString> runs_touched;
    for (const QString& id : plan.delete_ids) {
        const auto it = by_id.constFind(id);
        if (it == by_id.cend())
            continue;
        gone.append(it.value());
        runs_touched.insert(it->run_id);
        remove(id);
    }
    QStringList folders;
    for (const QString& run : std::as_const(runs_touched))
        folders << run_folder(run);
    erase_on_disk(gone, folders);
    if (!gone.isEmpty()) {
        LOG_INFO(kArtefactTag, QStringLiteral("Retention removed %1 artefact(s) (%2 → %3 bytes)")
                                   .arg(gone.size())
                                   .arg(plan.bytes_before)
                                   .arg(plan.bytes_after));
        for (const QString& run : std::as_const(runs_touched))
            emit q->artefacts_changed(run);
    }
    if (!sweep_files)
        return;
    // Files no row refers to: a crash between a file and its row, or a row
    // deleted while its file could not be removed. Folders of runs without
    // artefacts go too.
    QHash<QString, QSet<QString>> referenced;
    for (auto it = by_id.cbegin(); it != by_id.cend(); ++it)
        if (!it->stored_name.isEmpty())
            referenced[it->run_id].insert(it->stored_name);
    const QString root = root_folder();
    submit([root, referenced]() {
        const QDir dir(root);
        if (!dir.exists())
            return;
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        int removed = 0;
        for (const QFileInfo& run_dir : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QDir rd(run_dir.absoluteFilePath());
            QStringList names;
            for (const QFileInfo& f : rd.entryInfoList(QDir::Files | QDir::Hidden | QDir::System)) {
                if (now - f.lastModified().toMSecsSinceEpoch() >= kAsvSweepMinAgeMs)
                    names << f.fileName();
            }
            for (const QString& n : al::unreferenced_files(names, referenced.value(run_dir.fileName()))) {
                if (QFile::remove(rd.absoluteFilePath(n)))
                    ++removed;
            }
            QDir().rmdir(run_dir.absoluteFilePath()); // only when empty
        }
        if (removed > 0)
            LOG_INFO(kArtefactTag, QStringLiteral("Sweep removed %1 unreferenced file(s)").arg(removed));
    });
}

void ArtefactService::Impl::extract_now(const QString& run_id, int only_step) {
    auto& runs = TaskRunService::instance();
    const TaskRun* run = runs.find_run(run_id);
    if (!run)
        return;
    const QVector<al::ArtefactDraft> drafts = al::plan_run_artefacts(*run, runs.steps(run_id), keys_of(run_id), only_step);
    if (drafts.isEmpty())
        return;
    LOG_INFO(kArtefactTag, QStringLiteral("Run %1: %2 artefact(s) from %3")
                               .arg(run_id.left(8))
                               .arg(drafts.size())
                               .arg(only_step >= 0 ? QStringLiteral("step %1").arg(only_step + 1) : QStringLiteral("the run")));
    for (const al::ArtefactDraft& d : drafts) {
        RunArtefact a;
        a.run_id = run_id;
        a.step_idx = d.step_idx;
        a.kind = d.kind;
        a.title = d.title;
        a.source = d.source;
        QByteArray bytes;
        if (asv_is_tabular_kind(d.kind)) {
            bytes = al::table_to_csv(d.table).toUtf8();
            if (d.table.cell_count() <= al::kInlineTableMaxCells)
                a.inline_json = al::table_to_json(d.table);
        } else {
            bytes = d.text.toUtf8();
        }
        write_and_record(a, bytes, nullptr, {});
    }
}

// ── ArtefactService ──────────────────────────────────────────────────────────

ArtefactService& ArtefactService::instance() {
    static ArtefactService s;
    return s;
}

ArtefactService::ArtefactService() : QObject(nullptr), d_(std::make_unique<Impl>()) {
    d_->q = this;
    d_->worker = new QThreadPool(this);
    d_->worker->setMaxThreadCount(1); // one thread: a file is always written before its row, deletes after
    d_->quota_timer = new QTimer(this);
    d_->quota_timer->setSingleShot(true);
    d_->quota_timer->setInterval(kAsvQuotaCheckDelayMs);
    connect(d_->quota_timer, &QTimer::timeout, this, [this]() { d_->run_retention(false); });
    if (auto* app = QCoreApplication::instance()) {
        if (thread() != app->thread())
            moveToThread(app->thread());
        // Teardown: pending writes finish while the database is still open.
        connect(app, &QCoreApplication::aboutToQuit, this, [this]() { d_->worker->waitForDone(3000); });
    }
}

ArtefactService::~ArtefactService() = default;

void ArtefactService::initialize_deferred() {
    if (d_->initialized)
        return;
    d_->initialized = true;
    auto& runs = TaskRunService::instance();
    // A step's deliverable as soon as the step ends; the report when the run ends.
    connect(&runs, &TaskRunService::run_event, this, [this](const QString& run_id, const RunEvent& ev) {
        if (ev.kind != RunEventKind::StepEnd || ev.data.value(QStringLiteral("sub")).toString() == QLatin1String("window"))
            return;
        if (ev.data.value(QStringLiteral("status")).toString() != step_status_id(StepStatus::Done))
            return;
        ensure_extracted(run_id, ev.step);
    });
    connect(&runs, &TaskRunService::run_finished, this, [this](const QString& run_id) { ensure_extracted(run_id); });
}

QString ArtefactService::root_folder() {
    return AppPaths::files() + QStringLiteral("/agent-runs");
}

QString ArtefactService::run_folder(const QString& run_id) {
    return root_folder() + QLatin1Char('/') + run_id;
}

QString ArtefactService::file_path(const RunArtefact& artefact) {
    if (artefact.stored_name.isEmpty())
        return {};
    return run_folder(artefact.run_id) + QLatin1Char('/') + artefact.stored_name;
}

bool ArtefactService::is_loaded() const {
    return d_->loaded;
}

QVector<RunArtefact> ArtefactService::artefacts(const QString& run_id) const {
    QVector<RunArtefact> out;
    for (const QString& id : d_->by_run.value(run_id))
        out.append(d_->by_id.value(id));
    return out;
}

QVector<RunArtefact> ArtefactService::pinned_artefacts() const {
    QVector<RunArtefact> out;
    for (auto it = d_->by_id.cbegin(); it != d_->by_id.cend(); ++it)
        if (it->pinned)
            out.append(it.value());
    std::sort(out.begin(), out.end(), [](const RunArtefact& a, const RunArtefact& b) {
        return a.created_at_ms != b.created_at_ms ? a.created_at_ms > b.created_at_ms : a.id < b.id;
    });
    return out;
}

std::optional<RunArtefact> ArtefactService::find(const QString& artefact_id) const {
    const auto it = d_->by_id.constFind(artefact_id);
    if (it == d_->by_id.cend())
        return std::nullopt;
    return it.value();
}

qint64 ArtefactService::total_bytes() const {
    qint64 total = 0;
    for (auto it = d_->by_id.cbegin(); it != d_->by_id.cend(); ++it)
        total += std::max<qint64>(0, it->bytes);
    return total;
}

// ── Making artefacts ─────────────────────────────────────────────────────────

void ArtefactService::save_final_report(const QString& run_id, QObject* ctx, DoneCallback cb) {
    const TaskRun* run = TaskRunService::instance().find_run(run_id);
    if (!run || run->final_text.trimmed().isEmpty()) {
        if (cb)
            asv_post(ctx, [cb]() { cb(false, {}, asv_tr("This run has no answer to keep yet.")); });
        return;
    }
    const QString key = al::run_produces_artefacts(*run) ? QStringLiteral("final") : QStringLiteral("reply");
    for (const RunArtefact& a : artefacts(run_id)) {
        if (al::artefact_key(a) == key) {
            if (cb) {
                const QString id = a.id;
                asv_post(ctx, [cb, id]() { cb(true, id, {}); });
            }
            return;
        }
    }
    QJsonObject source;
    source[QStringLiteral("origin")] = key == QLatin1String("final") ? QStringLiteral("final") : QStringLiteral("reply");
    source[QStringLiteral("key")] = key;
    const QString title = (key == QLatin1String("final") ? asv_tr("Final report") : asv_tr("Reply")) +
                          QStringLiteral(" — ") + run->title.left(100);
    save_text(run_id, -1, QString::fromLatin1(kArtefactReport), title, run->final_text, source, ctx, std::move(cb));
}

void ArtefactService::extract_tables(const QString& run_id, int step_idx, const QString& markdown, QObject* ctx,
                                     ListCallback cb) {
    const QVector<al::MarkdownTable> tables = al::find_markdown_tables(markdown);
    if (tables.isEmpty()) {
        if (cb)
            asv_post(ctx, [cb]() { cb(true, {}, {}); });
        return;
    }
    struct Collect {
        int left = 0;
        QStringList ids;
        QString error;
    };
    auto collect = std::make_shared<Collect>();
    collect->left = static_cast<int>(tables.size());
    for (int i = 0; i < tables.size(); ++i) {
        QJsonObject source;
        source[QStringLiteral("origin")] = QStringLiteral("table");
        source[QStringLiteral("step")] = step_idx;
        const QString title = tables.at(i).caption.isEmpty() ? asv_tr("Table %1").arg(i + 1) : tables.at(i).caption;
        save_table(run_id, step_idx, title, tables.at(i).table, source, ctx,
                   [collect, cb](bool ok, const QString& id, const QString& err) {
                       if (ok)
                           collect->ids << id;
                       else
                           collect->error = err;
                       if (--collect->left == 0 && cb)
                           cb(!collect->ids.isEmpty(), collect->ids, collect->error);
                   });
    }
}

void ArtefactService::save_text(const QString& run_id, int step_idx, const QString& kind, const QString& title,
                                const QString& text, const QJsonObject& source, QObject* ctx, DoneCallback cb) {
    if (text.trimmed().isEmpty()) {
        if (cb)
            asv_post(ctx, [cb]() { cb(false, {}, asv_tr("There is no text to keep.")); });
        return;
    }
    RunArtefact a;
    a.run_id = run_id;
    a.step_idx = step_idx;
    a.kind = kind.isEmpty() ? QString::fromLatin1(kArtefactReport) : kind;
    a.title = title;
    a.source = source;
    if (!a.source.contains(QStringLiteral("origin")))
        a.source[QStringLiteral("origin")] = QStringLiteral("user");
    d_->write_and_record(a, text.toUtf8(), ctx, std::move(cb));
}

void ArtefactService::save_table(const QString& run_id, int step_idx, const QString& title,
                                 const al::ArtefactTable& table, const QJsonObject& source, QObject* ctx,
                                 DoneCallback cb) {
    if (table.isEmpty()) {
        if (cb)
            asv_post(ctx, [cb]() { cb(false, {}, asv_tr("The table is empty.")); });
        return;
    }
    RunArtefact a;
    a.run_id = run_id;
    a.step_idx = step_idx;
    a.kind = QString::fromLatin1(kArtefactTable);
    a.title = title;
    a.source = source;
    if (!a.source.contains(QStringLiteral("origin")))
        a.source[QStringLiteral("origin")] = QStringLiteral("user");
    if (table.cell_count() <= al::kInlineTableMaxCells)
        a.inline_json = al::table_to_json(table);
    d_->write_and_record(a, al::table_to_csv(table).toUtf8(), ctx, std::move(cb));
}

void ArtefactService::save_tool_result(const QString& run_id, int step_idx, const QString& tool,
                                       const QJsonValue& result, const QString& preview_text, const QJsonObject& source_in,
                                       QObject* ctx, DoneCallback cb) {
    QJsonObject source = source_in;
    source[QStringLiteral("origin")] = QStringLiteral("tool");
    if (!tool.isEmpty())
        source[QStringLiteral("tool")] = tool;
    if (step_idx >= 0)
        source[QStringLiteral("step")] = step_idx;
    const QString name = tool.trimmed().isEmpty() ? asv_tr("Tool") : tool.trimmed();
    const al::ToolResultView view = al::classify_tool_result(result, preview_text, true);
    switch (view.shape) {
        case al::ResultShape::Table: {
            const QString title = view.text.isEmpty() ? asv_tr("%1 result").arg(name) : asv_tr("%1 — %2").arg(name, view.text);
            save_table(run_id, step_idx, title, view.table, source, ctx, std::move(cb));
            return;
        }
        case al::ResultShape::KeyValue: {
            al::ArtefactTable t;
            t.columns = {asv_tr("Field"), asv_tr("Value")};
            for (const auto& f : view.fields)
                t.rows.append({f.first, f.second});
            save_table(run_id, step_idx, asv_tr("%1 result").arg(name), t, source, ctx, std::move(cb));
            return;
        }
        default:
            break;
    }
    QString text;
    QString kind = QString::fromLatin1(kArtefactText);
    if (view.value.isObject() || view.value.isArray()) {
        kind = QString::fromLatin1(kArtefactJson);
        text = view.value.isObject() ? QString::fromUtf8(QJsonDocument(view.value.toObject()).toJson(QJsonDocument::Indented))
                                     : QString::fromUtf8(QJsonDocument(view.value.toArray()).toJson(QJsonDocument::Indented));
    } else {
        text = view.text.isEmpty() ? preview_text : view.text;
    }
    save_text(run_id, step_idx, kind, asv_tr("%1 result").arg(name), text, source, ctx, std::move(cb));
}

void ArtefactService::ensure_extracted(const QString& run_id, int only_step) {
    if (run_id.isEmpty())
        return;
    if (!d_->loaded) {
        d_->deferred_extract.insert(only_step >= 0 ? run_id + QStringLiteral("#%1").arg(only_step) : run_id);
        return;
    }
    d_->extract_now(run_id, only_step);
}

QString ArtefactService::add_row(const QString& run_id, const RunArtefact& artefact) {
    RunArtefact a = artefact;
    a.run_id = run_id;
    if (a.id.isEmpty())
        a.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    d_->write_and_record(a, {}, nullptr, {});
    return a.id;
}

// ── Using artefacts ──────────────────────────────────────────────────────────

void ArtefactService::load_content(const QString& artefact_id, QObject* ctx, ContentCallback cb) {
    if (!cb)
        return;
    const auto found = find(artefact_id);
    if (!found) {
        asv_post(ctx, [cb]() {
            ArtefactContent c;
            c.error = asv_tr("This artefact was deleted.");
            cb(c);
        });
        return;
    }
    const RunArtefact a = *found;
    // A small table kept inline: no file read.
    if (asv_is_tabular_kind(a.kind) && a.inline_json.isObject()) {
        ArtefactContent c;
        c.ok = true;
        c.kind = a.kind;
        c.table = al::table_from_json(a.inline_json);
        c.file_path = file_path(a);
        c.bytes = a.bytes;
        asv_post(ctx, [cb, c]() { cb(c); });
        return;
    }
    const QString path = file_path(a);
    d_->submit([a, path, ctx, cb]() {
        ArtefactContent c;
        c.kind = a.kind;
        c.file_path = path;
        c.bytes = a.bytes;
        if (path.isEmpty()) {
            // Inline-only.
            if (a.inline_json.isObject() || a.inline_json.isArray()) {
                c.ok = true;
                c.table = al::table_from_json(a.inline_json);
                c.text = a.inline_json.isObject()
                             ? QString::fromUtf8(QJsonDocument(a.inline_json.toObject()).toJson(QJsonDocument::Indented))
                             : QString::fromUtf8(QJsonDocument(a.inline_json.toArray()).toJson(QJsonDocument::Indented));
            } else {
                c.ok = !a.inline_json.toString().isEmpty();
                c.text = a.inline_json.toString();
                if (!c.ok)
                    c.error = asv_tr("This artefact has no content.");
            }
            asv_post(ctx, [cb, c]() { cb(c); });
            return;
        }
        QString err;
        if (a.kind == QLatin1String(kArtefactImage)) {
            // The bytes too: "Add to report" stores them as they are (no re-encoding).
            const QByteArray raw = asv_read(path, al::kArtefactMaxBytes, &err);
            QImage img;
            c.ok = err.isEmpty() && img.loadFromData(raw);
            if (c.ok) {
                c.image = img;
                c.image_bytes = raw;
                const QString suffix = QFileInfo(path).suffix().toLower();
                c.image_format = suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg") ? QStringLiteral("jpg")
                                                                                                  : QStringLiteral("png");
                if (c.image_format == QLatin1String("png") && suffix != QLatin1String("png")) {
                    // Another format (gif, bmp, webp …): re-encoded once as PNG for the report.
                    QByteArray png;
                    QBuffer buf(&png);
                    if (buf.open(QIODevice::WriteOnly) && img.save(&buf, "PNG"))
                        c.image_bytes = png;
                    else
                        c.image_bytes.clear();
                }
            } else {
                c.error = err.isEmpty() ? asv_tr("The image could not be read.") : err;
            }
            asv_post(ctx, [cb, c]() { cb(c); });
            return;
        }
        const QByteArray raw = asv_read(path, al::kArtefactMaxBytes, &err);
        if (!err.isEmpty()) {
            c.error = err;
            asv_post(ctx, [cb, c]() { cb(c); });
            return;
        }
        c.ok = true;
        if (asv_is_tabular_kind(a.kind)) {
            c.table = al::table_from_csv(QString::fromUtf8(raw));
        } else if (a.kind == QLatin1String(kArtefactJson)) {
            const QJsonDocument doc = QJsonDocument::fromJson(raw);
            c.text = doc.isNull() ? QString::fromUtf8(raw) : QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
            if (!doc.isNull())
                c.table = al::table_from_json(doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object()));
        } else if (a.kind == QLatin1String(kArtefactFile)) {
            // A file of unknown type: shown as text when it reads as text.
            if (!raw.left(4096).contains('\0'))
                c.text = QString::fromUtf8(raw.left(256 * 1024));
        } else {
            c.text = QString::fromUtf8(raw);
        }
        asv_post(ctx, [cb, c]() { cb(c); });
    });
}

void ArtefactService::export_artefact(const QString& run_id, const QString& artefact_id, const QString& dest_path,
                                      QObject* ctx, ResultCallback cb) {
    Q_UNUSED(run_id)
    const auto found = find(artefact_id);
    if (!found || dest_path.trimmed().isEmpty()) {
        if (cb)
            asv_post(ctx, [cb]() { cb(false, asv_tr("Nothing to save: the artefact is gone.")); });
        return;
    }
    QPointer<ArtefactService> self(this);
    load_content(artefact_id, this, [self, a = *found, dest_path, ctx, cb](const ArtefactContent& c) {
        if (!self)
            return;
        if (!c.ok) {
            if (cb)
                asv_post(ctx, [cb, err = c.error]() { cb(false, err); });
            return;
        }
        self->d_->submit([a, c, dest_path, ctx, cb]() {
            const QString ext = QFileInfo(dest_path).suffix().toLower();
            const bool tabular = !c.table.isEmpty();
            QString err;
            bool ok = false;
            if (ext == QLatin1String("xlsx")) {
#ifdef FINCEPT_HAS_QXLSX
                if (!tabular) {
                    err = asv_tr("Only tables can be saved as Excel files.");
                } else {
                    QXlsx::Document xlsx;
                    for (int c0 = 0; c0 < c.table.columns.size(); ++c0)
                        xlsx.write(1, c0 + 1, c.table.columns.at(c0));
                    for (int r = 0; r < c.table.rows.size(); ++r) {
                        const QStringList& row = c.table.rows.at(r);
                        for (int c0 = 0; c0 < row.size(); ++c0) {
                            double v = 0;
                            if (asv_number(row.at(c0), &v))
                                xlsx.write(r + 2, c0 + 1, v);
                            else
                                xlsx.write(r + 2, c0 + 1, row.at(c0));
                        }
                    }
                    ok = xlsx.saveAs(dest_path);
                    if (!ok)
                        err = asv_tr("The Excel file could not be written.");
                }
#else
                err = asv_tr("Excel export is not part of this build: save the table as CSV instead.");
#endif
            } else {
                QByteArray bytes;
                if (ext == QLatin1String("csv") && tabular)
                    bytes = al::table_to_csv(c.table).toUtf8();
                else if ((ext == QLatin1String("md") || ext == QLatin1String("markdown")) && tabular && c.text.isEmpty())
                    bytes = al::table_to_markdown(c.table).toUtf8();
                else if (ext == QLatin1String("json") && tabular && a.kind != QLatin1String(kArtefactJson))
                    bytes = QJsonDocument(al::table_to_json(c.table)).toJson(QJsonDocument::Indented);
                else if (!c.file_path.isEmpty() && QFileInfo::exists(c.file_path))
                    bytes = asv_read(c.file_path, al::kArtefactMaxBytes, &err);
                else if (tabular)
                    bytes = al::table_to_csv(c.table).toUtf8();
                else
                    bytes = c.text.toUtf8();
                if (err.isEmpty())
                    ok = asv_write_atomic(dest_path, bytes, &err);
            }
            asv_post(ctx, [cb, ok, err]() {
                if (cb)
                    cb(ok, ok ? QString() : asv_tr("Not saved: %1").arg(err));
            });
        });
    });
}

void ArtefactService::register_with_file_manager(const QString& run_id, const QString& artefact_id, QObject* ctx,
                                                 ResultCallback cb) {
    Q_UNUSED(run_id)
    const auto found = find(artefact_id);
    const QString src = found ? file_path(*found) : QString();
    if (src.isEmpty()) {
        if (cb)
            asv_post(ctx, [cb]() { cb(false, asv_tr("This artefact has no file.")); });
        return;
    }
    const RunArtefact a = *found;
    const QString storage = services::FileManagerService::instance().storage_dir();
    const QString stored = QStringLiteral("%1_%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(a.stored_name);
    d_->submit([a, src, storage, stored, ctx, cb]() {
        QString err;
        const QByteArray bytes = asv_read(src, al::kArtefactMaxBytes, &err);
        const bool ok = err.isEmpty() && asv_write_atomic(storage + QLatin1Char('/') + stored, bytes, &err);
        const qint64 size = bytes.size();
        asv_post(ctx, [a, stored, size, ok, err, cb]() {
            if (ok)
                services::FileManagerService::instance().register_file(stored, a.title + QLatin1Char('.') +
                                                                                    QFileInfo(a.stored_name).suffix(),
                                                                       size, a.mime, QStringLiteral("chat_mode"));
            if (cb)
                cb(ok, ok ? QString() : err);
        });
    });
}

void ArtefactService::send_to_notes(const QString& artefact_id, QObject* ctx, NoteCallback cb) {
    const auto found = find(artefact_id);
    if (!found) {
        if (cb)
            asv_post(ctx, [cb]() { cb(false, 0, asv_tr("This artefact was deleted.")); });
        return;
    }
    const RunArtefact a = *found;
    QStringList tickers;
    if (const TaskRun* run = TaskRunService::instance().find_run(a.run_id)) {
        const QString sym = run->context.value(QStringLiteral("symbol")).toString();
        if (!sym.isEmpty())
            tickers << sym;
        for (const QJsonValue& v : run->context.value(QStringLiteral("symbols")).toArray())
            if (!v.toString().isEmpty() && !tickers.contains(v.toString()))
                tickers << v.toString();
    }
    QPointer<ArtefactService> self(this);
    load_content(artefact_id, this, [self, a, tickers, ctx, cb](const ArtefactContent& c) {
        if (!self)
            return;
        if (!c.ok) {
            if (cb)
                asv_post(ctx, [cb, err = c.error]() { cb(false, 0, err); });
            return;
        }
        QString body;
        if (!c.table.isEmpty() && (asv_is_tabular_kind(a.kind) || c.text.isEmpty()))
            body = al::table_to_markdown(c.table, 500);
        else if (a.kind == QLatin1String(kArtefactJson))
            body = QStringLiteral("```json\n") + c.text + QStringLiteral("\n```");
        else
            body = c.text;
        QStringList where;
        where << asv_tr("agent run “%1” (%2)").arg(a.run_title.isEmpty() ? asv_tr("deleted run") : a.run_title,
                                                   a.run_id.left(8));
        if (a.step_idx >= 0)
            where << asv_tr("step %1").arg(a.step_idx + 1);
        body += QStringLiteral("\n\n---\n") +
                asv_tr("Saved from Chat Mode: %1 · artefact “%2” · %3")
                    .arg(where.join(QStringLiteral(", ")), a.title,
                         QLocale().toString(QDateTime::fromMSecsSinceEpoch(a.created_at_ms), QLocale::ShortFormat));
        FinancialNote note;
        note.title = a.title.left(200);
        note.content = body;
        note.category = QStringLiteral("RESEARCH");
        note.priority = QStringLiteral("MEDIUM");
        note.tags = QStringLiteral("agent-run");
        note.tickers = tickers.join(QLatin1Char(','));
        note.sentiment = QStringLiteral("NEUTRAL");
        note.color_code = QStringLiteral("");
        note.reminder_date = QStringLiteral("");
        note.word_count = static_cast<int>(body.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).size());
        const QString run_id = a.run_id;
        const int step_idx = a.step_idx;
        self->d_->submit([note, run_id, step_idx, ctx, cb]() {
            auto r = NotesRepository::instance().create(note);
            const bool ok = r.is_ok();
            const int id = ok ? static_cast<int>(r.value()) : 0;
            const QString err = ok ? QString() : QString::fromStdString(r.error());
            if (ok) {
                // The note's board row names its run, so Notes' OPEN CHAT MODE finds it
                // even when the Notes screen was not open to hear the event below. An
                // existing row (Notes read the new note first) is left as it is.
                auto m = NoteBoardRepository::instance().insert_missing_meta(
                    {al::note_board_row_for_run(id, note.category, run_id, QDateTime::currentSecsSinceEpoch())});
                if (m.is_err())
                    LOG_WARN(kArtefactTag, QStringLiteral("Note %1: its link to run %2 was not stored: %3")
                                               .arg(id)
                                               .arg(run_id.left(8), QString::fromStdString(m.error())));
            }
            asv_post(ctx, [ok, id, run_id, step_idx, err, cb]() {
                if (ok) // the Notes screen and the dashboard tile reload on this
                    EventBus::instance().publish(QStringLiteral("notes.created"),
                                                 al::note_created_event(id, run_id, step_idx));
                if (cb)
                    cb(ok, id, ok ? QString() : asv_tr("The note could not be created: %1").arg(err));
            });
        });
    });
}

void ArtefactService::set_pinned(const QString& artefact_id, bool pinned) {
    auto it = d_->by_id.find(artefact_id);
    if (it == d_->by_id.end() || it->pinned == pinned)
        return;
    it->pinned = pinned;
    const RunArtefact a = it.value();
    d_->persist_row(a);
    emit artefacts_changed(a.run_id);
    if (!pinned && !TaskRunService::instance().find_run(a.run_id)) {
        // Unpinned after its run was deleted: nothing keeps it any more.
        d_->remove(a.id);
        d_->erase_on_disk({a}, {run_folder(a.run_id)});
        emit artefacts_changed(a.run_id);
    }
}

void ArtefactService::delete_artefact(const QString& artefact_id, QObject* ctx, ResultCallback cb) {
    const auto found = find(artefact_id);
    if (!found) {
        if (cb)
            asv_post(ctx, [cb]() { cb(true, {}); });
        return;
    }
    const RunArtefact a = *found;
    d_->remove(a.id);
    d_->erase_on_disk({a}, {run_folder(a.run_id)});
    emit artefacts_changed(a.run_id);
    if (cb)
        asv_post(ctx, [cb]() { cb(true, {}); });
}

// ── Run lifecycle ────────────────────────────────────────────────────────────

void ArtefactService::adopt_loaded(const QVector<RunArtefact>& rows) {
    for (const RunArtefact& a : rows)
        if (!d_->by_id.contains(a.id))
            d_->insert(a);
    d_->loaded = true;
    LOG_INFO(kArtefactTag, QStringLiteral("Loaded %1 artefact(s), %2 bytes").arg(d_->by_id.size()).arg(total_bytes()));
    emit artefacts_changed(QString());
    // Unpinned artefacts of runs that are gone (deleted, or pruned at start-up),
    // the size quota, and files no row refers to.
    QTimer::singleShot(0, this, [this]() { d_->run_retention(true); });
    const QSet<QString> deferred = std::exchange(d_->deferred_extract, {});
    for (const QString& entry : deferred) {
        const int hash = static_cast<int>(entry.indexOf(QLatin1Char('#')));
        if (hash > 0)
            d_->extract_now(entry.left(hash), entry.mid(hash + 1).toInt());
        else
            d_->extract_now(entry, -1);
    }
}

void ArtefactService::on_runs_deleted(const QStringList& run_ids) {
    for (const QString& run : run_ids) {
        QVector<RunArtefact> gone;
        for (const QString& id : d_->by_run.value(run)) {
            const RunArtefact a = d_->by_id.value(id);
            if (!a.pinned)
                gone.append(a);
        }
        for (const RunArtefact& a : std::as_const(gone))
            d_->remove(a.id);
        d_->erase_on_disk(gone, {run_folder(run)});
        emit artefacts_changed(run);
    }
}

} // namespace fincept::agent_tasks
