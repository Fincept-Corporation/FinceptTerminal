#include "trading/ExchangeDaemonPool.h"

#include "core/logging/Logger.h"
#include "core/process/ProcessTree.h"
#include "python/PythonRunner.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QThread>
#include <QTimer>

#include <algorithm>

namespace fincept::trading {

namespace {
const QString kPoolTag = "ExchangeDaemonPool";

/// Marks a `responses_` slot whose caller timed out and went away. Never
/// produced by the daemon, so it can't collide with a real reply. See the
/// timeout path in call() and the consume in drain_buffer().
constexpr auto kPoolAbandonedMarker = "__abandoned__";

/// `creds_sent_` value meaning "the daemon may hold credentials for this exchange
/// but we no longer know which". Never equal to a real fingerprint (16 hex chars),
/// so the next call() always re-sends — or, when the session's credentials are now
/// empty, tells the daemon to DROP what it holds. See send_credentials_if_needed().
const QString kCredsUnknown = QStringLiteral("?");

// Restart policy (see ExchangeDaemonPool::start() in the header).
constexpr int kDaemonRestartBaseMs = 2000;    // back-off after the first death; doubles each time
constexpr int kDaemonRestartCapMs = 30000;    // back-off ceiling
constexpr int kDaemonMaxQuickFailures = 6;    // consecutive deaths without a healthy run → give up
constexpr qint64 kDaemonHealthyRunMs = 60000; // a ready daemon up this long resets the count
/// Set on a daemon stop() is ending on purpose: its exit is not a failure.
constexpr char kDaemonStoppedProp[] = "_fincept_daemon_stopped";

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
    // The QProcess and every queued write live on this object's thread, so it
    // must be one with an event loop. The first instance() call can come from a
    // QtConcurrent worker (the workflow crypto node creates sessions there); a
    // pool left on that thread never ran start() or a single queued write.
    if (auto* app = QCoreApplication::instance(); app && thread() != app->thread())
        moveToThread(app->thread());
    // Pre-warm on a deferred tick so PythonRunner has finished first-run init
    // before we try to resolve its python/scripts paths.
    QTimer::singleShot(0, this, [this]() { start_when_allowed(); });
}

ExchangeDaemonPool::~ExchangeDaemonPool() {
    stop();
}

void ExchangeDaemonPool::start() {
    // Explicit start: re-arms a pool that gave up and skips a pending back-off.
    if (gave_up_.exchange(false))
        LOG_INFO(kPoolTag, "Exchange daemon start requested — restart limit reset");
    quick_failures_ = 0;
    backoff_ms_ = 0;
    launch();
}

void ExchangeDaemonPool::start_when_allowed() {
    if (process_ && process_->state() != QProcess::NotRunning)
        return;
    if (gave_up_.load())
        return; // call() reports why; only an explicit start() tries again
    if (backoff_ms_ > 0 && since_failure_.isValid()) {
        const qint64 left = backoff_ms_ - since_failure_.elapsed();
        if (left > 0) {
            // Too soon after the last death: start once the back-off has passed
            // (one pending timer, however many callers are waiting).
            if (!start_scheduled_) {
                start_scheduled_ = true;
                QTimer::singleShot(static_cast<int>(left), this, [this]() {
                    start_scheduled_ = false;
                    start_when_allowed();
                });
            }
            return;
        }
    }
    launch();
}

void ExchangeDaemonPool::note_daemon_lost(const QString& why) {
    // A daemon that became ready and stayed up starts a new count; one that keeps
    // dying at start (missing ccxt, broken interpreter) keeps adding to it.
    const bool healthy_run = ready_seen_ && since_spawn_.isValid() && since_spawn_.elapsed() >= kDaemonHealthyRunMs;
    quick_failures_ = healthy_run ? 1 : quick_failures_ + 1;
    since_failure_.start();
    QString reason = why;
    if (!last_stderr_.isEmpty())
        reason += QStringLiteral(" — ") + last_stderr_;
    if (quick_failures_ >= kDaemonMaxQuickFailures) {
        backoff_ms_ = 0;
        {
            QMutexLocker lock(&mutex_);
            last_failure_ = reason;
            gave_up_ = true;
            response_ready_.wakeAll(); // waiters in wait_for_ready() answer now, not at their timeout
        }
        LOG_TRANSITION(kPoolTag, QStringLiteral("daemon"), 100, LogLevel::Error,
                       logf("Exchange daemon keeps failing at start; it is not restarted any more",
                            {{"failures", quick_failures_}, {"reason", log_scrub(reason)}}));
        return;
    }
    backoff_ms_ = std::min(kDaemonRestartBaseMs << (quick_failures_ - 1), kDaemonRestartCapMs);
    // State = the failure count, so each back-off step is one line (at most five).
    LOG_TRANSITION(kPoolTag, QStringLiteral("daemon"), quick_failures_, LogLevel::Warn,
                   logf("Exchange daemon down; next start after back-off",
                        {{"failures", quick_failures_}, {"retry_in_ms", backoff_ms_}, {"reason", log_scrub(reason)}}));
}

QString ExchangeDaemonPool::unavailable_text() {
    QString reason;
    {
        QMutexLocker lock(&mutex_);
        reason = last_failure_;
    }
    return tr("The exchange service stopped after repeated failed starts (%1). Nothing was sent. Check Settings > "
              "Python Environment, then restart the terminal.")
        .arg(log_scrub(reason).left(300));
}

void ExchangeDaemonPool::launch() {
    // Starting counts as "already launched" too: only Running used to be checked,
    // so a second start() during the spawn window created a second daemon and
    // orphaned the first (it kept running, unowned, until app exit).
    if (process_ && process_->state() != QProcess::NotRunning)
        return;

    const QString python_path = python::PythonRunner::instance().python_path();
    const QString script_path = pool_resolve_script_path("exchange/exchange_daemon.py");
    if (python_path.isEmpty() || script_path.isEmpty()) {
        // Every call() retries the start: one line until it can start. Python not set
        // up yet is an expected state (INFO); a missing script is a broken install.
        LOG_TRANSITION(kPoolTag, QStringLiteral("daemon_files"), 1,
                       script_path.isEmpty() ? LogLevel::Warn : LogLevel::Info,
                       logf("Exchange daemon cannot start", {{"python_found", !python_path.isEmpty()},
                                                             {"script_found", !script_path.isEmpty()},
                                                             {"script", "exchange/exchange_daemon.py"}}));
        return;
    }
    if (Logger::instance().transition_possible(0))
        Logger::instance().transition(kPoolTag, QStringLiteral("daemon_files"), 0); // found again: re-arm the line

    process_ = new QProcess(this);
    process_->setProcessChannelMode(QProcess::SeparateChannels);
    // The daemon and anything it starts are one unit: stop() kills the whole tree,
    // a crashed daemon's leftovers are reaped when its QProcess is deleted, and on
    // Windows the OS kills the tree if the terminal itself dies (kill-on-close job).
    // Also hides the console window. Must precede start().
    process::own_tree(process_);
    // Identify THIS spawn in its handlers: stop() detaches process_ before the old
    // daemon has actually exited, so a restart can leave an old process's signals
    // arriving while process_ already names its replacement.
    QProcess* const spawned = process_;
    ready_seen_ = false;
    last_stderr_.clear();
    since_spawn_.start();

    connect(process_, &QProcess::readyReadStandardOutput, this, &ExchangeDaemonPool::drain_buffer);
    connect(process_, &QProcess::readyReadStandardError, this, [this]() {
        if (!process_)
            return;
        const QString err = QString::fromUtf8(process_->readAllStandardError()).trimmed();
        if (!err.isEmpty()) {
            LOG_DEBUG(kPoolTag, "Daemon stderr: " + err);
            // Kept as the reason if this daemon dies at start ("No module named 'ccxt'").
            last_stderr_ = err.section(QLatin1Char('\n'), -1).trimmed().left(300);
        }
    });
    // finished() is NOT emitted when the interpreter never started at all (bad
    // path, blocked by AV, missing runtime DLL) — only errorOccurred(FailedToStart)
    // fires. Without this, process_ stayed non-null forever, so wait_for_ready()
    // never re-kicked start() and every call() burned its full ready-wait.
    connect(process_, &QProcess::errorOccurred, this, [this, spawned](QProcess::ProcessError err) {
        if (err != QProcess::FailedToStart)
            return; // Crashed et al. are followed by finished(); the rest are transient I/O
        // The reason goes into note_daemon_lost()'s back-off line.
        LOG_DEBUG(kPoolTag, logf("Exchange daemon failed to start", {{"error", spawned->errorString()}}));
        if (process_ == spawned) {
            ready_ = false;
            process_ = nullptr; // next call()/wait_for_ready() retries start() after the back-off
            if (!spawned->property(kDaemonStoppedProp).toBool())
                note_daemon_lost(tr("Python could not be started: %1").arg(spawned->errorString()));
        }
        spawned->deleteLater();
    });
    connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, spawned](int code, QProcess::ExitStatus status) {
                // One line per exit at DEBUG: a death is reported once per back-off
                // step by note_daemon_lost(), with the reason.
                LOG_DEBUG(kPoolTag, logf("Exchange daemon exited",
                                         {{"code", code},
                                          {"status", status == QProcess::CrashExit ? "crash" : "normal"},
                                          {"uptime_ms", since_spawn_.isValid() ? since_spawn_.elapsed() : -1}}));
                // A newer daemon has already replaced this one (stop() then start()):
                // its ready flag, creds and pending replies are not ours to reset.
                if (process_ && process_ != spawned)
                    return;
                ready_ = false;
                if (process_) {
                    process_->deleteLater();
                    process_ = nullptr;
                }
                // A daemon that dies at start prints its reason and exits at once;
                // finished() can arrive before that stderr was read.
                const QString err_tail = QString::fromUtf8(spawned->readAllStandardError()).trimmed();
                if (!err_tail.isEmpty())
                    last_stderr_ = err_tail.section(QLatin1Char('\n'), -1).trimmed().left(300);
                // stop() ended it on purpose; anything else is a death the restart
                // back-off counts (the next start waits, or the pool gives up).
                if (!spawned->property(kDaemonStoppedProp).toBool())
                    note_daemon_lost(status == QProcess::CrashExit ? tr("the exchange service crashed")
                                                                   : tr("the exchange service exited (code %1)").arg(code));
                // Creds must be re-sent after restart because the daemon
                // has forgotten them with the interpreter.
                QMutexLocker lock(&mutex_);
                creds_sent_.clear();
                // Anything still parked here is unclaimable: the ids belong to
                // the dead interpreter's request stream and no reply is coming.
                // Waiters aren't stranded — they're on a bounded wait and fall
                // through to their own timeout.
                responses_.clear();
            });

    // The shared Python environment, as every other Python spawn gets it
    // (ExchangeSession::start_ws does the same for ws_stream.py). Without it the
    // daemon ran with no PYTHONIOENCODING: on Windows its stdout pipe used the
    // ANSI code page, a reply with a CJK market symbol (Binance / MEXC / Bitget
    // search) could not be encoded and was never written, and the caller waited
    // out its 15 s. It also lacked FINCEPT_DATA_DIR, so it kept its markets cache
    // apart from ws_stream's. start() always runs on this object's thread (the
    // main thread, see the constructor); build_python_env() reads SecureStorage,
    // which is safe from any thread.
    process_->setProcessEnvironment(python::PythonRunner::instance().build_python_env());

    QStringList args;
    args << "-u" << "-B" << script_path;
    process_->start(python_path, args);
    LOG_DEBUG(kPoolTag, logf("Exchange daemon starting", {{"attempt", quick_failures_ + 1}}));
}

