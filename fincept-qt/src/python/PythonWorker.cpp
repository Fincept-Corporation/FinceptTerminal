#include "python/PythonWorker.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "python/PythonSetupManager.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>

namespace fincept::python {

namespace {

/// Pack a 4-byte big-endian length prefix followed by the JSON payload bytes.
/// Mirrors `_daemon_write_frame` on the Python side.
QByteArray encode_frame(const QJsonObject& obj) {
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

} // namespace

PythonWorker& PythonWorker::instance() {
    static PythonWorker s;
    return s;
}

PythonWorker::PythonWorker() {
    ready_watchdog_.setSingleShot(true);
    ready_watchdog_.setInterval(kReadyTimeoutMs);
    connect(&ready_watchdog_, &QTimer::timeout, this, [this]() {
        if (ready_)
            return;
        LOG_WARN("PythonWorker", "Daemon handshake timed out — killing and restarting");
        if (proc_) {
            proc_->kill();
        }
    });

    sweep_timer_.setSingleShot(false);
    sweep_timer_.setInterval(kSweepIntervalMs);
    sweep_timer_.setTimerType(Qt::CoarseTimer);
    connect(&sweep_timer_, &QTimer::timeout, this, &PythonWorker::sweep_deadlines);

    connect(&PythonRunner::instance(), &PythonRunner::python_ready, this, [this]() { ensure_started(); });

    connect(&PythonSetupManager::instance(), &PythonSetupManager::setup_complete, this,
            [this](bool success, const QString& /*error*/) {
                if (success) {
                    LOG_INFO("PythonWorker",
                             "Python/Venv setup completed successfully. Resetting restart budget and starting daemon.");
                    restart_count_ = 0;
                    ensure_started();
                }
            });
}

PythonWorker::~PythonWorker() {
    stop();
}

void PythonWorker::submit(const QString& action, const QJsonObject& payload, Callback cb) {
    // QProcess and our in_flight_/queue_ containers are owned by this
    // worker's thread (main thread in practice). MCP tool handlers call
    // submit() from QtConcurrent worker threads — touching proc_->write()
    // from there triggers `QSocketNotifier: cannot be enabled or disabled
    // from another thread` and the daemon stalls. Marshal back if needed.
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(
            this, [this, action, payload, cb = std::move(cb)]() mutable { submit(action, payload, std::move(cb)); },
            Qt::QueuedConnection);
        return;
    }

    ensure_started();

    const int id = next_id_++;
    Pending p;
    p.action = action;
    p.payload = payload;
    p.cb = std::move(cb);
    // Deadline starts now, not at dispatch: a request stuck behind a daemon
    // that never comes up is just as invisible to the caller as one stuck
    // behind a wedged Yahoo fetch.
    p.deadline = QDeadlineTimer(kRequestTimeoutMs);

    if (!ready_ || !proc_ || proc_->state() != QProcess::Running) {
        // Queue until the daemon is ready (or restarted).
        queue_.append({id, std::move(p)});
        update_sweep_timer();
        return;
    }

    QJsonObject req;
    req["id"] = id;
    req["action"] = action;
    req["payload"] = payload;
    in_flight_.insert(id, std::move(p));
    update_sweep_timer();
    proc_->write(encode_frame(req));
}

void PythonWorker::stop() {
    shutting_down_ = true;
    sweep_timer_.stop();
    ready_watchdog_.stop();
    if (!proc_)
        return;
    if (proc_->state() == QProcess::Running) {
        QJsonObject req;
        req["id"] = 0;
        req["action"] = QStringLiteral("shutdown");
        req["payload"] = QJsonObject{};
        proc_->write(encode_frame(req));
        proc_->closeWriteChannel();
        proc_->waitForFinished(2'000); // destructor-only blocking call (P1 allows)
        if (proc_->state() != QProcess::NotRunning)
            proc_->kill();
    }
    fail_all_pending(QStringLiteral("worker shutting down"));
}

void PythonWorker::ensure_started() {
    if (proc_ && proc_->state() != QProcess::NotRunning)
        return;
    launch_process();
}

void PythonWorker::launch_process() {
    auto& runner = PythonRunner::instance();
    const QString scripts_dir = runner.scripts_dir();
    const QString script_path = scripts_dir + "/yfinance_data.py";
    if (!QFileInfo::exists(script_path)) {
        LOG_WARN("PythonWorker", "yfinance_data.py not found — worker disabled");
        return;
    }

    // Always use venv-numpy2 — yfinance lives there. Fall back to the
    // PythonRunner's resolved interpreter if the venv isn't present.
    QString python_exe = PythonSetupManager::instance().python_path(QStringLiteral("venv-numpy2"));
    if (!QFileInfo::exists(python_exe))
        python_exe = runner.python_path();
    if (python_exe.isEmpty()) {
        LOG_WARN("PythonWorker", "No Python interpreter resolved — worker disabled");
        return;
    }

    if (proc_) {
        proc_->deleteLater();
        proc_ = nullptr;
    }
    proc_ = new QProcess(this);
    proc_->setProcessEnvironment(runner.build_python_env());
    proc_->setWorkingDirectory(scripts_dir);
    proc_->setReadChannel(QProcess::StandardOutput);

#ifdef _WIN32
    proc_->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
        cpa->flags |= 0x08000000; // CREATE_NO_WINDOW
    });
#endif

