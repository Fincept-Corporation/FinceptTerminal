// NotebookKernel.cpp — see NotebookKernel.h.

#include "services/notebooks/NotebookKernel.h"

#include "core/logging/Logger.h"
#include "core/process/ProcessTree.h"
#include "python/PythonRunner.h"
#include "python/PythonSetupManager.h"
#include "services/notebooks/NotebookLibraryService.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTimer>

#include <algorithm>

namespace fincept::services {

namespace {

constexpr const char* kTag = "NotebookKernel";
const QByteArray kMarker("\x1e" "FNB ");
/// The kernel stops a cell that ignores KeyboardInterrupt after 3 s itself;
/// this is how long we wait before killing a kernel that did not even do that.
constexpr int kInterruptBackstopMs = 8000;
/// Added to a cell's own time limit before the process is killed as wedged.
constexpr int kDeadlineBackstopMs = 20000;
/// Graceful shutdown: how long "shutdown" + closed stdin get before a kill.
constexpr int kShutdownGraceMs = 1500;
constexpr int kStderrTailBytes = 4096;

/// Kill the kernel and everything it started; marked so its finished() handler
/// does not release the tree as if it had ended by itself.
void nb_kill_tree(QProcess* proc) {
    if (!proc)
        return;
    proc->setProperty("fincept_nb_killed", true);
    process::kill_tree(proc);
}

} // namespace

NotebookKernel::NotebookKernel(QObject* parent) : QObject(parent) {
    backstop_ = new QTimer(this);
    backstop_->setSingleShot(true);
    connect(backstop_, &QTimer::timeout, this, [this]() {
        if (!proc_ || run_id_.isEmpty())
            return;
        LOG_WARN(kTag, "Kernel did not stop the cell in time — killing it");
        reason_ = tr("The cell did not stop; the kernel was restarted and its variables were cleared.");
        nb_kill_tree(proc_); // finished() reports the run as kernel_lost
    });
}

NotebookKernel::~NotebookKernel() {
    if (proc_) {
        proc_->disconnect(this);
        nb_kill_tree(proc_);
        proc_->waitForFinished(500);
    }
}

void NotebookKernel::set_environment(const QString& env) {
    env_ = env == QLatin1String("numpy1") ? QStringLiteral("numpy1") : QStringLiteral("numpy2");
}

void NotebookKernel::set_chart_style(const QJsonObject& style) {
    if (style == chart_style_)
        return;
    chart_style_ = style;
    if (alive())
        send(QJsonObject{{QStringLiteral("op"), QStringLiteral("style")}, {QStringLiteral("style"), style}});
}

bool NotebookKernel::alive() const {
    return proc_ && proc_->state() != QProcess::NotRunning;
}

qint64 NotebookKernel::current_run_elapsed_ms() const {
    return run_id_.isEmpty() || !run_clock_.isValid() ? 0 : run_clock_.elapsed();
}

QString NotebookKernel::start_problem(const QString& env) {
    if (env == QLatin1String("numpy1")) {
        QString why;
        if (NotebookLibraryService::kernel_python(QStringLiteral("numpy1"), &why).isEmpty())
            return why;
    } else if (python::PythonRunner::instance().python_path().isEmpty()) {
        return python::PythonRunner::python_unavailable_message();
    }
    const QString script = python::PythonRunner::instance().scripts_dir() + QStringLiteral("/notebook_cell_runner.py");
    if (!QFileInfo::exists(script))
        return tr("The notebook runner (notebook_cell_runner.py) is missing from this installation. "
                  "Reinstall the terminal to restore it.");
    return {};
}

void NotebookKernel::set_state(State s) {
    if (state_ == s)
        return;
    state_ = s;
    emit state_changed(s);
}

bool NotebookKernel::start(QString* why) {
    if (alive())
        return true;
    const QString problem = start_problem(env_);
    if (!problem.isEmpty()) {
        reason_ = problem;
        set_state(State::Unavailable);
        if (why)
            *why = problem;
        return false;
    }
    auto& runner = python::PythonRunner::instance();
    const QString python = env_ == QLatin1String("numpy1")
                               ? NotebookLibraryService::kernel_python(QStringLiteral("numpy1"))
                               : runner.python_path();
    const QString script = runner.scripts_dir() + QStringLiteral("/notebook_cell_runner.py");
    QString dir = working_dir_;
    if (dir.isEmpty() || !QFileInfo(dir).isDir())
        dir = QDir::homePath();

    out_buf_.clear();
    err_tail_.clear();
    version_.clear();
    reason_.clear();
    stopping_ = false;

    proc_ = new QProcess(this);
    QProcess* const spawned = proc_;
    proc_->setProcessChannelMode(QProcess::SeparateChannels);
    QProcessEnvironment env = runner.build_python_env();
    if (!chart_style_.isEmpty())
        env.insert(QStringLiteral("FINCEPT_NB_MPL_STYLE"),
                   QString::fromUtf8(QJsonDocument(chart_style_).toJson(QJsonDocument::Compact)));
    proc_->setProcessEnvironment(env);
    proc_->setWorkingDirectory(dir);
    // One process tree, no console window: a wedged-cell kill, shutdown and quit
    // also stop what the user's code started (multiprocessing workers, servers);
    // a kernel that exits by itself leaves them running (core/process/ProcessTree.h).
    process::own_tree(proc_);
    connect(proc_, &QProcess::readyReadStandardOutput, this, [this, spawned]() {
        if (proc_ == spawned)
            on_stdout();
    });
    connect(proc_, &QProcess::readyReadStandardError, this, [this, spawned]() {
        if (proc_ == spawned)
            on_stderr();
    });
    connect(proc_, &QProcess::errorOccurred, this, [this, spawned](QProcess::ProcessError err) {
        // finished() is not emitted when the interpreter never started.
        if (err != QProcess::FailedToStart || proc_ != spawned)
            return;
        on_process_gone(spawned, tr("Python could not be started: %1").arg(spawned->errorString()));
    });
    connect(proc_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, spawned](int code, QProcess::ExitStatus status) {
                // A kernel that reported itself lost (finish_run, kernel_lost) exits right after:
                // treat its tree as killed, so what the wedged cell started goes with it —
                // kill_tree() after the child ended reaches the leftovers on both platforms.
                // Ended by itself otherwise: what it started on purpose keeps running.
                if (proc_ == spawned && stopping_)
                    process::kill_tree(spawned);
                else if (status == QProcess::NormalExit && !spawned->property("fincept_nb_killed").toBool())
                    process::release_tree(spawned);
                if (proc_ != spawned)
                    return; // an old kernel we already let go of
                on_stderr(); // anything still buffered
                QString why;
                if (!stopping_) {
                    QString tail = QString::fromUtf8(err_tail_).trimmed();
                    if (tail.size() > 600)
                        tail = QStringLiteral("…") + tail.right(600);
                    why = status == QProcess::CrashExit ? tr("The kernel crashed.")
                                                        : tr("The kernel exited (code %1).").arg(code);
                    if (!tail.isEmpty())
                        why += QLatin1Char('\n') + tail;
                }
                on_process_gone(spawned, why);
            });

