#include "trading/ExchangeDaemonPool.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QThread>
#include <QTimer>

namespace fincept::trading {

namespace {
const QString kPoolTag = "ExchangeDaemonPool";

QString pool_resolve_script_path(const QString& relative) {
    const QString dir = fincept::python::PythonRunner::instance().scripts_dir();
    if (dir.isEmpty())
        return {};
    return dir + "/" + relative;
}
} // namespace

ExchangeDaemonPool& ExchangeDaemonPool::instance() {
    static ExchangeDaemonPool s;
    return s;
}

ExchangeDaemonPool::ExchangeDaemonPool() {
    // Pre-warm on a deferred tick so PythonRunner has finished first-run init
    // before we try to resolve its python/scripts paths.
    QTimer::singleShot(0, this, [this]() { start(); });
}

ExchangeDaemonPool::~ExchangeDaemonPool() {
    stop();
}

void ExchangeDaemonPool::start() {
    if (process_ && process_->state() == QProcess::Running)
        return;

    const QString python_path = python::PythonRunner::instance().python_path();
    const QString script_path = pool_resolve_script_path("exchange/exchange_daemon.py");
    if (python_path.isEmpty() || script_path.isEmpty()) {
        LOG_WARN(kPoolTag,"Cannot start daemon: python or exchange_daemon.py not found");
        return;
    }

    process_ = new QProcess(this);
    process_->setProcessChannelMode(QProcess::SeparateChannels);

    connect(process_, &QProcess::readyReadStandardOutput, this, &ExchangeDaemonPool::drain_buffer);
    connect(process_, &QProcess::readyReadStandardError, this, [this]() {
        if (!process_)
            return;
        const QString err = QString::fromUtf8(process_->readAllStandardError()).trimmed();
        if (!err.isEmpty())
            LOG_DEBUG(kPoolTag, "Daemon stderr: " + err);
    });
    connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
                LOG_WARN(kPoolTag,QString("Daemon exited (code=%1, status=%2)").arg(code).arg(status));
                ready_ = false;
                if (process_) {
                    process_->deleteLater();
                    process_ = nullptr;
                }
                // Creds must be re-sent after restart because the daemon
                // has forgotten them with the interpreter.
                QMutexLocker lock(&mutex_);
                creds_sent_.clear();
            });

    QStringList args;
    args << "-u" << "-B" << script_path;
    process_->start(python_path, args);
    LOG_INFO(kPoolTag,"Exchange daemon starting...");
}

void ExchangeDaemonPool::stop() {
    if (!process_)
        return;
    ready_ = false;
    process_->closeWriteChannel();
    auto* proc = process_;
    process_ = nullptr;
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), proc, &QObject::deleteLater);
    QTimer::singleShot(2000, proc, [proc]() {
        if (proc->state() != QProcess::NotRunning)
            proc->kill();
    });
}

bool ExchangeDaemonPool::wait_for_ready(int timeout_ms) {
    if (ready_.load())
        return true;

    // Kick start from the owner thread if nothing is running yet.
    if (!process_) {
        if (QThread::currentThread() == thread()) {
            start();
        } else {
            QMetaObject::invokeMethod(this, [this]() { start(); }, Qt::QueuedConnection);
        }
    }

    QMutexLocker lock(&mutex_);
    QElapsedTimer elapsed;
    elapsed.start();
    while (!ready_.load() && elapsed.elapsed() < timeout_ms) {
        const int remaining = timeout_ms - static_cast<int>(elapsed.elapsed());
        response_ready_.wait(&mutex_, std::max(remaining, 50));
    }
    return ready_.load();
}

QString ExchangeDaemonPool::credential_fingerprint(const ExchangeCredentials& creds) {
    if (creds.api_key.isEmpty() && creds.secret.isEmpty() && creds.password.isEmpty())
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(creds.api_key.toUtf8());
    hash.addData("|");
    hash.addData(creds.secret.toUtf8());
    hash.addData("|");
    hash.addData(creds.password.toUtf8());
    return QString::fromLatin1(hash.result().toHex().left(16));
}

