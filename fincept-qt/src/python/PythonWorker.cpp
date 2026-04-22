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
        if (ready_) return;
        LOG_WARN("PythonWorker", "Daemon handshake timed out — killing and restarting");
        if (proc_) {
            proc_->kill();
        }
    });
}

PythonWorker::~PythonWorker() {
    stop();
}

void PythonWorker::submit(const QString& action, const QJsonObject& payload, Callback cb) {
    ensure_started();

    const int id = next_id_++;
    Pending p;
    p.action = action;
    p.payload = payload;
    p.cb = std::move(cb);

    if (!ready_ || !proc_ || proc_->state() != QProcess::Running) {
        // Queue until the daemon is ready (or restarted).
        queue_.append({id, std::move(p)});
        return;
    }

    QJsonObject req;
    req["id"] = id;
    req["action"] = action;
    req["payload"] = payload;
    in_flight_.insert(id, std::move(p));
    proc_->write(encode_frame(req));
}

void PythonWorker::stop() {
    shutting_down_ = true;
    if (!proc_)
        return;
    if (proc_->state() == QProcess::Running) {
        QJsonObject req;
        req["id"] = 0;
        req["action"] = QStringLiteral("shutdown");
        req["payload"] = QJsonObject{};
        proc_->write(encode_frame(req));
        proc_->closeWriteChannel();
        proc_->waitForFinished(2'000);  // destructor-only blocking call (P1 allows)
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
    QString python_exe =
        PythonSetupManager::instance().python_path(QStringLiteral("venv-numpy2"));
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
        cpa->flags |= 0x08000000;  // CREATE_NO_WINDOW
    });
#endif

    connect(proc_, &QProcess::readyReadStandardOutput, this, &PythonWorker::on_ready_read);
    connect(proc_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &PythonWorker::on_process_finished);
    connect(proc_, &QProcess::errorOccurred, this, &PythonWorker::on_process_error);
    // Drain stderr to the log so Python import/runtime errors surface.
    connect(proc_, &QProcess::readyReadStandardError, this, [this]() {
        if (!proc_) return;
        const QByteArray err = proc_->readAllStandardError();
        if (!err.isEmpty()) {
            LOG_WARN("PythonWorker", QString("stderr: %1").arg(QString::fromUtf8(err).trimmed()));
        }
    });

    read_buf_.clear();
    ready_ = false;
    LOG_INFO("PythonWorker", QString("Launching daemon: %1 %2 --daemon")
                                  .arg(python_exe).arg(script_path));
    proc_->start(python_exe, {script_path, QStringLiteral("--daemon")});
    ready_watchdog_.start();
}

void PythonWorker::on_process_finished(int exit_code, QProcess::ExitStatus status) {
    ready_ = false;
    ready_watchdog_.stop();
    const QString reason = QString("daemon exited (code=%1 status=%2)").arg(exit_code).arg(status);
    LOG_INFO("PythonWorker", reason);

    // Fail any in-flight requests; queued requests are preserved for the
    // next restart so callers don't observe spurious failures.
    for (auto it = in_flight_.begin(); it != in_flight_.end(); ++it) {
        if (it.value().cb) it.value().cb(false, {}, reason);
    }
    in_flight_.clear();

    if (shutting_down_ || QCoreApplication::closingDown()) {
        return;
    }

    if (restart_count_ >= kMaxRestarts) {
        LOG_ERROR("PythonWorker",
                  QString("Restart cap (%1) reached — giving up, pending requests will fail")
                      .arg(kMaxRestarts));
        fail_all_pending(QStringLiteral("worker restart cap reached"));
        return;
    }
    ++restart_count_;
    LOG_INFO("PythonWorker", QString("Restarting daemon (attempt %1/%2)")
                                  .arg(restart_count_).arg(kMaxRestarts));
    launch_process();
}

void PythonWorker::on_process_error(QProcess::ProcessError err) {
    LOG_WARN("PythonWorker", QString("QProcess error: %1 (%2)")
                                  .arg(err)
                                  .arg(proc_ ? proc_->errorString() : QString()));
}

void PythonWorker::on_ready_read() {
    if (!proc_) return;
    read_buf_.append(proc_->readAllStandardOutput());
    try_drain_frames();
}

void PythonWorker::try_drain_frames() {
    while (read_buf_.size() >= 4) {
        const quint8 b0 = static_cast<quint8>(read_buf_.at(0));
        const quint8 b1 = static_cast<quint8>(read_buf_.at(1));
        const quint8 b2 = static_cast<quint8>(read_buf_.at(2));
        const quint8 b3 = static_cast<quint8>(read_buf_.at(3));
        const quint32 n =
            (static_cast<quint32>(b0) << 24) | (static_cast<quint32>(b1) << 16) |
            (static_cast<quint32>(b2) << 8)  |  static_cast<quint32>(b3);
        if (n > 64u * 1024u * 1024u) {
            LOG_ERROR("PythonWorker", QString("Frame size %1 exceeds 64MB cap — resetting stream").arg(n));
            read_buf_.clear();
            if (proc_) proc_->kill();
            return;
        }
        if (static_cast<quint32>(read_buf_.size()) < 4 + n) {
            return;  // wait for more bytes
        }
        const QByteArray body = read_buf_.mid(4, static_cast<int>(n));
        read_buf_.remove(0, 4 + static_cast<int>(n));

        QJsonParseError pe;
        const QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
        if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
            LOG_WARN("PythonWorker",
                     QString("Bad JSON frame: %1 (%2 bytes)").arg(pe.errorString()).arg(body.size()));
            continue;
        }
        const QJsonObject obj = doc.object();

        // Ready handshake
        if (!ready_ && obj.value("ready").toBool()) {
            ready_ = true;
            ready_watchdog_.stop();
            restart_count_ = 0;  // clean handshake resets the retry budget
            LOG_INFO("PythonWorker", QString("Daemon ready (pid=%1)")
                                          .arg(obj.value("pid").toInt()));
            dispatch_queued();
            continue;
        }

        // Response
        const int id = obj.value("id").toInt();
        auto it = in_flight_.find(id);
        if (it == in_flight_.end()) {
            LOG_DEBUG("PythonWorker", QString("Unknown response id=%1 — ignoring").arg(id));
            continue;
        }
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
        if (p.cb) p.cb(ok, result_obj, err);
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
    for (auto& entry : queue_) {
        if (entry.second.cb) entry.second.cb(false, {}, reason);
    }
    queue_.clear();
    for (auto it = in_flight_.begin(); it != in_flight_.end(); ++it) {
        if (it.value().cb) it.value().cb(false, {}, reason);
    }
    in_flight_.clear();
}

} // namespace fincept::python