    set_state(State::Starting);
    LOG_INFO(kTag, QStringLiteral("Starting %1 kernel in %2").arg(env_, dir));
    proc_->start(python, {QStringLiteral("-u"), QStringLiteral("-B"), script, QStringLiteral("kernel"),
                          QStringLiteral("--cwd"), dir});
    return true;
}

void NotebookKernel::on_process_gone(QProcess* proc, const QString& reason) {
    const bool unexpected = !stopping_;
    if (proc_ == proc) {
        proc_ = nullptr;
        proc->deleteLater();
    }
    backstop_->stop();
    if (!run_id_.isEmpty()) {
        const QString msg = !reason_.isEmpty() ? reason_
                            : !reason.isEmpty() ? reason + QLatin1Char(' ') + tr("Its variables were cleared.")
                                                : tr("The kernel stopped; its variables were cleared.");
        fail_run(QStringLiteral("KernelRestarted"), msg);
    }
    if (unexpected && !reason.isEmpty())
        LOG_WARN(kTag, QStringLiteral("Kernel stopped: %1").arg(reason.left(300)));
    const QString shown = reason_.isEmpty() ? reason : reason_;
    stopping_ = false;
    version_.clear();
    emit variables_changed({}, 0);
    if (state_ != State::Unavailable)
        set_state(State::Off);
    emit stopped(shown, unexpected);
    reason_.clear();
}

