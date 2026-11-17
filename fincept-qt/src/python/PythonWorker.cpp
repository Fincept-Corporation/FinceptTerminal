#include "python/PythonWorker.h"

#include "core/logging/Logger.h"
#include "core/process/ProcessTree.h"
#include "core/privacy/LogRedact.h"
#include "python/PythonRunner.h"
#include "python/PythonSetupManager.h"

#include <QByteArray>
#include <QChar>
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMetaObject>
#include <QMutexLocker>
#include <QThread>

#include <utility>

namespace fincept::python {

namespace {

/// A length prefix above this is a corrupt stream, not a reply.
constexpr quint32 pw_kMaxFrameBytes = 64u * 1024u * 1024u;
/// Lines of daemon stderr kept to explain its death.
constexpr int pw_kStderrTailLines = 60;
/// A stderr burst longer than this is logged in several entries.
constexpr int pw_kStderrLogBlockLines = 200;
/// An unterminated stderr line (progress-bar redraws use '\r') is cut here.
constexpr qsizetype pw_kStderrMaxLineBytes = 16 * 1024;
/// Quiet time that ends a stderr burst (one traceback → one log entry).
constexpr int pw_kStderrBurstMs = 150;

/// Pack a 4-byte big-endian length prefix followed by the JSON payload bytes.
/// Mirrors `_daemon_write_frame` on the Python side.
QByteArray pw_encode_frame(const QJsonObject& obj) {
    const QByteArray body = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    const quint32 n = static_cast<quint32>(body.size());
    QByteArray out;
    out.reserve(4 + body.size());
    out.append(static_cast<char>((n >> 24) & 0xff));
    out.append(static_cast<char>((n >> 16) & 0xff));
    out.append(static_cast<char>((n >> 8) & 0xff));
    out.append(static_cast<char>(n & 0xff));
    out.append(body);
    return out;
}

/// One line, short enough that our prefix plus the cause survives callers that
/// clip error text to 200 characters (MarketDataService does).
QString pw_clip_cause(const QString& s) {
    constexpr int kMax = 150;
    const QString t = s.simplified();
    return t.size() <= kMax ? t : t.left(kMax - 1) + QChar(0x2026);
}

bool pw_is_indented(const QString& line) {
    return !line.isEmpty() && line.at(0).isSpace();
}

} // namespace

// ── Reply decoding ───────────────────────────────────────────────────────────
// Lives on PythonWorker's decode thread. Owns the current daemon's stdout byte
// stream: splits it into frames, parses each JSON body, runs a request's
// Prepare step, and posts the decoded frames to the owner thread in arrival
// order. It never touches the QProcess or any owner-thread state; it only
// exists while the decode thread runs, and the owner joins that thread before
// it is destroyed, so posting to the owner from here is always safe.

class PythonWorkerDecoder : public QObject {
  public:
    PythonWorkerDecoder(PythonWorker* sink, std::shared_ptr<PythonWorker::PrepareRegistry> prepares)
        : sink_(sink), prepares_(std::move(prepares)) {}

    void feed(quint64 gen, QByteArray chunk) {
        if (gen != gen_) { // a new daemon: whatever the old one left is gone
            gen_ = gen;
            buf_.clear();
            poisoned_ = false;
        }
        if (poisoned_)
            return; // past a corrupt length prefix nothing lines up again
        if (buf_.isEmpty())
            buf_ = std::move(chunk);
        else
            buf_.append(chunk);

        QVector<PythonWorker::DecodedFrame> out;
        qsizetype off = 0;
        while (buf_.size() - off >= 4) {
            const auto* b = reinterpret_cast<const uchar*>(buf_.constData() + off);
            const quint32 n = (static_cast<quint32>(b[0]) << 24) | (static_cast<quint32>(b[1]) << 16) |
                              (static_cast<quint32>(b[2]) << 8) | static_cast<quint32>(b[3]);
            if (n > pw_kMaxFrameBytes) {
                PythonWorker::DecodedFrame f;
                f.corrupt = true;
                f.corrupt_size = n;
                out.append(std::move(f));
                buf_.clear();
                off = 0;
                poisoned_ = true;
                break;
            }
            if (static_cast<quint64>(buf_.size() - off) < 4ull + n)
                break; // wait for more bytes
            decode(buf_.mid(off + 4, static_cast<qsizetype>(n)), out);
            off += 4 + static_cast<qsizetype>(n);
        }
        if (off > 0)
            buf_.remove(0, off);
        if (!out.isEmpty())
            deliver(gen, std::move(out));
    }

    /// Free the buffer of a stream the owner has dropped.
    void reset(quint64 gen) {
        if (gen != gen_)
            return;
        buf_.clear();
        buf_.squeeze();
    }

    /// Run `then` on the owner thread after every frame fed so far.
    void barrier(std::function<void()> then) {
        QMetaObject::invokeMethod(sink_, std::move(then), Qt::QueuedConnection);
    }

  private:
    void decode(const QByteArray& body, QVector<PythonWorker::DecodedFrame>& out) {
        QJsonParseError pe;
        const QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
        if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
            LOG_WARN("PythonWorker", QString("Bad JSON frame: %1 (%2 bytes)").arg(pe.errorString()).arg(body.size()));
            return;
        }
        const QJsonObject obj = doc.object();