void ExchangeDaemonPool::stop() {
    if (!process_)
        return;
    ready_ = false;
    process_->setProperty(kDaemonStoppedProp, true); // its exit is not a failure
    process_->closeWriteChannel();
    auto* proc = process_;
    process_ = nullptr;
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), proc, &QObject::deleteLater);
    QTimer::singleShot(2000, proc, [proc]() {
        if (proc->state() != QProcess::NotRunning)
            process::kill_tree(proc); // the daemon and every process it started
    });
}

bool ExchangeDaemonPool::wait_for_ready(int timeout_ms) {
    if (ready_.load())
        return true;
    if (gave_up_.load())
        return false; // not restarted any more: call() says why

    // Kick start from the owner thread if nothing is running yet (after the
    // restart back-off when the last daemon died).
    if (!process_) {
        if (QThread::currentThread() == thread()) {
            start_when_allowed();
        } else {
            QMetaObject::invokeMethod(this, [this]() { start_when_allowed(); }, Qt::QueuedConnection);
        }
    }

    QMutexLocker lock(&mutex_);
    QElapsedTimer elapsed;
    elapsed.start();
    while (!ready_.load() && !gave_up_.load() && elapsed.elapsed() < timeout_ms) {
        const int remaining = timeout_ms - static_cast<int>(elapsed.elapsed());
        response_ready_.wait(&mutex_, std::max(remaining, 50));
    }
    return ready_.load();
}