void NotebookKernel::detach_process() {
    if (!proc_)
        return;
    QProcess* proc = proc_;
    proc_ = nullptr;
    proc->disconnect(this);
    if (proc->state() == QProcess::NotRunning) {
        proc->deleteLater();
        return;
    }
    proc->write(QByteArrayLiteral("{\"op\":\"shutdown\"}\n"));
    proc->closeWriteChannel();
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), proc, &QObject::deleteLater);
    QTimer::singleShot(kShutdownGraceMs, proc, [proc]() {
        if (proc->state() != QProcess::NotRunning)
            nb_kill_tree(proc);
    });
}

void NotebookKernel::shutdown() {
    backstop_->stop();
    const bool had = proc_ != nullptr;
    stopping_ = true;
    detach_process();
    if (!run_id_.isEmpty())
        fail_run(QStringLiteral("KeyboardInterrupt"), tr("The kernel was shut down; the cell was stopped."));
    stopping_ = false;
    version_.clear();
    out_buf_.clear();
    if (had) {
        emit variables_changed({}, 0);
        emit stopped(QString(), false);
    }
    if (state_ != State::Unavailable)
        set_state(State::Off);
}

void NotebookKernel::restart() {
    shutdown();
    start();
}

void NotebookKernel::send(const QJsonObject& command) {
    if (!proc_)
        return;
    QByteArray line = QJsonDocument(command).toJson(QJsonDocument::Compact);
    line += '\n';
    proc_->write(line);
}

bool NotebookKernel::execute(const QString& run_id, const QString& code, int timeout_s, QString* why) {
    if (!run_id_.isEmpty()) {
        if (why)
            *why = tr("Another cell is still running.");
        return false;
    }
    if (!alive() && !start(why))
        return false;
    run_id_ = run_id;
    run_clock_.start();
    send(QJsonObject{{QStringLiteral("op"), QStringLiteral("execute")},
                     {QStringLiteral("id"), run_id},
                     {QStringLiteral("code"), code},
                     {QStringLiteral("timeout"), std::max(0, timeout_s)}});
    if (timeout_s > 0)
        backstop_->start(timeout_s * 1000 + kDeadlineBackstopMs);
    else
        backstop_->stop();
    set_state(State::Busy);
    return true;
}

void NotebookKernel::interrupt() {
    if (!proc_ || run_id_.isEmpty())
        return;
    send(QJsonObject{{QStringLiteral("op"), QStringLiteral("interrupt")}});
    set_state(State::Interrupting);
    backstop_->start(kInterruptBackstopMs);
}

void NotebookKernel::request_variables() {
    if (alive())
        send(QJsonObject{{QStringLiteral("op"), QStringLiteral("variables")}});
}

void NotebookKernel::inspect(const QString& request_id, const QString& name) {
    if (alive())
        send(QJsonObject{{QStringLiteral("op"), QStringLiteral("inspect")},
                         {QStringLiteral("id"), request_id},
                         {QStringLiteral("name"), name}});
}

void NotebookKernel::complete(const QString& request_id, const QString& prefix) {
    if (alive())
        send(QJsonObject{{QStringLiteral("op"), QStringLiteral("complete")},
                         {QStringLiteral("id"), request_id},
                         {QStringLiteral("prefix"), prefix}});
}

void NotebookKernel::preview(const QString& request_id, const QString& name) {
    if (alive())
        send(QJsonObject{{QStringLiteral("op"), QStringLiteral("preview")},
                         {QStringLiteral("id"), request_id},
                         {QStringLiteral("name"), name}});
}

// ── Output ───────────────────────────────────────────────────────────────────

void NotebookKernel::on_stdout() {
    if (!proc_)
        return;
    out_buf_ += proc_->readAllStandardOutput();
    qsizetype start = 0;
    for (;;) {
        const qsizetype nl = out_buf_.indexOf('\n', start);
        if (nl < 0)
            break;
        handle_line(out_buf_.mid(start, nl - start));
        if (!proc_)
            return; // a handler stopped the kernel
        start = nl + 1;
    }
    out_buf_.remove(0, start);
}