        PythonWorker::DecodedFrame f;
        f.ready = obj.value("ready").toBool();
        f.pid = obj.value("pid").toInt();
        f.id = obj.value("id").toInt();
        f.ok = obj.value("ok").toBool();
        f.error = obj.value("error").toString();
        // Result can be any JSON type; wrap non-object results in a container
        // under "_value" so callers always receive an object.
        const QJsonValue rv = obj.value("result");
        if (rv.isObject())
            f.result = rv.toObject();
        else
            f.result["_value"] = rv;

        // submit_prepared(): the caller's heavy step runs here, not on the owner.
        PythonWorker::Prepare prepare;
        {
            QMutexLocker lock(&prepares_->mutex);
            prepare = prepares_->by_id.value(f.id);
        }
        if (prepare) {
            f.prepared = prepare(f.ok, f.result, f.error);
            // Release our copy while the continuation still holds whatever the
            // step captured: the caller's callback is never destroyed here.
            prepare = nullptr;
            f.result = QJsonObject(); // consumed — freed on this thread
        }
        out.append(std::move(f));
    }

    void deliver(quint64 gen, QVector<PythonWorker::DecodedFrame> frames) {
        PythonWorker* const sink = sink_;
        QMetaObject::invokeMethod(
            sink, [sink, gen, frames = std::move(frames)]() { sink->on_frames_decoded(gen, frames); },
            Qt::QueuedConnection);
    }

    PythonWorker* const sink_;
    const std::shared_ptr<PythonWorker::PrepareRegistry> prepares_;
    QByteArray buf_;
    quint64 gen_ = 0;
    bool poisoned_ = false;
};

PythonWorker& PythonWorker::instance() {
    static PythonWorker s;
    return s;
}

const char* PythonWorker::state_name(State s) {
    switch (s) {
        case State::Idle:
            return "idle";
        case State::Unavailable:
            return "unavailable";
        case State::Starting:
            return "starting";
        case State::Ready:
            return "ready";
        case State::BackingOff:
            return "backing off";
        case State::Capped:
            return "capped";
        case State::Stopped:
            return "stopped";
    }
    return "?";
}

PythonWorker::PythonWorker() {
    // Every timer is a member: destroyed with this object, so none can fire
    // into a destroyed worker. stop() stops them all.
    ready_watchdog_.setSingleShot(true);
    ready_watchdog_.setInterval(kReadyTimeoutMs);
    connect(&ready_watchdog_, &QTimer::timeout, this, [this]() {
        // NotRunning: it already exited and its death is being handled (after
        // the decode thread delivers its last frames) — not a handshake timeout.
        if (state_ != State::Starting || !proc_ || proc_->state() == QProcess::NotRunning)
            return;
        kill_reason_ = tr("no handshake within %1 s").arg(kReadyTimeoutMs / 1000);
        // A cold first start while setup still precompiles bytecode is slow, not
        // broken; handle_daemon_death() does not charge the budget for it.
        if (PythonSetupManager::instance().is_running())
            kill_reason_ += tr(" (Python setup is still running)");
        LOG_WARN("PythonWorker",
                 QString("Daemon handshake timed out after %1 s — killing it").arg(kReadyTimeoutMs / 1000));
        process::kill_tree(proc_); // finished() arrives via the event loop → handle_daemon_death()
    });

    sweep_timer_.setSingleShot(false);
    sweep_timer_.setInterval(kSweepIntervalMs);
    sweep_timer_.setTimerType(Qt::CoarseTimer);
    connect(&sweep_timer_, &QTimer::timeout, this, &PythonWorker::sweep_deadlines);

    restart_timer_.setSingleShot(true);
    connect(&restart_timer_, &QTimer::timeout, this, [this]() {
        if (state_ != State::BackingOff)
            return;
        state_ = State::Idle;
        try_launch(); // Starting, or Unavailable (which fails anything still queued)
    });

    fail_timer_.setSingleShot(true);
    fail_timer_.setInterval(0);
    connect(&fail_timer_, &QTimer::timeout, this, &PythonWorker::flush_failures);

    stderr_log_timer_.setSingleShot(true);
    stderr_log_timer_.setInterval(pw_kStderrBurstMs);
    connect(&stderr_log_timer_, &QTimer::timeout, this, &PythonWorker::flush_stderr_log);

    // Recovery wiring. python_ready fires when PythonRunner (re-)resolves an
    // interpreter — notably on core_ready, when the managed venv becomes usable.
    connect(&PythonRunner::instance(), &PythonRunner::python_ready, this,
            [this]() { reset_restart_budget(QStringLiteral("Python interpreter ready")); });

    connect(&PythonSetupManager::instance(), &PythonSetupManager::setup_complete, this,
            [this](bool success, const QString& /*error*/) {
                if (success) {
                    LOG_INFO("PythonWorker",
                             "Python/Venv setup completed successfully. Resetting restart budget and starting daemon.");
                    reset_restart_budget(QStringLiteral("Python setup completed"));
                }
            });
}

PythonWorker::~PythonWorker() {
    stop();
}