QString ExchangeDaemonPool::credential_fingerprint(const ExchangeCredentials& creds) {
    if (creds.api_key.isEmpty() && creds.secret.isEmpty() && creds.password.isEmpty() &&
        creds.wallet_address.isEmpty() && creds.private_key.isEmpty())
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(creds.api_key.toUtf8());
    hash.addData("|");
    hash.addData(creds.secret.toUtf8());
    hash.addData("|");
    hash.addData(creds.password.toUtf8());
    hash.addData("|");
    hash.addData(creds.wallet_address.toUtf8());
    hash.addData("|");
    hash.addData(creds.private_key.toUtf8());
    return QString::fromLatin1(hash.result().toHex().left(16));
}

void ExchangeDaemonPool::forget_credentials(const QString& exchange) {
    // Mark as unknown rather than erase: an erased entry means "the daemon holds
    // nothing", which made an empty credential set skip the daemon entirely — so
    // CLEAR wiped SecureStorage while the daemon kept trading on the old keys.
    QMutexLocker lock(&mutex_);
    creds_sent_[exchange] = kCredsUnknown;
}

void ExchangeDaemonPool::clear_credentials(const QString& exchange) {
    {
        QMutexLocker lock(&mutex_);
        creds_sent_[exchange] = kCredsUnknown;
    }
    // Push the clear now instead of waiting for the next call(): the daemon drops
    // the keys and its authenticated client, so nothing can trade on them.
    auto push = [this, exchange]() { send_credentials_if_needed(exchange, ExchangeCredentials{}); };
    if (QThread::currentThread() == thread())
        push();
    else
        QMetaObject::invokeMethod(this, push, Qt::QueuedConnection);
}