void NotebookKernel::on_stderr() {
    if (!proc_)
        return;
    const QByteArray chunk = proc_->readAllStandardError();
    if (chunk.isEmpty())
        return;
    err_tail_ += chunk;
    if (err_tail_.size() > kStderrTailBytes)
        err_tail_ = err_tail_.right(kStderrTailBytes);
    // Written straight to file descriptor 2 (native code, a crashing interpreter):
    // it belongs to the running cell when there is one.
    if (!run_id_.isEmpty())
        emit exec_stream(run_id_, QString::fromUtf8(chunk), true);
}

void NotebookKernel::handle_line(const QByteArray& raw) {
    QByteArray line = raw;
    if (line.endsWith('\r'))
        line.chop(1);
    const qsizetype at = line.indexOf(kMarker);
    if (at != 0) {
        // Text a native library wrote to file descriptor 1, outside Python's sys.stdout.
        const QByteArray text = at < 0 ? line : line.left(at);
        if (!run_id_.isEmpty() && !text.isEmpty())
            emit exec_stream(run_id_, QString::fromUtf8(text) + (at < 0 ? QStringLiteral("\n") : QString()), false);
        if (at < 0)
            return;
    }
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(line.mid(at + kMarker.size()), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return;
    handle_event(doc.object());
}

void NotebookKernel::handle_event(const QJsonObject& ev) {
    const QString type = ev.value(QStringLiteral("type")).toString();
    const QString id = ev.value(QStringLiteral("id")).toString();
    const bool ours = !run_id_.isEmpty() && (id.isEmpty() || id == run_id_);

    if (type == QLatin1String("ready")) {
        version_ = ev.value(QStringLiteral("version")).toString();
        if (run_id_.isEmpty())
            set_state(State::Idle);
        emit ready(version_);
    } else if (type == QLatin1String("start")) {
        if (ours)
            emit exec_started(run_id_);
    } else if (type == QLatin1String("stream")) {
        if (ours)
            emit exec_stream(run_id_, ev.value(QStringLiteral("text")).toString(),
                             ev.value(QStringLiteral("name")).toString() == QLatin1String("stderr"));
    } else if (type == QLatin1String("result") || type == QLatin1String("display") || type == QLatin1String("error")) {
        if (ours)
            emit exec_event(run_id_, ev);
    } else if (type == QLatin1String("done")) {
        if (ours)
            finish_run(ev.value(QStringLiteral("status")).toString(),
                       static_cast<qint64>(ev.value(QStringLiteral("elapsed_ms")).toDouble()),
                       ev.value(QStringLiteral("truncated")).toBool(), ev.value(QStringLiteral("kernel_lost")).toBool());
    } else if (type == QLatin1String("variables")) {
        emit variables_changed(ev.value(QStringLiteral("items")).toArray(), ev.value(QStringLiteral("total")).toInt());
    } else if (type == QLatin1String("inspect") || type == QLatin1String("complete") || type == QLatin1String("preview")) {
        emit reply(ev);
    }
}

void NotebookKernel::finish_run(const QString& status, qint64 elapsed_ms, bool truncated, bool kernel_lost) {
    backstop_->stop();
    const QString id = run_id_;
    run_id_.clear();
    if (kernel_lost) {
        // The kernel exits right after this report; the next run starts a new one. stopping_ makes
        // its finished() handler kill what the wedged cell started (kill_tree) instead of releasing it.
        stopping_ = true;
        reason_ = tr("The cell did not stop; the kernel was restarted and its variables were cleared.");
    } else if (proc_) {
        set_state(State::Idle);
    }
    emit exec_finished(id, status.isEmpty() ? QStringLiteral("error") : status, elapsed_ms, truncated, kernel_lost);
}

void NotebookKernel::fail_run(const QString& ename, const QString& evalue) {
    if (run_id_.isEmpty())
        return;
    const QString id = run_id_;
    const qint64 elapsed = current_run_elapsed_ms();
    emit exec_event(id, QJsonObject{{QStringLiteral("type"), QStringLiteral("error")},
                                    {QStringLiteral("ename"), ename},
                                    {QStringLiteral("evalue"), evalue},
                                    {QStringLiteral("traceback"), QJsonArray()},
                                    {QStringLiteral("line"), 0}});
    run_id_.clear();
    backstop_->stop();
    emit exec_finished(id, ename == QLatin1String("KeyboardInterrupt") ? QStringLiteral("cancelled")
                                                                         : QStringLiteral("error"),
                       elapsed, false, true);
}

} // namespace fincept::services