void ExchangeDaemonPool::forget_credentials(const QString& exchange) {
    QMutexLocker lock(&mutex_);
    creds_sent_.remove(exchange);
}

void ExchangeDaemonPool::send_credentials_if_needed(const QString& exchange, const ExchangeCredentials& creds) {
    // Caller must hold no locks. Runs on the owner thread only.
    const QString fp = credential_fingerprint(creds);
    if (fp.isEmpty())
        return;
    {
        QMutexLocker lock(&mutex_);
        if (creds_sent_.value(exchange) == fp)
            return;
        creds_sent_[exchange] = fp;
    }
    if (!process_)
        return;
    QJsonObject c;
    c["api_key"] = creds.api_key;
    c["secret"] = creds.secret;
    if (!creds.password.isEmpty())
        c["password"] = creds.password;
    QJsonObject req;
    req["id"] = "__creds_" + exchange + "_" + fp;
    req["method"] = "set_credentials";
    req["exchange"] = exchange;
    req["args"] = c;
    process_->write(QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n");
}

void ExchangeDaemonPool::drain_buffer() {
    if (!process_)
        return;
    while (process_->canReadLine()) {
        const QString line = QString::fromUtf8(process_->readLine()).trimmed();
        if (line.isEmpty() || line[0] != '{')
            continue;
        QJsonParseError err;
        const auto doc = QJsonDocument::fromJson(line.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError)
            continue;
        const auto obj = doc.object();
        const QString id = obj.value("id").toString();

        if (id == "__init__") {
            ready_ = true;
            LOG_INFO(kPoolTag,"Exchange daemon ready (pid=" +
                              obj.value("data").toObject().value("pid").toVariant().toString() + ")");
            response_ready_.wakeAll();
            emit ready();
            continue;
        }

        // Ignore credential-ack replies — they carry no caller.
        if (id.startsWith("__creds_"))
            continue;

        {
            QMutexLocker lock(&mutex_);
            responses_[id] = obj;
        }
        response_ready_.wakeAll();
    }
}

QJsonObject ExchangeDaemonPool::call(const QString& exchange,
                                     const QString& method,
                                     const QJsonObject& args,
                                     const ExchangeCredentials& credentials,
                                     int timeout_ms) {
    if (!ready_.load()) {
        if (!wait_for_ready(std::min(timeout_ms, 8000))) {
            LOG_WARN(kPoolTag,QString("Daemon not ready for %1/%2").arg(exchange, method));
            return {{"error", "Daemon not ready"}};
        }
    }

    const QString req_id = QString("r_%1").arg(next_req_id_.fetch_add(1));
    QJsonObject req;
    req["id"] = req_id;
    req["method"] = method;
    req["exchange"] = exchange;
    req["args"] = args;
    const QByteArray payload = QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";

    // Marshal the write + any credential send to the owner thread. Returns
    // immediately — the worker then blocks on the response wait below.
    QMetaObject::invokeMethod(
        this,
        [this, exchange, credentials, payload]() {
            if (!process_)
                return;
            send_credentials_if_needed(exchange, credentials);
            process_->write(payload);
        },
        Qt::QueuedConnection);

    QMutexLocker lock(&mutex_);
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < timeout_ms) {
        auto it = responses_.find(req_id);
        if (it != responses_.end()) {
            const QJsonObject resp = it.value();
            responses_.erase(it);
            lock.unlock();
            // Unwrap {"success":true,"data":{...}} when the data is an object.
            if (resp.value("success").toBool(false) && resp.contains("data")) {
                const auto d = resp.value("data");
                if (d.isObject())
                    return d.toObject();
            }
            return resp;
        }
        response_ready_.wait(&mutex_, 50);
    }
    lock.unlock();
    LOG_WARN(kPoolTag,QString("Daemon call timed out: %1/%2").arg(exchange, method));
    return {{"error", QString("Daemon call timed out: %1/%2").arg(exchange, method)}};
}

} // namespace fincept::trading