void ExchangeDaemonPool::send_credentials_if_needed(const QString& exchange, const ExchangeCredentials& creds) {
    // Caller must hold no locks. Runs on the owner thread only.
    // No daemon: nothing to update. A replacement daemon starts with no keys and
    // its finished() handler resets creds_sent_, so leave the bookkeeping alone.
    if (!process_)
        return;
    const QString fp = credential_fingerprint(creds);
    {
        QMutexLocker lock(&mutex_);
        if (fp.isEmpty()) {
            // No credentials now. Only talk to the daemon if it may still hold some
            // for this exchange (sent earlier, or unknown after forget/clear).
            if (!creds_sent_.contains(exchange))
                return;
            creds_sent_.remove(exchange);
        } else {
            if (creds_sent_.value(exchange) == fp)
                return;
            creds_sent_[exchange] = fp;
        }
    }
    QJsonObject c; // empty args = "drop the credentials for this exchange"
    if (!fp.isEmpty()) {
        c["api_key"] = creds.api_key;
        c["secret"] = creds.secret;
        if (!creds.password.isEmpty())
            c["password"] = creds.password;
        if (!creds.wallet_address.isEmpty())
            c["wallet_address"] = creds.wallet_address;
        if (!creds.private_key.isEmpty())
            c["private_key"] = creds.private_key;
    }
    QJsonObject req;
    req["id"] = "__creds_" + exchange + "_" + (fp.isEmpty() ? QStringLiteral("clear") : fp);
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
            ready_seen_ = true;
            LOG_INFO(kPoolTag, logf("Exchange daemon ready",
                                    {{"pid", obj.value("data").toObject().value("pid").toVariant().toString()},
                                     {"startup_ms", since_spawn_.isValid() ? since_spawn_.elapsed() : -1}}));
            LogTransition t;
            auto& log = Logger::instance();
            if (log.transition_possible(0) && log.transition(kPoolTag, QStringLiteral("daemon"), 0, &t))
                LOG_INFO(kPoolTag, logf("Exchange daemon running again",
                                        {{"failed_starts", quick_failures_}, {"down_ms", t.ms_in_previous}}));
            response_ready_.wakeAll();
            emit ready();
            continue;
        }

        // Ignore credential-ack replies — they carry no caller.
        if (id.startsWith("__creds_"))
            continue;

        {
            QMutexLocker lock(&mutex_);
            // A reply whose caller already timed out must be dropped, not
            // stored: nothing will ever claim it, and these payloads are order
            // books and OHLC batches (5–50 KB each). call() leaves a tiny
            // tombstone under the request id when it gives up — see
            // kPoolAbandonedMarker there — which we consume here.
            auto existing = responses_.find(id);
            if (existing != responses_.end() && existing.value().value(kPoolAbandonedMarker).toBool(false)) {
                responses_.erase(existing);
                continue;
            }
            responses_[id] = obj;
        }
        response_ready_.wakeAll();
    }
}