void PythonWorker::submit(const QString& action, const QJsonObject& payload, Callback cb) {
    Pending p;
    p.action = action;
    p.payload = payload;
    p.cb = std::move(cb);
    enqueue(std::move(p));
}

void PythonWorker::submit_prepared(const QString& action, const QJsonObject& payload, Prepare prepare) {
    Pending p;
    p.action = action;
    p.payload = payload;
    p.prepare = std::move(prepare);
    enqueue(std::move(p));
}

void PythonWorker::enqueue(Pending p) {
    // QProcess and our in_flight_/queue_ containers are owned by this
    // worker's thread (main thread in practice). MCP tool handlers call
    // submit() from QtConcurrent worker threads — touching proc_->write()
    // from there triggers `QSocketNotifier: cannot be enabled or disabled
    // from another thread` and the daemon stalls. Marshal back if needed.
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, [this, p = std::move(p)]() mutable { enqueue(std::move(p)); },
                                  Qt::QueuedConnection);
        return;
    }

    const int id = next_id_++;
    p.id = id;
    // Deadline starts now, not at dispatch: a request stuck behind a daemon
    // that never comes up is just as invisible to the caller as one stuck
    // behind a wedged Yahoo fetch.
    p.deadline = QDeadlineTimer(kRequestTimeoutMs);
    // Registered before the request can reach the daemon, so the decode thread
    // finds the step for any reply to it.
    if (p.prepare) {
        QMutexLocker lock(&prepares_->mutex);
        prepares_->by_id.insert(id, p.prepare);
    }

    // 1. Give the daemon its chance to start. Every check here is cheap (two
    //    file-existence tests and cached setup flags), so an Unavailable worker
    //    notices a finished setup on the very next request.
    switch (state_) {
        case State::Idle:
        case State::Unavailable:
            try_launch();
            break;
        case State::Capped:
            if (capped_since_.isValid() && capped_since_.elapsed() >= kCappedProbeIntervalMs) {
                LOG_INFO("PythonWorker", QString("Restart cap cool-down (%1 s) elapsed — probing one daemon start")
                                             .arg(kCappedProbeIntervalMs / 1000));
                state_ = State::Idle;
                try_launch();
            }
            break;
        case State::Starting:
        case State::Ready:
        case State::BackingOff:
        case State::Stopped:
            break;
    }

    // 2. Write, queue, or fail on the next event-loop turn — never inline.
    switch (state_) {
        case State::Ready:
            if (proc_ && proc_->state() == QProcess::Running) {
                QJsonObject req;
                req["id"] = id;
                req["action"] = p.action;
                req["payload"] = p.payload;
                p.in_flight_since.start();
                in_flight_.insert(id, std::move(p));
                update_sweep_timer();
                proc_->write(pw_encode_frame(req));
                return;
            }
            // The process is gone and its finished() is not handled yet: hold
            // the request for the restart, like any other post-ready death.
            [[fallthrough]];
        case State::Starting:
            queue_.append({id, std::move(p)});
            update_sweep_timer();
            return;
        case State::BackingOff:
            if (!last_start_failed_) {
                // A daemon that had been serving died; the restart is probably
                // fine, so the request waits for it (bounded by its deadline).
                queue_.append({id, std::move(p)});
                update_sweep_timer();
                return;
            }
            // The last START failed; the next one likely fails the same way.
            fail_later(std::move(p), last_failure_);
            return;
        case State::Capped:
            fail_later(std::move(p), capped_reason_);
            return;
        case State::Unavailable:
            fail_later(std::move(p), unavailable_reason_);
            return;
        case State::Idle: // not reachable: try_launch() always leaves Idle
            fail_later(std::move(p), tr("Market data daemon is not running"));
            return;
        case State::Stopped:
            fail_later(std::move(p), QStringLiteral("worker shutting down"));
            return;
    }
}

void PythonWorker::retry() {
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, [this]() { retry(); }, Qt::QueuedConnection);
        return;
    }
    if (state_ == State::Stopped)
        return;
    status_probed_ = false; // re-read the setup status once
    reset_restart_budget(QStringLiteral("Explicit retry"));
}

