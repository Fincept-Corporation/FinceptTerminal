// NotebookLibraryService.cpp

#include "services/notebooks/NotebookLibraryService.h"

#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "python/PythonSetupManager.h"
#include "services/file_manager/FileManagerService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSet>
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>
#include <QUuid>

#include <algorithm>
#include <utility>

namespace fincept::services {

static constexpr const char* kTag = "NotebookLibrary";

// ── Singleton ────────────────────────────────────────────────────────────────

NotebookLibraryService& NotebookLibraryService::instance() {
    static NotebookLibraryService s_instance;
    return s_instance;
}

NotebookLibraryService::NotebookLibraryService(QObject* parent) : QObject(parent) {}

// ── Asset resolution ──────────────────────────────────────────────────────────
// Mirrors PythonRunner::find_scripts_dir / ComponentCatalog::load_with_fallbacks:
// the notebooks ship under resources/notebooks/ next to the binary (POST_BUILD)
// and inside the app bundle (install). Walk a handful of candidate layouts, then
// fall back to walking up the tree to find the source-tree copy in dev runs.

QString NotebookLibraryService::assets_dir() {
    if (assets_dir_resolved_)
        return assets_dir_cache_;
    assets_dir_resolved_ = true;

    const QString exe = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        exe + "/resources/notebooks",              // dev + Windows/Linux next-to-exe
        exe + "/../Resources/resources/notebooks", // macOS .app/Contents/Resources
        exe + "/../resources/notebooks",
        exe + "/../../resources/notebooks",
        exe + "/../share/fincept-terminal/resources/notebooks", // Linux install
    };
    for (const QString& c : candidates) {
        if (QFileInfo::exists(c + "/notebooks_manifest.json")) {
            assets_dir_cache_ = QDir(c).absolutePath();
            return assets_dir_cache_;
        }
    }

    // Dev fallback: walk up from the executable looking for the source-tree copy.
    QDir dir(exe);
    for (int i = 0; i < 8 && dir.cdUp(); ++i) {
        const QString p = dir.filePath("resources/notebooks/notebooks_manifest.json");
        if (QFileInfo::exists(p)) {
            assets_dir_cache_ = QFileInfo(p).absolutePath();
            return assets_dir_cache_;
        }
        const QString p2 = dir.filePath("fincept-qt/resources/notebooks/notebooks_manifest.json");
        if (QFileInfo::exists(p2)) {
            assets_dir_cache_ = QFileInfo(p2).absolutePath();
            return assets_dir_cache_;
        }
    }

    LOG_WARN(kTag, "Notebook assets dir not found");
    assets_dir_cache_.clear();
    return assets_dir_cache_;
}

QString NotebookLibraryService::asset_path(const NotebookCatalogEntry& entry) {
    const QString dir = assets_dir();
    if (dir.isEmpty() || entry.file.isEmpty())
        return {};
    return dir + "/" + entry.file;
}

// ── Catalog ───────────────────────────────────────────────────────────────────

const QVector<NotebookCatalogEntry>& NotebookLibraryService::catalog() {
    if (!loaded_) {
        load_catalog();
        loaded_ = true;
    }
    return catalog_;
}