QJsonObject ExchangeDaemonPool::call(const QString& exchange, const QString& method, const QJsonObject& args,
                                     const ExchangeCredentials& credentials, int timeout_ms) {
    if (!ready_.load()) {
        // Given up after repeated failed starts: answer at once with the reason (the
        // give-up was logged once) instead of every caller waiting out 8 s.
        if (gave_up_.load())
            return {{"error", unavailable_text()}, {"code", "DAEMON_NOT_READY"}};
        if (!wait_for_ready(std::min(timeout_ms, 8000))) {
            if (gave_up_.load())
                return {{"error", unavailable_text()}, {"code", "DAEMON_NOT_READY"}};
            // One line per outage (5 min), however many calls are dropped meanwhile.
            LOG_WARN_RL(kPoolTag, QStringLiteral("daemon_not_ready"), 300000,
                        logf("Exchange daemon not ready, call dropped", {{"exchange", exchange}, {"method", method}}));
            // Nothing was sent: callers may treat this as a definite failure.
            return {{"error", "Daemon not ready"}, {"code", "DAEMON_NOT_READY"}};
        }
    }

    const QString req_id = QString("r_%1").arg(next_req_id_.fetch_add(1));
    QJsonObject req;
    req["id"] = req_id;
    req["method"] = method;
    req["exchange"] = exchange;
    req["args"] = args;
    // How long this caller waits. The daemon answers a request that has queued
    // past it EXPIRED without sending it, and sends an order / cancel / leverage
    // change only while enough of it is left for the reply to arrive in time —
    // so an order is not placed after the caller has already given up on it.
    req["timeout_ms"] = timeout_ms;
    const QByteArray payload = QJsonDocument(req).toJson(QJsonDocument::Compact) + "\n";

    // Marshal the write + any credential send to the owner thread. Returns
    // immediately — the worker then blocks on the response wait below.
    QMetaObject::invokeMethod(
        this,
        [this, exchange, credentials, payload, req_id]() {
            if (!process_) {
                // The daemon is gone (exited between the ready check and this
                // write): nothing was sent. Answer now instead of letting the
                // caller wait out its whole budget and then report an order
                // that certainly was not placed as "not confirmed".
                {
                    QMutexLocker lock(&mutex_);
                    auto existing = responses_.find(req_id);
                    if (existing != responses_.end() && existing.value().value(kPoolAbandonedMarker).toBool(false)) {
                        responses_.erase(existing);
                        return;
                    }
                    responses_[req_id] = QJsonObject{{"id", req_id},
                                                     {"success", false},
                                                     {"error", QStringLiteral("The exchange service is not running; "
                                                                              "nothing was sent")},
                                                     {"code", "DAEMON_NOT_READY"},
                                                     {"outcome", "rejected"}};
                }
                response_ready_.wakeAll();
                return;
            }
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
    // Timed out. Two things can leak here and both are handled:
    //   1. A reply that landed between the last find() and the loop exit —
    //      overwritten by the tombstone below, so its payload is freed now.
    //   2. A reply that lands *after* we return — the normal case for a slow
    //      exchange call. drain_buffer() sees the tombstone and drops the
    //      payload instead of storing it.
    // Before this, either case left a full 5–50 KB response object in
    // responses_ with no caller left to erase it, for the life of the process.
    // The tombstone itself is a one-key object and is consumed by the late
    // reply; only a request whose reply never arrives at all leaves one behind.
    responses_[req_id] = QJsonObject{{kPoolAbandonedMarker, true}};
    lock.unlock();
    // Orders, cancels and leverage / margin changes: every timeout is written (the
    // outcome is unknown). Market-data calls: once a minute per exchange + method.
    const bool trading_call = method.contains(QLatin1String("order")) || method.contains(QLatin1String("leverage")) ||
                              method.contains(QLatin1String("margin"));
    if (trading_call)
        LOG_WARN(kPoolTag, logf("Exchange call timed out", {{"venue", exchange},
                                                            {"method", method},
                                                            {"timeout_ms", timeout_ms},
                                                            {"request", req_id}}));
    else
        LOG_WARN_RL(kPoolTag, QStringLiteral("timeout|") + exchange + QLatin1Char('|') + method, 60000,
                    logf("Exchange call timed out", {{"venue", exchange},
                                                     {"method", method},
                                                     {"timeout_ms", timeout_ms},
                                                     {"request", req_id}}));
    // "TIMEOUT": no reply yet — the request may still complete. For an order or
    // a cancel the outcome is unknown, not failed (see the crypto screen).
    return {{"error", QString("Daemon call timed out: %1/%2").arg(exchange, method)}, {"code", "TIMEOUT"}};
}

} // namespace fincept::trading