void PythonWorker::stop() {
    if (state_ != State::Stopped) {
        state_ = State::Stopped;
        sweep_timer_.stop();
        ready_watchdog_.stop();
        restart_timer_.stop();
        if (proc_) {
            QProcess* const p = proc_;
            proc_ = nullptr;
            // From here on nothing this process emits (finished, errorOccurred,
            // readyRead) can reach the worker.
            p->disconnect(this);
            if (p->state() == QProcess::Running) {
                QJsonObject req;
                req["id"] = 0;
                req["action"] = QStringLiteral("shutdown");
                req["payload"] = QJsonObject{};
                p->write(pw_encode_frame(req));
                p->closeWriteChannel();
                p->waitForFinished(2'000); // aboutToQuit-only blocking call (P1 allows)
            }
            // The whole tree: whatever the daemon started dies with it, also when
            // it already left by itself after the shutdown request.
            process::kill_tree(p);
            if (p->state() != QProcess::NotRunning)
                p->waitForFinished(1'000);
            stderr_partial_.append(p->readAllStandardError());
            split_stderr_lines(/*final=*/true);
            p->deleteLater(); // also a child of this worker, so freed with it at the latest
        }
        drop_stream();
        flush_stderr_log();
        fail_queue_later(QStringLiteral("worker shutting down"));
        fail_in_flight_later(QStringLiteral("worker shutting down"));
    }
    // Unconditionally: handle_daemon_death() can enter Stopped by itself while
    // the application closes down, and the decode thread must be joined before
    // this object goes away either way.
    shutdown_decoder();
    // Synchronously: after aboutToQuit there may be no event-loop turn left to
    // run fail_timer_. Bounded loop — a callback may submit() again, which
    // parks another failure.
    for (int round = 0; round < 8 && !failed_.isEmpty(); ++round)
        flush_failures();
}

void PythonWorker::ensure_started() {
    if (state_ == State::Idle || state_ == State::Unavailable)
        try_launch();
}

void PythonWorker::reset_restart_budget(const QString& why) {
    if (state_ == State::Stopped)
        return;
    restart_count_ = 0;
    backoff_step_ = 0;
    if (state_ == State::Capped || state_ == State::BackingOff) {
        LOG_INFO("PythonWorker",
                 QString("%1 — restart budget reset (was %2)").arg(why, QLatin1String(state_name(state_))));
        restart_timer_.stop();
        state_ = State::Idle;
    }
    ensure_started();
}

PythonWorker::Blocker PythonWorker::launch_blocker(QString* python_exe, QString* script_path, QString* reason) {
    const QString script = PythonRunner::instance().scripts_dir() + QStringLiteral("/yfinance_data.py");
    if (!QFileInfo::exists(script)) {
        *reason = QStringLiteral("Script not found: ") + script; // PythonRunner's wording
        return Blocker::ScriptMissing;
    }

    // Only the app-managed analytics venv — the one PythonSetupManager installs
    // yfinance into — and only once it reports the essentials installed. No
    // fallback to PythonRunner's system python: it has no yfinance, so every
    // start would die with ModuleNotFoundError and burn the restart budget.
    auto& setup = PythonSetupManager::instance();
    const QString venv_python = setup.python_path(QStringLiteral("venv-numpy2"));
    const bool have_python = QFileInfo::exists(venv_python);
    bool core_ready = setup.is_core_ready();
    if (have_python && !core_ready && !setup.is_running() && !status_probed_) {
        // is_core_ready() is a cache that only check_status() and a setup run
        // fill. Read the real status once rather than refusing a usable venv
        // because nobody has asked yet.
        status_probed_ = true;
        core_ready = setup.check_status().core_ready;
    }
    if (!have_python || !core_ready) {
        *reason = PythonRunner::python_unavailable_message();
        return setup.is_running() ? Blocker::SetupRunning : Blocker::NotSetUp;
    }

    *python_exe = venv_python;
    *script_path = script;
    return Blocker::None;
}

bool PythonWorker::try_launch() {
    QString python_exe;
    QString script_path;
    QString reason;
    const Blocker blocker = launch_blocker(&python_exe, &script_path, &reason);
    if (blocker != Blocker::None) {
        enter_unavailable(blocker, reason);
        return false;
    }
    blocker_ = Blocker::None;
    unavailable_reason_.clear();
    launch_process(python_exe, script_path);
    return true;
}

void PythonWorker::enter_unavailable(Blocker blocker, const QString& reason) {
    // Log transitions only — this runs on every request while Python is absent.
    if (state_ != State::Unavailable || blocker != blocker_) {
        const QString line = QString("Daemon not started: %1").arg(reason);
        if (blocker == Blocker::ScriptMissing)
            LOG_WARN("PythonWorker", line);
        else
            LOG_INFO("PythonWorker", line);
    }
    blocker_ = blocker;
    unavailable_reason_ = reason;
    state_ = State::Unavailable;
    fail_queue_later(reason);
}

void PythonWorker::enter_capped(const QString& cause) {
    state_ = State::Capped;
    capped_reason_ = tr("Market data daemon gave up after %1 restarts: %2").arg(QString::number(kMaxRestarts), cause);
    capped_since_.start();
    LOG_ERROR("PythonWorker", QString("Restart cap (%1) reached — failing requests fast until Python setup completes, "
                                      "the interpreter is re-resolved or the user retries. Last error: %2")
                                  .arg(QString::number(kMaxRestarts), cause));
    fail_queue_later(capped_reason_);
}

void PythonWorker::launch_process(const QString& python_exe, const QString& script_path) {
    retire_process(); // normally already gone; never leave two daemons
    flush_stderr_log();

    auto& runner = PythonRunner::instance();
    proc_ = new QProcess(this);
    proc_->setProcessEnvironment(runner.build_python_env());
    proc_->setWorkingDirectory(runner.scripts_dir());
    proc_->setReadChannel(QProcess::StandardOutput);

    // The daemon and anything it starts form one tree (job object / process
    // group): every kill below takes the whole tree, and a daemon that dies by
    // itself has its leftovers reaped when the QProcess is deleted (Windows
    // kill-on-close). No console window.
    process::own_tree(proc_);

    connect(proc_, &QProcess::readyReadStandardOutput, this, &PythonWorker::on_ready_read);
    connect(proc_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &PythonWorker::on_process_finished);
    connect(proc_, &QProcess::errorOccurred, this, &PythonWorker::on_process_error);
    // stderr is buffered by line and logged per burst, and its tail explains a
    // death (see stderr_cause()).
    connect(proc_, &QProcess::readyReadStandardError, this, &PythonWorker::on_stderr_ready);

    ensure_decoder();
    ++stream_gen_; // a fresh stdout stream: the decoder starts with an empty buffer
    stderr_partial_.clear();
    stderr_tail_.clear();
    kill_reason_.clear();
    state_ = State::Starting;
    launch_clock_.start();
    LOG_DEBUG("PythonWorker", QString("Python daemon launching interpreter=%1 script=%2")
                                  .arg(privacy::redact_home(python_exe), QFileInfo(script_path).fileName()));

    QProcess* const p = proc_;
    p->start(python_exe, {script_path, QStringLiteral("--daemon")});
    // start() may report FailedToStart synchronously (Windows), which has
    // already run handle_daemon_death() and retired `p`.
    if (proc_ == p && state_ == State::Starting)
        ready_watchdog_.start();
}

void PythonWorker::retire_process() {
    if (!proc_)
        return;
    QProcess* const p = proc_;
    proc_ = nullptr;
    p->disconnect(this);
    process::kill_tree(p);
    // deleteLater, not delete: we may be inside one of p's own signals.
    p->deleteLater();
    drop_stream();
}

void PythonWorker::on_process_finished(int exit_code, QProcess::ExitStatus status) {
    if (!proc_)
        return;
    // Anything still buffered: a last response frame (the traceback is read
    // in finish_process_death()).
    if (proc_->bytesAvailable() > 0)
        feed_decoder(proc_->readAllStandardOutput());
    // Frames already read are being decoded off this thread. A reply the
    // daemon finished writing before it died still completes, so the death is
    // handled only once the decoder has delivered all of them: the barrier
    // queues behind them on the decode thread and comes back after them.
    const quint64 gen = stream_gen_;
    std::function<void()> then = [this, gen, exit_code, status]() { finish_process_death(gen, exit_code, status); };
    if (!decoder_) {
        then();
        return;
    }
    PythonWorkerDecoder* const d = decoder_;
    QMetaObject::invokeMethod(
        d, [d, then = std::move(then)]() mutable { d->barrier(std::move(then)); }, Qt::QueuedConnection);
}

void PythonWorker::finish_process_death(quint64 gen, int exit_code, QProcess::ExitStatus status) {
    // A reply callback stopped the worker, or the daemon was retired, meanwhile.
    if (gen != stream_gen_ || !proc_)
        return;
    stderr_partial_.append(proc_->readAllStandardError());
    split_stderr_lines(/*final=*/true);

    const bool was_ready = state_ == State::Ready;
    QString cause = kill_reason_;
    if (cause.isEmpty())
        cause = stderr_cause(/*allow_last_line=*/!was_ready);
    if (cause.isEmpty()) {
        cause = status == QProcess::CrashExit ? tr("crashed (exit code %1)").arg(exit_code)
                                              : tr("exit code %1").arg(exit_code);
    }
    LOG_DEBUG("PythonWorker",
             QString("Python daemon exited code=%1 status=%2 uptime_ms=%3")
                 .arg(exit_code)
                 .arg(status == QProcess::CrashExit ? QStringLiteral("crash") : QStringLiteral("normal"))
                 .arg(launch_clock_.isValid() ? launch_clock_.elapsed() : -1));
    handle_daemon_death(cause);
}

void PythonWorker::on_process_error(QProcess::ProcessError err) {
    if (!proc_)
        return;
    const QString what = proc_->errorString();
    LOG_WARN("PythonWorker", QString("Python daemon process error code=%1 error=\"%2\" interpreter=%3")
                                 .arg(int(err))
                                 .arg(what, privacy::redact_home(proc_->program())));
    // A process that never started emits no finished(): this is its only
    // death notice. Every other error is followed by finished() (or is benign).
    if (err == QProcess::FailedToStart)
        handle_daemon_death(tr("could not run the Python interpreter: %1").arg(pw_clip_cause(what)));
}

void PythonWorker::handle_daemon_death(const QString& cause) {
    const bool was_ready = state_ == State::Ready;
    ready_watchdog_.stop();
    flush_stderr_log(); // stderr lines at DEBUG; the tail goes into the verdict below
    // The last few non-empty stderr lines (policy: never forwarded line by line
    // at INFO+; attached once to the exit line instead).
    QStringList last_stderr;
    for (int i = stderr_tail_.size() - 1; i >= 0 && last_stderr.size() < 5; --i) {
        const QString l = stderr_tail_.at(i).trimmed();
        if (!l.isEmpty())
            last_stderr.prepend(privacy::redact_text(l.left(200)));
    }
    retire_process();
    kill_reason_.clear();

    const QString reason = was_ready ? tr("Market data daemon exited: %1").arg(cause)
                                     : tr("Market data daemon failed to start: %1").arg(cause);
    LOG_WARN("PythonWorker", QString("Python daemon %1 cause=\"%2\" last_stderr=\"%3\"")
                                 .arg(was_ready ? QStringLiteral("exited") : QStringLiteral("failed to start"),
                                      privacy::redact_text(cause).left(200),
                                      last_stderr.join(QStringLiteral(" | "))));

    // Whatever the daemon held is lost with it.
    fail_in_flight_later(reason);

    if (state_ == State::Stopped || QCoreApplication::closingDown()) {
        state_ = State::Stopped;
        fail_queue_later(QStringLiteral("worker shutting down"));
        update_sweep_timer();
        return;
    }

    // Budget: failures while Python setup is running (packages landing,
    // bytecode still compiling) say nothing about whether the daemon can work.
    ++backoff_step_;
    const bool setup_running = PythonSetupManager::instance().is_running();
    if (!setup_running) {
        if (restart_count_ >= kMaxRestarts) {
            enter_capped(cause);
            return;
        }
        ++restart_count_;
    }

    if (!was_ready) {
        // Every queued request was waiting for this start, and the next one is
        // likely to fail the same way: tell them now, not at the 90 s deadline.
        fail_queue_later(reason);
    }
    last_start_failed_ = !was_ready;
    last_failure_ = reason;

    const int shift = qMin(backoff_step_ - 1, 5);
    const int delay_ms = qMin(kBackoffBaseMs << shift, kBackoffMaxMs);
    state_ = State::BackingOff;
    restart_timer_.start(delay_ms);
    if (setup_running) {
        LOG_INFO("PythonWorker", QString("Restarting daemon in %1 ms (Python setup is running — not counted against "
                                         "the restart budget)")
                                     .arg(delay_ms));
    } else {
        LOG_INFO(
            "PythonWorker",
            QString("Restarting daemon in %1 ms (attempt %2/%3)").arg(delay_ms).arg(restart_count_).arg(kMaxRestarts));
    }
    update_sweep_timer();
}

void PythonWorker::sweep_deadlines() {
    // Collect + erase first, complete via failed_ — an entry we are about to
    // fail must already be out of in_flight_/queue_ so a late response frame
    // for the same id cannot complete it a second time (on_frames_decoded()
    // looks the id up and drops it as unknown).
    QVector<Pending> expired;
    bool wedged = false;

    for (auto it = in_flight_.begin(); it != in_flight_.end();) {
        if (it.value().deadline.hasExpired()) {
            const qint64 held_ms = it.value().in_flight_since.isValid() ? it.value().in_flight_since.elapsed() : 0;
            LOG_WARN("PythonWorker", QString("Request id=%1 action=%2 exceeded %3 ms budget (%4 ms at the daemon) — "
                                             "failing")
                                         .arg(it.key())
                                         .arg(it.value().action)
                                         .arg(kRequestTimeoutMs)
                                         .arg(held_ms));
            // Only a request the daemon itself sat on for long proves it wedged.
            // One that spent its budget waiting for a slow cold start does not,
            // and killing a healthy daemon would just repeat that cold start.
            if (held_ms >= kWedgeMinInFlightMs)
                wedged = true;
            expired.append(std::move(it.value()));
            it = in_flight_.erase(it);
        } else {
            ++it;
        }
    }

    // Queued requests expire too (a daemon stuck in Starting, or a restart
    // that has not come back).
    for (int i = queue_.size() - 1; i >= 0; --i) {
        if (queue_[i].second.deadline.hasExpired()) {
            LOG_WARN("PythonWorker", QString("Queued request id=%1 action=%2 expired before dispatch (daemon %3)")
                                         .arg(queue_[i].first)
                                         .arg(queue_[i].second.action)
                                         .arg(QLatin1String(state_name(state_))));
            expired.append(std::move(queue_[i].second));
            queue_.removeAt(i);
        }
    }

    if (wedged && proc_ && proc_->state() != QProcess::NotRunning) {
        // The daemon dispatch loop is serial, so whatever wedged the expired
        // request still owns the pipe and everything behind it is stuck too.
        // Killing is the only recovery; handle_daemon_death() restarts it and
        // fails the remaining in-flight requests. kill() is asynchronous, so
        // `finished` arrives via the event loop — it cannot re-enter here.
        kill_reason_ = tr("restarted after a request hung for over %1 s").arg(kWedgeMinInFlightMs / 1000);
        LOG_WARN("PythonWorker", "Killing wedged daemon after request timeout");
        process::kill_tree(proc_);
    }

    const QString timeout_msg = tr("Market data request timed out after %1 s").arg(kRequestTimeoutMs / 1000);
    for (Pending& p : expired)
        fail_later(std::move(p), timeout_msg);
    update_sweep_timer();
}

void PythonWorker::update_sweep_timer() {
    const bool want = state_ != State::Stopped && (!in_flight_.isEmpty() || !queue_.isEmpty());
    if (want && !sweep_timer_.isActive())
        sweep_timer_.start();
    else if (!want && sweep_timer_.isActive())
        sweep_timer_.stop();
}

void PythonWorker::fail_later(Pending p, const QString& reason) {
    failed_.append(Failed{std::move(p), reason});
    if (!fail_timer_.isActive())
        fail_timer_.start();
}

void PythonWorker::fail_queue_later(const QString& reason) {
    if (queue_.isEmpty())
        return;
    auto taken = std::move(queue_);
    queue_.clear();
    for (auto& entry : taken)
        fail_later(std::move(entry.second), reason);
    update_sweep_timer();
}

void PythonWorker::fail_in_flight_later(const QString& reason) {
    if (in_flight_.isEmpty())
        return;
    QHash<int, Pending> taken;
    taken.swap(in_flight_);
    for (auto it = taken.begin(); it != taken.end(); ++it)
        fail_later(std::move(it.value()), reason);
    update_sweep_timer();
}

void PythonWorker::flush_failures() {
    fail_timer_.stop();
    // Take the list out before invoking anything: a callback is free to
    // submit() again (a retry), which may park a new failure — that one runs
    // on the next turn, never in this loop.
    QVector<Failed> taken;
    taken.swap(failed_);
    for (Failed& f : taken)
        complete(f.p, false, {}, f.reason);
}

void PythonWorker::forget_prepare(const Pending& p) {
    if (!p.prepare)
        return;
    QMutexLocker lock(&prepares_->mutex);
    prepares_->by_id.remove(p.id);
}

void PythonWorker::complete(Pending& p, bool ok, const QJsonObject& result, const QString& error) {
    forget_prepare(p);
    if (p.cb) {
        p.cb(ok, result, error);
    } else if (p.prepare) {
        // No reply to prepare off-thread (a failure): cheap, so run it here.
        const std::function<void()> then = p.prepare(ok, result, error);
        if (then)
            then();
    }
}

// ── reply decoding ───────────────────────────────────────────────────────────

void PythonWorker::ensure_decoder() {
    if (decode_thread_)
        return;
    decode_thread_ = new QThread(this); // joined and deleted in shutdown_decoder()
    decode_thread_->setObjectName(QStringLiteral("PythonWorkerDecode"));
    decoder_ = new PythonWorkerDecoder(this, prepares_);
    decoder_->moveToThread(decode_thread_);
    decode_thread_->start();
}

void PythonWorker::shutdown_decoder() {
    if (!decode_thread_)
        return;
    decode_thread_->quit();
    decode_thread_->wait(); // aboutToQuit / destructor only; bounded by one frame's parse
    // The thread has finished, so nothing can be running in the decoder.
    delete decoder_;
    decoder_ = nullptr;
    delete decode_thread_;
    decode_thread_ = nullptr;
}

void PythonWorker::feed_decoder(QByteArray chunk) {
    if (chunk.isEmpty() || !decoder_)
        return;
    const quint64 gen = stream_gen_;
    PythonWorkerDecoder* const d = decoder_;
    QMetaObject::invokeMethod(
        d, [d, gen, chunk = std::move(chunk)]() mutable { d->feed(gen, std::move(chunk)); }, Qt::QueuedConnection);
}

void PythonWorker::drop_stream() {
    const quint64 old = stream_gen_++;
    if (decoder_) {
        PythonWorkerDecoder* const d = decoder_;
        QMetaObject::invokeMethod(d, [d, old]() { d->reset(old); }, Qt::QueuedConnection);
    }
}

void PythonWorker::on_ready_read() {
    if (!proc_)
        return;
    // Only a copy of what QProcess has buffered happens on this thread; framing
    // and the JSON parse run on the decode thread (on_frames_decoded()).
    feed_decoder(proc_->readAllStandardOutput());
}

void PythonWorker::on_frames_decoded(quint64 gen, const QVector<DecodedFrame>& frames) {
    for (const DecodedFrame& f : frames) {
        // The daemon was retired or the worker stopped — possibly by a callback
        // earlier in this very batch. Nothing more of that stream counts.
        if (gen != stream_gen_)
            break;

        if (f.corrupt) {
            LOG_ERROR("PythonWorker", QString("Frame size %1 exceeds 64MB cap — resetting stream").arg(f.corrupt_size));
            if (proc_) {
                kill_reason_ = tr("sent a corrupt frame (%1 bytes)").arg(f.corrupt_size);
                process::kill_tree(proc_);
            }
            break; // the decoder discarded the rest of the stream
        }

        // Ready handshake
        if (state_ == State::Starting && f.ready) {
            state_ = State::Ready;
            ready_watchdog_.stop();
            // A clean handshake ends the failure streak.
            restart_count_ = 0;
            backoff_step_ = 0;
            last_start_failed_ = false;
            last_failure_.clear();
            LOG_INFO("PythonWorker", QString("Python daemon ready pid=%1 startup_ms=%2")
                                         .arg(f.pid)
                                         .arg(launch_clock_.isValid() ? launch_clock_.elapsed() : -1));
            dispatch_queued();
            continue;
        }

        // Response. An id that is no longer in flight was already completed —
        // most often by sweep_deadlines() failing it as timed out and this
        // frame arriving late. Dropping it here is what keeps the callback
        // exactly-once; ids are never reused, so this can't be a mismatch.
        auto it = in_flight_.find(f.id);
        if (it == in_flight_.end()) {
            LOG_DEBUG("PythonWorker", QString("Unknown/already-completed response id=%1 — ignoring").arg(f.id));
            continue;
        }
        // Take the entry out before invoking: the callback may re-enter
        // submit(), which inserts into in_flight_.
        Pending p = std::move(it.value());
        in_flight_.erase(it);
        if (f.prepared) {
            forget_prepare(p);
            f.prepared();
        } else {
            complete(p, f.ok, f.result, f.error);
        }
    }
    // The single place the sweep timer needs re-evaluating after replies.
    update_sweep_timer();
}

void PythonWorker::dispatch_queued() {
    if (state_ != State::Ready || !proc_)
        return;
    auto local = std::move(queue_);
    queue_.clear();
    for (auto& entry : local) {
        const int id = entry.first;
        Pending& p = entry.second;
        QJsonObject req;
        req["id"] = id;
        req["action"] = p.action;
        req["payload"] = p.payload;
        p.in_flight_since.start();
        in_flight_.insert(id, std::move(p));
        proc_->write(pw_encode_frame(req));
    }
    update_sweep_timer();
}

// ── stderr ───────────────────────────────────────────────────────────────────
// Read by line, not by chunk: with PYTHONUNBUFFERED a traceback arrives in
// many small writes, and logging each chunk shredded it into dozens of WARN
// lines. Lines are collected and a short quiet period ends the burst, so one
// traceback becomes one multi-line log entry.

void PythonWorker::on_stderr_ready() {
    if (!proc_)
        return;
    stderr_partial_.append(proc_->readAllStandardError());
    split_stderr_lines(/*final=*/false);
    if (!stderr_log_block_.isEmpty())
        stderr_log_timer_.start(); // (re)arm — the burst ends after a quiet period
}

void PythonWorker::split_stderr_lines(bool final) {
    for (;;) {
        const qsizetype nl = stderr_partial_.indexOf('\n');
        if (nl < 0)
            break;
        QByteArray line = stderr_partial_.left(nl);
        stderr_partial_.remove(0, nl + 1);
        add_stderr_line(std::move(line));
    }
    if (final || stderr_partial_.size() > pw_kStderrMaxLineBytes) {
        if (!stderr_partial_.isEmpty())
            add_stderr_line(stderr_partial_);
        stderr_partial_.clear();
    }
}

void PythonWorker::add_stderr_line(QByteArray line) {
    if (line.endsWith('\r'))
        line.chop(1);
    // Progress-bar redraws ("\r 40%… \r 80%…"): keep only the final state.
    const qsizetype cr = line.lastIndexOf('\r');
    if (cr >= 0)
        line = line.mid(cr + 1);
    const QString text = QString::fromUtf8(line);

    stderr_tail_.append(text);
    while (stderr_tail_.size() > pw_kStderrTailLines)
        stderr_tail_.removeFirst();

    stderr_log_block_.append(text);
    if (stderr_log_block_.size() >= pw_kStderrLogBlockLines)
        flush_stderr_log();
}

void PythonWorker::flush_stderr_log() {
    stderr_log_timer_.stop();
    if (stderr_log_block_.isEmpty())
        return;
    QStringList block;
    block.swap(stderr_log_block_);
    while (!block.isEmpty() && block.constLast().trimmed().isEmpty())
        block.removeLast();
    while (!block.isEmpty() && block.constFirst().trimmed().isEmpty())
        block.removeFirst();
    if (block.isEmpty())
        return;
    // DEBUG only: a script's stderr is diagnostics, kept as the tail for the exit line.
    // ONE line per block (the lines joined by " | ", home folders / URLs / tokens scrubbed,
    // capped) — never a multi-line entry.
    if (!Logger::instance().should_log(LogLevel::Debug, "PythonWorker"))
        return;
    for (QString& l : block)
        l = l.trimmed();
    block.removeAll(QString());
    const QString joined = privacy::redact_text(block.join(QStringLiteral(" | ")));
    LOG_DEBUG("PythonWorker", logf("Python daemon stderr", {{"lines", block.size()}, {"text", joined.left(1000)}}));
}

QString PythonWorker::stderr_cause(bool allow_last_line) const {
    // An uncaught exception prints "Traceback (most recent call last):",
    // indented frame lines, then the unindented "Type: message" line. With
    // chained exceptions the LAST Traceback block holds the one that escaped.
    for (int i = stderr_tail_.size() - 1; i >= 0; --i) {
        if (!stderr_tail_.at(i).startsWith(QLatin1String("Traceback (most recent call last)")))
            continue;
        for (int j = i + 1; j < stderr_tail_.size(); ++j) {
            const QString& l = stderr_tail_.at(j);
            if (!l.trimmed().isEmpty() && !pw_is_indented(l))
                return pw_clip_cause(l);
        }
        break;
    }
    for (int i = stderr_tail_.size() - 1; i >= 0; --i) {
        if (stderr_tail_.at(i).startsWith(QLatin1String("Fatal Python error")))
            return pw_clip_cause(stderr_tail_.at(i));
    }
    // During startup everything on stderr is about startup, so the last line
    // is a fair cause. After the handshake it may be an unrelated yfinance
    // warning from an earlier request — not used then.
    if (allow_last_line) {
        for (int i = stderr_tail_.size() - 1; i >= 0; --i) {
            if (!stderr_tail_.at(i).trimmed().isEmpty())
                return pw_clip_cause(stderr_tail_.at(i));
        }
    }
    return {};
}

} // namespace fincept::python