void NotebookLibraryService::load_catalog() {
    catalog_.clear();

    const QString dir = assets_dir();
    if (dir.isEmpty())
        return;

    QFile f(dir + "/notebooks_manifest.json");
    if (!f.open(QIODevice::ReadOnly)) {
        LOG_WARN(kTag, "Cannot open notebooks_manifest.json");
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return;

    for (const QJsonValue& v : doc.object()["notebooks"].toArray()) {
        const QJsonObject o = v.toObject();
        NotebookCatalogEntry e;
        e.id = o["id"].toString();
        e.title = o["title"].toString();
        e.category = o["category"].toString();
        e.difficulty = o["difficulty"].toString();
        e.est_minutes = o["est_minutes"].toInt();
        e.requirements = o["requires"].toString();
        e.summary = o["summary"].toString();
        e.file = o["file"].toString();
        if (!e.id.isEmpty() && !e.file.isEmpty())
            catalog_.append(e);
    }
    LOG_INFO(kTag, QString("Loaded %1 prebuilt notebooks").arg(catalog_.size()));
}

// ── Seeding ───────────────────────────────────────────────────────────────────

QString NotebookLibraryService::seed_marker_path() const {
    return fincept::AppPaths::data() + "/.fincept_notebooks_seeded_v1";
}

void NotebookLibraryService::seed_into_files(bool force) {
    if (!force && QFileInfo::exists(seed_marker_path()))
        return;

    const auto& cat = catalog();
    if (cat.isEmpty())
        return; // assets missing — try again next launch (no marker written)

    auto& fm = FileManagerService::instance();

    // Index existing code_editor files by their original name so re-seeding never
    // duplicates a notebook the user already has.
    QSet<QString> existing;
    for (const QJsonValue& v : fm.files_for_screen("code_editor"))
        existing.insert(v.toObject()["originalName"].toString());

    int seeded = 0;
    for (const NotebookCatalogEntry& e : cat) {
        if (existing.contains(e.file))
            continue;
        const QString src = asset_path(e);
        if (src.isEmpty() || !QFileInfo::exists(src))
            continue;
        if (!fm.import_file(src, "code_editor").isEmpty())
            ++seeded;
    }

    // Mark seeded only once we actually have the assets, so a launch before the
    // bundle is in place doesn't permanently skip seeding.
    QFile marker(seed_marker_path());
    if (marker.open(QIODevice::WriteOnly)) {
        marker.write("seeded\n");
        marker.close();
    }
    LOG_INFO(kTag, QString("Seeded %1 prebuilt notebooks into File Manager").arg(seeded));
}

// ── Working copy (copy-on-open) ────────────────────────────────────────────────

QString NotebookLibraryService::working_copy_for(const NotebookCatalogEntry& entry) {
    auto& fm = FileManagerService::instance();

    // Reuse an existing managed copy if one is already present (e.g. seeded).
    for (const QJsonValue& v : fm.files_for_screen("code_editor")) {
        const QJsonObject o = v.toObject();
        if (o["originalName"].toString() != entry.file)
            continue;
        const QString p = fm.full_path(o["name"].toString());
        if (!p.isEmpty() && QFileInfo::exists(p))
            return p;
    }

    // Otherwise import the bundled asset into managed storage.
    const QString src = asset_path(entry);
    if (!src.isEmpty() && QFileInfo::exists(src)) {
        const QString id = fm.import_file(src, "code_editor");
        if (!id.isEmpty()) {
            const QString p = fm.full_path(fm.find_by_id(id).name);
            if (!p.isEmpty())
                return p;
        }
    }

    // Last resort: open the read-only bundled asset directly.
    return src;
}

// ── Cell execution ────────────────────────────────────────────────────────────
// Runs scripts/notebook_cell_runner.py through PythonRunner (D1/P4). The cell
// source goes over stdin; the runner streams the cell's stdout/stderr and
// reports structured events on lines prefixed with "\x1eFNB ".

namespace {
constexpr int kNbMaxOutputChars = 2 * 1024 * 1024; // per run; the rest is dropped and flagged
constexpr int kNbFlushIntervalMs = 100;
const QLatin1String kNbEventMarker("\x1e" "FNB ");
} // namespace

struct NotebookLibraryService::ActiveRun {
    QPointer<QObject> context;
    NotebookStreamCallback on_output;
    NotebookRunCallback on_done;
    NotebookRunResult result;
    QString cancel_file;
    QVector<QPair<bool, QString>> pending; // not yet delivered to on_output
    qint64 chars = 0;
    bool started = false;  // the process is running (its "started" event arrived)
    bool done_seen = false; // the runner reported a final status
    bool resolved = false;  // on_done already fired (e.g. cancelled while queued)
};

QString NotebookLibraryService::kernel_python(const QString& kernel, QString* why) {
    if (kernel != QLatin1String("numpy1"))
        return {}; // default kernel: PythonRunner's own interpreter (venv-numpy2)
    const QString py = python::PythonSetupManager::instance().python_path(QStringLiteral("venv-numpy1"));
    if (!QFileInfo::exists(py)) {
        if (why)
            *why = tr("The numpy1 environment (venv-numpy1) is not installed. Run the Python setup from "
                      "Settings → Python Env, or switch the kernel back to NUMPY2.");
        return {};
    }
    return py;
}

static void nb_append(QVector<QPair<bool, QString>>& chunks, bool is_stderr, const QString& text) {
    if (!chunks.isEmpty() && chunks.last().first == is_stderr)
        chunks.last().second += text;
    else
        chunks.push_back({is_stderr, text});
}

QString NotebookLibraryService::run_cell(const NotebookRunRequest& request, QObject* context,
                                         NotebookStreamCallback on_output, NotebookRunCallback cb) {
    const QString run_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto run = std::make_shared<ActiveRun>();
    run->context = context;
    run->on_output = std::move(on_output);
    run->on_done = std::move(cb);

    QString why;
    const QString kernel_py = kernel_python(request.kernel, &why);
    if (request.kernel == QLatin1String("numpy1") && kernel_py.isEmpty()) {
        run->result.status = NotebookRunResult::Status::Failed;
        run->result.failure = why;
        // Deliver asynchronously so callers see the same contract either way.
        QPointer<QObject> ctx = context;
        QMetaObject::invokeMethod(
            this,
            [run, ctx]() {
                if (ctx && run->on_done)
                    run->on_done(run->result);
            },
            Qt::QueuedConnection);
        return run_id;
    }

    run->cancel_file = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                           .filePath(QStringLiteral("fincept_nbcancel_%1").arg(run_id));
    runs_.insert(run_id, run);

    if (!flush_timer_) {
        flush_timer_ = new QTimer(this);
        flush_timer_->setInterval(kNbFlushIntervalMs);
        connect(flush_timer_, &QTimer::timeout, this, &NotebookLibraryService::flush_output);
    }
    if (!flush_timer_->isActive())
        flush_timer_->start();

    QStringList args{QStringLiteral("run"), QStringLiteral("--cancel-file"), run->cancel_file,
                     QStringLiteral("--timeout"), QString::number(std::max(1, request.timeout_s))};
    if (!request.working_dir.isEmpty())
        args << QStringLiteral("--cwd") << request.working_dir;
    if (!kernel_py.isEmpty())
        args << QStringLiteral("--python") << kernel_py;

    python::PythonRunner::RunOptions opts;
    opts.expect_json = false;
    // The runner enforces the cell limit itself and reports it; the watchdog is
    // only the backstop for a runner that cannot (grace + process start-up).
    opts.timeout_ms = (std::max(1, request.timeout_s) + 30) * 1000;
    opts.stdin_data = request.code.toUtf8();
    if (opts.stdin_data.isEmpty())
        opts.stdin_data = QByteArrayLiteral("\n");

    QPointer<NotebookLibraryService> self = this;
    python::PythonRunner::instance().run_with_options(
        QStringLiteral("notebook_cell_runner.py"), args, opts,
        [self, run_id](python::PythonResult r) {
            if (self)
                self->finish_run(run_id, r.exit_code, r.success ? QString() : r.error);
        },
        [self, run_id](QString line, bool is_stderr) {
            if (self)
                self->on_run_line(run_id, line, is_stderr);
        });
    return run_id;
}

void NotebookLibraryService::on_run_line(const QString& run_id, const QString& line, bool is_stderr) {
    const auto it = runs_.constFind(run_id);
    if (it == runs_.constEnd())
        return;
    ActiveRun& run = **it;
    if (!is_stderr && line.startsWith(kNbEventMarker)) {
        const QJsonObject ev = QJsonDocument::fromJson(line.mid(kNbEventMarker.size()).toUtf8()).object();
        const QString type = ev.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("started")) {
            run.started = true;
        } else if (type == QLatin1String("result")) {
            run.result.result_text = ev.value(QStringLiteral("text")).toString();
        } else if (type == QLatin1String("error")) {
            run.result.error_name = ev.value(QStringLiteral("ename")).toString();
            run.result.error_value = ev.value(QStringLiteral("evalue")).toString();
            run.result.traceback.clear();
            for (const QJsonValue& v : ev.value(QStringLiteral("traceback")).toArray())
                run.result.traceback << v.toString().chopped(v.toString().endsWith(QLatin1Char('\n')) ? 1 : 0);
        } else if (type == QLatin1String("done")) {
            run.done_seen = true;
            run.result.elapsed_ms = ev.value(QStringLiteral("elapsed_ms")).toInteger();
            const QString st = ev.value(QStringLiteral("status")).toString();
            using S = NotebookRunResult::Status;
            run.result.status = st == QLatin1String("ok")          ? S::Ok
                                : st == QLatin1String("timeout")   ? S::TimedOut
                                : st == QLatin1String("cancelled") ? S::Cancelled
                                                                   : S::Error;
        }
        return;
    }
    if (run.chars >= kNbMaxOutputChars) {
        run.result.output_truncated = true;
        return;
    }
    const QString text = line + QLatin1Char('\n');
    run.chars += text.size();
    nb_append(run.result.streams, is_stderr, text);
    nb_append(run.pending, is_stderr, text);
}