    connect(proc_, &QProcess::readyReadStandardOutput, this, &PythonWorker::on_ready_read);
    connect(proc_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &PythonWorker::on_process_finished);
    connect(proc_, &QProcess::errorOccurred, this, &PythonWorker::on_process_error);
    // Drain stderr to the log so Python import/runtime errors surface.
    connect(proc_, &QProcess::readyReadStandardError, this, [this]() {
        if (!proc_)
            return;
        const QByteArray err = proc_->readAllStandardError();
        if (!err.isEmpty()) {
            LOG_WARN("PythonWorker", QString("stderr: %1").arg(QString::fromUtf8(err).trimmed()));
        }
    });

    read_buf_.clear();
    ready_ = false;
    LOG_INFO("PythonWorker", QString("Launching daemon: %1 %2 --daemon").arg(python_exe).arg(script_path));
    proc_->start(python_exe, {script_path, QStringLiteral("--daemon")});
    ready_watchdog_.start();
}

void PythonWorker::on_process_finished(int exit_code, QProcess::ExitStatus status) {
    ready_ = false;
    ready_watchdog_.stop();
    const QString reason = QString("daemon exited (code=%1 status=%2)").arg(exit_code).arg(status);
    LOG_INFO("PythonWorker", reason);

    // Take the in-flight map out of the object *before* running any callback.
    // Two reasons, both load-bearing:
    //  1. exactly-once — once swapped out, nothing else in this class can see
    //     these ids, so no other path can complete them a second time;
    //  2. reentrancy — a callback is free to call submit() (a retry) and
    //     inserting into a container we are iterating is undefined behaviour.
    QHash<int, Pending> taken;
    taken.swap(in_flight_);

    // Queued requests are normally preserved across a restart so callers don't
    // observe spurious failures; only a spent restart budget retires them.
    QVector<QPair<int, Pending>> taken_queue;

    if (!shutting_down_ && !QCoreApplication::closingDown()) {
        if (restart_count_ >= kMaxRestarts) {
            LOG_ERROR("PythonWorker",
                      QString("Restart cap (%1) reached — giving up, pending requests will fail").arg(kMaxRestarts));
            taken_queue.swap(queue_);
        } else {
            ++restart_count_;
            LOG_INFO("PythonWorker",
                     QString("Restarting daemon (attempt %1/%2)").arg(restart_count_).arg(kMaxRestarts));
            // Relaunch before the callbacks run: a callback that re-submits
            // then finds a live-but-not-ready process and queues, rather than
            // racing ensure_started() into launching a second daemon.
            launch_process();
        }
    }

    update_sweep_timer();

    for (auto it = taken.begin(); it != taken.end(); ++it) {
        if (it.value().cb)
            it.value().cb(false, {}, reason);
    }
    for (auto& entry : taken_queue) {
        if (entry.second.cb)
            entry.second.cb(false, {}, QStringLiteral("worker restart cap reached"));
    }
}

void PythonWorker::sweep_deadlines() {
    // Collect + erase first, invoke last — same invariant as
    // on_process_finished(): an entry we are about to fail must already be
    // out of in_flight_/queue_ so a late response frame for the same id
    // cannot complete it a second time (try_drain_frames() looks the id up
    // and drops it as unknown), and so a re-entrant submit() from a callback
    // cannot invalidate the iterators we hold here.
    QVector<Pending> expired;
    bool in_flight_expired = false;

    for (auto it = in_flight_.begin(); it != in_flight_.end();) {
        if (it.value().deadline.hasExpired()) {
            LOG_WARN("PythonWorker", QString("Request id=%1 action=%2 exceeded %3 ms budget — failing")
                                         .arg(it.key())
                                         .arg(it.value().action)
                                         .arg(kRequestTimeoutMs));
            expired.append(std::move(it.value()));
            it = in_flight_.erase(it);
            in_flight_expired = true;
        } else {
            ++it;
        }
    }

    // Queued requests expire too. Without this, a worker that never starts
    // (script missing, no interpreter — launch_process() returns early and
    // never arms ready_watchdog_) leaves its callers with no callback at all.
    for (int i = queue_.size() - 1; i >= 0; --i) {
        if (queue_[i].second.deadline.hasExpired()) {
            LOG_WARN("PythonWorker", QString("Queued request id=%1 action=%2 expired before dispatch")
                                         .arg(queue_[i].first)
                                         .arg(queue_[i].second.action));
            expired.append(std::move(queue_[i].second));
            queue_.removeAt(i);
        }
    }

    if (in_flight_expired && proc_ && proc_->state() != QProcess::NotRunning) {
        // The daemon dispatch loop is serial, so whatever wedged the expired
        // request still owns the pipe and everything behind it is stuck too.
        // Killing is the only recovery; on_process_finished() restarts it and
        // fails the remaining (not-yet-expired) in-flight requests. kill() is
        // asynchronous, so `finished` arrives via the event loop — it cannot
        // re-enter this function.
        LOG_WARN("PythonWorker", "Killing wedged daemon after request timeout");
        proc_->kill();
    }

    update_sweep_timer();

    for (Pending& p : expired) {
        if (p.cb)
            p.cb(false, {}, QStringLiteral("daemon timeout"));
    }
}