void NotebookLibraryService::flush_output() {
    bool any_active = false;
    for (auto it = runs_.begin(); it != runs_.end(); ++it) {
        ActiveRun& run = **it;
        if (run.resolved)
            continue;
        any_active = true;
        if (run.pending.isEmpty())
            continue;
        const auto chunks = std::exchange(run.pending, {});
        if (!run.context || !run.on_output)
            continue;
        for (const auto& [is_stderr, text] : chunks)
            run.on_output(text, is_stderr);
    }
    if (!any_active && flush_timer_)
        flush_timer_->stop();
}

void NotebookLibraryService::cancel_run(const QString& run_id) {
    const auto it = runs_.constFind(run_id);
    if (it == runs_.constEnd())
        return;
    auto run = *it;
    if (run->resolved)
        return;
    QFile f(run->cancel_file);
    if (f.open(QIODevice::WriteOnly))
        f.close();
    if (!run->started) {
        // Still queued behind other Python work: report now and ignore whatever
        // the process says later (it exits as soon as it sees the cancel file).
        run->resolved = true;
        run->result.status = NotebookRunResult::Status::Cancelled;
        run->result.error_name = QStringLiteral("KeyboardInterrupt");
        run->result.error_value = tr("Cancelled before it started");
        if (run->context && run->on_done)
            run->on_done(run->result);
    }
}

void NotebookLibraryService::finish_run(const QString& run_id, int exit_code, const QString& runner_error) {
    const auto run = runs_.take(run_id);
    if (!run)
        return;
    QFile::remove(run->cancel_file);
    if (run->resolved)
        return; // already reported (cancelled while queued) — late result ignored
    run->resolved = true;
    if (!run->done_seen) {
        // The runner never reported: it could not start, crashed, or the
        // PythonRunner watchdog killed it.
        const bool timed_out = runner_error.contains(QLatin1String("timed out"), Qt::CaseInsensitive);
        run->result.status = timed_out ? NotebookRunResult::Status::TimedOut : NotebookRunResult::Status::Failed;
        QString msg = runner_error.trimmed();
        if (msg.size() > 2000)
            msg = QStringLiteral("…") + msg.right(2000);
        run->result.failure = msg.isEmpty() ? tr("The cell process exited with code %1").arg(exit_code) : msg;
        LOG_WARN(kTag, QStringLiteral("Notebook cell runner ended without a status (exit %1)").arg(exit_code));
    }
    // Deliver any output the flush timer has not handed over yet.
    if (run->context && run->on_output)
        for (const auto& [is_stderr, text] : std::exchange(run->pending, {}))
            run->on_output(text, is_stderr);
    if (run->context && run->on_done)
        run->on_done(run->result);
}

} // namespace fincept::services