void PythonWorker::update_sweep_timer() {
    const bool want = !shutting_down_ && (!in_flight_.isEmpty() || !queue_.isEmpty());
    if (want && !sweep_timer_.isActive())
        sweep_timer_.start();
    else if (!want && sweep_timer_.isActive())
        sweep_timer_.stop();
}

void PythonWorker::on_process_error(QProcess::ProcessError err) {
    LOG_WARN("PythonWorker", QString("QProcess error: %1 (%2)").arg(err).arg(proc_ ? proc_->errorString() : QString()));
}

void PythonWorker::on_ready_read() {
    if (!proc_)
        return;
    read_buf_.append(proc_->readAllStandardOutput());
    try_drain_frames();
    // Every early-return path in try_drain_frames() returns here, so this is
    // the single place the sweep timer needs re-evaluating after a drain.
    update_sweep_timer();
}

void PythonWorker::try_drain_frames() {
    while (read_buf_.size() >= 4) {
        const quint8 b0 = static_cast<quint8>(read_buf_.at(0));
        const quint8 b1 = static_cast<quint8>(read_buf_.at(1));
        const quint8 b2 = static_cast<quint8>(read_buf_.at(2));
        const quint8 b3 = static_cast<quint8>(read_buf_.at(3));
        const quint32 n = (static_cast<quint32>(b0) << 24) | (static_cast<quint32>(b1) << 16) |
                          (static_cast<quint32>(b2) << 8) | static_cast<quint32>(b3);
        if (n > 64u * 1024u * 1024u) {
            LOG_ERROR("PythonWorker", QString("Frame size %1 exceeds 64MB cap — resetting stream").arg(n));
            read_buf_.clear();
            if (proc_)
                proc_->kill();
            return;
        }
        if (static_cast<quint32>(read_buf_.size()) < 4 + n) {
            return; // wait for more bytes
        }
        const QByteArray body = read_buf_.mid(4, static_cast<int>(n));
        read_buf_.remove(0, 4 + static_cast<int>(n));

        QJsonParseError pe;
        const QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
        if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
            LOG_WARN("PythonWorker", QString("Bad JSON frame: %1 (%2 bytes)").arg(pe.errorString()).arg(body.size()));
            continue;
        }
        const QJsonObject obj = doc.object();

        // Ready handshake
        if (!ready_ && obj.value("ready").toBool()) {
            ready_ = true;
            ready_watchdog_.stop();
            restart_count_ = 0; // clean handshake resets the retry budget
            LOG_INFO("PythonWorker", QString("Daemon ready (pid=%1)").arg(obj.value("pid").toInt()));
            dispatch_queued();
            continue;
        }

        // Response. An id that is no longer in flight was already completed —
        // most often by sweep_deadlines() failing it as timed out and this
        // frame arriving late. Dropping it here is what keeps the callback
        // exactly-once; ids are never reused, so this can't be a mismatch.
        const int id = obj.value("id").toInt();
        auto it = in_flight_.find(id);
        if (it == in_flight_.end()) {
            LOG_DEBUG("PythonWorker", QString("Unknown/already-completed response id=%1 — ignoring").arg(id));
            continue;
        }
        // Take the entry out before invoking: the callback may re-enter
        // submit(), which inserts into in_flight_.
        Pending p = std::move(it.value());
        in_flight_.erase(it);
        const bool ok = obj.value("ok").toBool();
        const QString err = obj.value("error").toString();
        QJsonObject result_obj;
        // Result can be any JSON type; wrap non-object results in a container
        // under "_value" so callers always receive an object.
        const QJsonValue rv = obj.value("result");
        if (rv.isObject()) {
            result_obj = rv.toObject();
        } else {
            result_obj["_value"] = rv;
        }
        if (p.cb)
            p.cb(ok, result_obj, err);
    }
}

void PythonWorker::dispatch_queued() {
    if (!ready_ || !proc_)
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
        in_flight_.insert(id, std::move(p));
        proc_->write(encode_frame(req));
    }
}

void PythonWorker::fail_all_pending(const QString& reason) {
    // Take both containers out before invoking anything — see the swap
    // comment in on_process_finished(). This is what makes the callback
    // exactly-once even when a callback re-enters submit() or stop().
    auto taken_queue = std::move(queue_);
    queue_.clear();
    QHash<int, Pending> taken;
    taken.swap(in_flight_);
    update_sweep_timer();

    for (auto& entry : taken_queue) {
        if (entry.second.cb)
            entry.second.cb(false, {}, reason);
    }
    for (auto it = taken.begin(); it != taken.end(); ++it) {
        if (it.value().cb)
            it.value().cb(false, {}, reason);
    }
}

} // namespace fincept::python
