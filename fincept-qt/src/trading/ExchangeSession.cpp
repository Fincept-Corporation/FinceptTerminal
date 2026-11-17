#include "trading/ExchangeSession.h"

#include "core/logging/Logger.h"
#include "core/process/ProcessTree.h"
#include "python/PythonRunner.h"
#include "storage/secure/SecureStorage.h"
#include "trading/BrokerRegistry.h"
#include "trading/ExchangeDaemonPool.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QPointer>
#include <QThread>
#include <QTimer>

namespace fincept::trading {

namespace {
const QString kSessionTag = "ExchangeSession";

// Bounded auto-restart policy for a WS subprocess that dies unexpectedly.
// Capped + exponentially backed off so a python script that crashes on start
// can't spin into a tight respawn loop.
constexpr int kMaxWsRestartAttempts = 5;
constexpr int kWsRestartBaseMs = 1000;    // first backoff; doubles each attempt
constexpr int kWsRestartMaxMs = 30000;    // backoff ceiling
constexpr qint64 kWsHealthyRunMs = 60000; // uptime past which a drop earns a fresh budget

// Stored credentials that could not be read for a temporary reason (busy / closed
// store) are read again after 2, 4, 8, 16 and 32 s; a permanent one waits for the user.
constexpr int kCredReadRetryBaseMs = 2000;
constexpr int kMaxCredReadRetries = 5;

// SecureStorage keys for per-exchange crypto credentials. Keeping three
// separate keys (rather than one JSON blob) matches the existing `auth`
// module convention and lets operators rotate fields independently.
QString creds_key(const QString& exchange, const QString& field) {
    return QStringLiteral("crypto:") + exchange + QLatin1Char(':') + field;
}

QString session_resolve_script_path(const QString& relative) {
    const QString dir = fincept::python::PythonRunner::instance().scripts_dir();
    if (dir.isEmpty())
        return {};
    return dir + "/" + relative;
}

bool session_is_broker_stream(const QString& id) {
    return fincept::trading::BrokerRegistry::instance().has(id);
}

QStringList session_to_broker_symbol_args(const QStringList& symbols) {
    QStringList out;
    out.reserve(symbols.size());
    for (const auto& s : symbols) {
        QString sym = s.trimmed();
        if (sym.isEmpty())
            continue;
        QString exchange = "NSE";
        if (sym.contains(':')) {
            exchange = sym.section(':', 0, 0).trimmed().toUpper();
            sym = sym.section(':', 1).trimmed();
        }
        out << (sym + ":" + exchange + ":0");
    }
    return out;
}
} // namespace

ExchangeSession::ExchangeSession(const QString& exchange_id, SessionPublisher publisher, QObject* parent)
    : QObject(parent), exchange_id_(exchange_id), publisher_(std::move(publisher)) {
    // Phase 5: rehydrate any previously-saved credentials for this exchange
    // so users don't have to re-enter API keys on every launch.
    load_stored_credentials();
}

ExchangeSession::~ExchangeSession() {
    // No ws_state_changed during teardown: the manager deletes sessions while
    // holding its mutex, and a listener re-reading the manager would deadlock.
    blockSignals(true);
    stop_ws();
}

// ── Credentials ────────────────────────────────────────────────────────────

void ExchangeSession::set_credentials(const ExchangeCredentials& creds) {
    {
        QMutexLocker lock(&mutex_);
        credentials_ = creds;
        credentials_read_error_.clear(); // the user's explicit entry replaces what could not be read
    }
    // Tell the shared daemon pool to resend on next call (it fingerprints
    // per-exchange, so no wasted writes if the key didn't actually change).
    // A CLEAR (every field empty) must also reach the running daemon right away:
    // it otherwise kept the old keys and its authenticated client, and LIVE
    // orders / cancels / leverage changes kept working on the "cleared" account.
    const bool cleared = creds.api_key.isEmpty() && creds.secret.isEmpty() && creds.password.isEmpty() &&
                         creds.wallet_address.isEmpty() && creds.private_key.isEmpty();
    if (cleared)
        ExchangeDaemonPool::instance().clear_credentials(exchange_id_);
    else
        ExchangeDaemonPool::instance().forget_credentials(exchange_id_);

    // Persist to OS-native encrypted storage so the next launch recovers
    // these without prompting. Empty fields map to a remove() so clearing
    // credentials actually wipes them from disk. A failure to persist is
    // non-fatal — the in-memory credentials still drive this session.
    auto& ss = fincept::SecureStorage::instance();
    const QString k_api = creds_key(exchange_id_, "api_key");
    const QString k_sec = creds_key(exchange_id_, "secret");
    const QString k_pw = creds_key(exchange_id_, "password");
    const QString k_wallet = creds_key(exchange_id_, "wallet_address");
    const QString k_pk = creds_key(exchange_id_, "private_key");

    auto put_or_remove = [&](const QString& key, const QString& value) {
        if (value.isEmpty()) {
            (void)ss.remove(key);
        } else {
            auto r = ss.store(key, value);
            if (r.is_err())
                LOG_WARN(kSessionTag, QString("SecureStorage: failed to persist %1").arg(key));
        }
    };
    put_or_remove(k_api, creds.api_key);
    put_or_remove(k_sec, creds.secret);
    put_or_remove(k_pw, creds.password);
    put_or_remove(k_wallet, creds.wallet_address);
    put_or_remove(k_pk, creds.private_key);

    LOG_INFO(
        kSessionTag,
        QString("Credentials updated for %1 (persisted=%2)").arg(exchange_id_, creds.api_key.isEmpty() ? "no" : "yes"));
}

void ExchangeSession::load_stored_credentials() {
    auto& ss = fincept::SecureStorage::instance();
    ExchangeCredentials next;
    {
        QMutexLocker lock(&mutex_);
        next = credentials_;
    }
    SecretRead failed;
    QString failed_field;
    auto take = [&](const char* field, QString& dest) {
        const SecretRead r = ss.read(creds_key(exchange_id_, QLatin1String(field)));
        if (r.failed()) {
            if (!failed.failed()) {
                failed = r;
                failed_field = QLatin1String(field);
            }
            return;
        }
        if (r.found() && !r.value.isEmpty())
            dest = r.value;
    };
    take("api_key", next.api_key);
    take("secret", next.secret);
    take("password", next.password);
    take("wallet_address", next.wallet_address);
    take("private_key", next.private_key);

    const bool failing = failed.failed();
    {
        QMutexLocker lock(&mutex_);
        credentials_read_error_ =
            failing ? tr("Could not read the stored credential: %1.").arg(failed.reason()) : QString();
    }
    LOG_TRANSITION(kSessionTag, QStringLiteral("cred_read|") + exchange_id_, failing,
                   failing ? LogLevel::Warn : LogLevel::Info,
                   failing ? logf("Stored exchange credentials could not be read; the session keeps no keys and "
                                  "nothing is written",
                                  {{"exchange", exchange_id_}, {"field", failed_field}, {"kind", failed.error_name()}})
                           : logf("Stored exchange credentials readable again", {{"exchange", exchange_id_}}));
    if (failing) {
        // A partial key set (key without its secret, wallet without its private key)
        // would authenticate as the wrong / an incomplete account: install nothing.
        if (failed.transient() && cred_read_retries_ < kMaxCredReadRetries) {
            const int delay_ms = kCredReadRetryBaseMs << cred_read_retries_;
            ++cred_read_retries_;
            QPointer<ExchangeSession> self = this;
            QTimer::singleShot(delay_ms, this, [self]() {
                if (self)
                    self->load_stored_credentials();
            });
        }
        return;
    }
    cred_read_retries_ = 0;

    if (next.api_key.isEmpty() && next.secret.isEmpty() && next.password.isEmpty() && next.wallet_address.isEmpty() &&
        next.private_key.isEmpty())
        return; // nothing stored — leave session state as-is

    {
        QMutexLocker lock(&mutex_);
        credentials_ = next;
    }
    // Invalidate any cached "already sent" state on the daemon so the next
    // RPC picks up the restored keys.
    ExchangeDaemonPool::instance().forget_credentials(exchange_id_);
    LOG_DEBUG(kSessionTag, "Restored stored credentials for " + exchange_id_);
}

ExchangeCredentials ExchangeSession::get_credentials() const {
    QMutexLocker lock(&mutex_);
    return credentials_;
}

QString ExchangeSession::credentials_read_error() const {
    QMutexLocker lock(&mutex_);
    return credentials_read_error_;
}

// ── Daemon RPC helper ──────────────────────────────────────────────────────

QJsonObject ExchangeSession::daemon_call(const QString& method, const QJsonObject& args, int timeout_ms) {
    ExchangeCredentials creds;
    {
        QMutexLocker lock(&mutex_);
        creds = credentials_;
    }
    return ExchangeDaemonPool::instance().call(exchange_id_, method, args, creds, timeout_ms);
}

QJsonObject ExchangeSession::request(const QString& method, const QJsonObject& args, int timeout_ms) {
    return daemon_call(method, args, timeout_ms);
}

QJsonObject ExchangeSession::request_as(const QString& method, const QJsonObject& args,
                                        const ExchangeCredentials& creds, int timeout_ms) {
    return ExchangeDaemonPool::instance().call(exchange_id_, method, args, creds, timeout_ms);
}

QString ExchangeSession::account_id() const {
    QMutexLocker lock(&mutex_);
    return ExchangeDaemonPool::credential_fingerprint(credentials_);
}

QString ExchangeSession::account_label() const {
    QMutexLocker lock(&mutex_);
    const QString wallet = credentials_.wallet_address.trimmed();
    if (!wallet.isEmpty())
        return wallet.size() > 12 ? tr("wallet %1…%2").arg(wallet.left(6), wallet.right(4)) : tr("wallet %1").arg(wallet);
    const QString key = credentials_.api_key.trimmed();
    if (key.isEmpty())
        return credentials_read_error_.isEmpty() ? tr("no API key") : tr("stored keys unreadable");
    return key.size() > 10 ? tr("API key %1…%2").arg(key.left(4), key.right(4)) : tr("API key ****");
}

// ── Watch management ───────────────────────────────────────────────────────

void ExchangeSession::watch_symbol(const QString& symbol, const QString& portfolio_id) {
    QMutexLocker lock(&mutex_);
    watched_[symbol].insert(portfolio_id);
}

void ExchangeSession::unwatch_symbol(const QString& symbol, const QString& portfolio_id) {
    QMutexLocker lock(&mutex_);
    auto it = watched_.find(symbol);
    if (it != watched_.end()) {
        it->remove(portfolio_id);
        if (it->isEmpty())
            watched_.erase(it);
    }
}

QHash<QString, QSet<QString>> ExchangeSession::snapshot_watched() const {
    QMutexLocker lock(&mutex_);
    return watched_;
}

// ── Callbacks ──────────────────────────────────────────────────────────────

// ── WS lifecycle ───────────────────────────────────────────────────────────

bool ExchangeSession::start_ws(const QString& primary_symbol, const QStringList& all_symbols, bool hub_owned) {
    stop_ws();

    ws_primary_symbol_ = primary_symbol;
    ws_all_symbols_ = all_symbols;
    // Fold in the pairs DataHub subscribers (dashboard ticker / trade tiles) need, so
    // a screen starting its own stream does not silently drop them.
    for (const auto& pair : hub_pairs_) {
        if (!ws_all_symbols_.contains(pair))
            ws_all_symbols_.append(pair);
    }

    const QString python_path = python::PythonRunner::instance().python_path();
    QString script_path;
    QStringList args;

    // Every Python spawn must inherit the shared base env (PYTHONIOENCODING,
    // PYTHONDONTWRITEBYTECODE, FINCEPT_DATA_DIR, PYTHONPATH, …). This process
    // previously called neither setProcessEnvironment nor build_python_env(),
    // so the WS bridge ran without any of them. Broker credentials ride in
    // this env too — see the SECURITY note below.
    QProcessEnvironment ws_env = python::PythonRunner::instance().build_python_env();

    if (session_is_broker_stream(exchange_id_)) {
        script_path = session_resolve_script_path("exchange/broker_ws_bridge.py");
        if (python_path.isEmpty() || script_path.isEmpty()) {
            LOG_ERROR(kSessionTag, "Python or broker_ws_bridge.py not found");
            return false;
        }
        auto* broker = BrokerRegistry::instance().get(exchange_id_);
        if (!broker) {
            LOG_ERROR(kSessionTag, "Broker stream requested but broker is not registered: " + exchange_id_);
            return false;
        }
        QString creds_error;
        auto creds = broker->load_credentials(&creds_error);
        if (!creds_error.isEmpty()) {
            // Stored but unreadable — not "missing" (BrokerRegistry logged it once).
            emit ws_failed(exchange_id_, tr("Live prices from %1 are unavailable: %2").arg(exchange_id_, creds_error));
            return false;
        }
        if (creds.api_key.isEmpty() || creds.access_token.isEmpty()) {
            LOG_ERROR(kSessionTag, "Broker stream credentials missing for " + exchange_id_);
            return false;
        }
        QString feed_token;
        QString user_id = creds.user_id;
        if (!creds.additional_data.isEmpty()) {
            QJsonParseError ad_error;
            const auto ad = QJsonDocument::fromJson(creds.additional_data.toUtf8(), &ad_error).object();
            if (ad_error.error != QJsonParseError::NoError) // never the blob itself
                LOG_WARN(kSessionTag, logf("Broker additional_data is not valid JSON, feed token unavailable",
                                           {{"exchange", exchange_id_}, {"error", ad_error.errorString()}}));
            if (user_id.isEmpty())
                user_id = ad.value("client_code").toString();
            feed_token = ad.value("feed_token").toString();
        }
        // NOTE: deliberately no `user_id = creds.api_key` fallback here — that
        // put the api_key back into argv. broker_ws_bridge.py applies the same
        // fallback internally, where the key never leaves the process.

        QStringList broker_symbols = session_to_broker_symbol_args(all_symbols);
        if (broker_symbols.isEmpty())
            broker_symbols = session_to_broker_symbol_args({primary_symbol});
        if (broker_symbols.isEmpty()) {
            LOG_ERROR(kSessionTag, "No symbols provided for broker websocket stream");
            return false;
        }

        // ── SECURITY: broker credentials go in the environment, never argv ───
        // api_key / access_token / feed_token are live broker credentials that
        // stay valid for the whole trading session. A process command line is
        // readable by any process running as the same user (WMI
        // Win32_Process.CommandLine on Windows with no elevation,
        // /proc/<pid>/cmdline on Linux) and is captured by crash dumps and EDR
        // telemetry — passing them there undoes SecureStorage entirely.
        // build_python_env() already carries 17+ other credential keys this
        // way. broker_ws_bridge.py reads os.environ first and only falls back
        // to argv so an older caller still works.
        ws_env.insert(QStringLiteral("BROKER_API_KEY"), creds.api_key);
        ws_env.insert(QStringLiteral("BROKER_ACCESS_TOKEN"), creds.access_token);
        if (!feed_token.isEmpty())
            ws_env.insert(QStringLiteral("BROKER_FEED_TOKEN"), feed_token);

        // Positional arity is preserved (broker, api_key, access_token,
        // user_id) so the script's argparse contract is unchanged; the two
        // credential slots are now empty placeholders. user_id is a client
        // code, not a secret, and may legitimately be empty.
        args << "-u" << "-B" << script_path << exchange_id_ << QString() << QString() << user_id;
        args << "--symbols";
        args.append(broker_symbols);
    } else {
        script_path = session_resolve_script_path("exchange/ws_stream.py");
        if (python_path.isEmpty() || script_path.isEmpty()) {
            LOG_ERROR(kSessionTag, "Python or ws_stream.py not found");
            emit ws_failed(exchange_id_, python_path.isEmpty() ? python::PythonRunner::python_unavailable_message()
                                                               : tr("Live prices are unavailable: the streaming "
                                                                    "script is missing from this installation."));
            return false;
        }
        args << "-u" << "-B" << script_path << exchange_id_;
        for (const auto& sym : ws_all_symbols_)
            args << sym;
    }

    ws_process_ = new QProcess(this);
    // The bridge and anything it starts are one unit: stop_ws() kills the whole tree,
    // a crashed bridge's leftovers are reaped when its QProcess is deleted, and on
    // Windows the OS kills the tree if the terminal itself dies (kill-on-close job).
    // Also hides the console window. Must precede start().
    process::own_tree(ws_process_);
    ws_process_->setProcessEnvironment(ws_env);
    connect(ws_process_, &QProcess::readyReadStandardOutput, this, &ExchangeSession::drain_ws_buffer);
    connect(ws_process_, &QProcess::readyReadStandardError, this, [this]() {
        if (!ws_process_)
            return;
        const QString err = QString::fromUtf8(ws_process_->readAllStandardError()).trimmed();
        if (!err.isEmpty()) {
            LOG_DEBUG(kSessionTag, QString("[%1] WS stderr: %2").arg(exchange_id_, err));
            // The last line is reported with the process exit (handle_ws_finished).
            ws_process_->setProperty("fincept_last_stderr", err.section(QLatin1Char('\n'), -1).trimmed().left(200));
        }
    });
    connect(ws_process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        LOG_ERROR(kSessionTag,
                  QString("[%1] WS process errorOccurred: %2").arg(exchange_id_).arg(static_cast<int>(err)));
        ws_connected_ = false;
        emit ws_state_changed(exchange_id_);
    });
    // A clean subprocess exit emits no python "status" line, so ws_connected_
    // would otherwise stay stale (badge stuck on LIVE). Detect the death here,
    // drop the flag, and attempt a bounded respawn. stop_ws() disconnects
    // `this` before a deliberate terminate, so this only fires on unexpected
    // exits.
    connect(ws_process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) { handle_ws_finished(code, status); });

    ws_last_error_.clear();
    ws_error_fatal_ = false;
    ws_process_->start(python_path, args);
    ws_should_run_ = true;
    ws_hub_owned_ = hub_owned;
    ws_uptime_.restart();
    LOG_DEBUG(kSessionTag, QString("WS stream start requested for %1/%2").arg(exchange_id_, primary_symbol));
    emit ws_state_changed(exchange_id_);
    return true;
}

QString ExchangeSession::ws_failure_text() const {
    if (ws_error_fatal_ && ws_last_error_.contains(QLatin1String("not available"), Qt::CaseInsensitive))
        return tr("Live prices are unavailable: the exchange streaming library (ccxt) is not installed. "
                  "Open Settings > Python Environment to install it.");
    if (ws_error_fatal_)
        return tr("Live prices are unavailable: %1 is not supported for streaming.").arg(exchange_id_);
    if (!ws_last_error_.isEmpty())
        return tr("Live prices from %1 stopped: %2").arg(exchange_id_, ws_last_error_);
    return tr("Live prices from %1 stopped and could not be restarted.").arg(exchange_id_);
}

bool ExchangeSession::is_ws_active() const {
    return ws_process_ != nullptr && ws_process_->state() != QProcess::NotRunning;
}

void ExchangeSession::stop_ws() {
    if (!ws_process_)
        return;
    auto* proc = ws_process_;
    ws_process_ = nullptr;
    ws_connected_ = false;
    ws_should_run_ = false; // deliberate stop — suppress the finished-driven respawn
    ws_hub_owned_ = false;
    proc->disconnect(this);
    proc->terminate();
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), proc, &QObject::deleteLater);
    QTimer::singleShot(3000, proc, [proc]() {
        if (proc->state() != QProcess::NotRunning)
            process::kill_tree(proc); // the bridge and every process it started
    });
    emit ws_state_changed(exchange_id_);
}

void ExchangeSession::handle_ws_finished(int exit_code, QProcess::ExitStatus status) {
    ws_connected_ = false;
    QString last_stderr;
    if (ws_process_) {
        const QString tail = QString::fromUtf8(ws_process_->readAllStandardError()).trimmed();
        last_stderr = tail.isEmpty() ? ws_process_->property("fincept_last_stderr").toString()
                                     : tail.section(QLatin1Char('\n'), -1).trimmed().left(200);
    }
    LOG_WARN(kSessionTag, logf("Exchange stream process exited", {{"exchange", exchange_id_},
                                                                  {"code", exit_code},
                                                                  {"status", status == QProcess::CrashExit ? "crash"
                                                                                                           : "normal"},
                                                                  {"restarts", ws_restart_attempts_},
                                                                  {"last_stderr", log_scrub(last_stderr)}}));

    // Reap the finished process object. stop_ws() disconnects `this` before a
    // deliberate terminate, so reaching here means an *unexpected* exit.
    if (ws_process_) {
        // A script that fails at startup prints its reason and exits at once;
        // `finished` can arrive before those lines were drained. Read them now,
        // or the reason is lost with the process object.
        while (ws_process_->canReadLine()) {
            const QString line = QString::fromUtf8(ws_process_->readLine()).trimmed();
            if (!line.isEmpty())
                handle_ws_line(line);
        }
        ws_process_->deleteLater();
        ws_process_ = nullptr;
    }
    emit ws_state_changed(exchange_id_);

    if (!ws_should_run_)
        return; // not supposed to be running — leave it down

    // A missing library or an exchange the library does not know fails the same
    // way on every respawn. Say so once instead of burning the retry budget
    // while subscribers sit on a loading state. ws_should_run_ is dropped so
    // nothing reports a respawn as pending; the DataHub demand path still
    // re-checks periodically, which picks the stream up once the library is
    // installed.
    if (ws_error_fatal_) {
        ws_should_run_ = false;
        LOG_ERROR(kSessionTag, QString("[%1] WS not restarting — %2").arg(exchange_id_, ws_last_error_));
        emit ws_failed(exchange_id_, ws_failure_text());
        return;
    }

    // A stream that ran healthily and only just dropped earns a fresh restart
    // budget; a fast crash-loop keeps burning the existing one.
    if (ws_uptime_.isValid() && ws_uptime_.elapsed() >= kWsHealthyRunMs)
        ws_restart_attempts_ = 0;

    if (ws_restart_attempts_ >= kMaxWsRestartAttempts) {
        LOG_ERROR(
            kSessionTag,
            QString("[%1] WS not restarting — gave up after %2 attempts").arg(exchange_id_).arg(ws_restart_attempts_));
        emit ws_failed(exchange_id_, ws_failure_text());
        return;
    }

    ++ws_restart_attempts_;
    const int backoff_ms = qMin(kWsRestartBaseMs * (1 << (ws_restart_attempts_ - 1)), kWsRestartMaxMs);
    LOG_DEBUG(kSessionTag, QString("[%1] WS restart attempt %2/%3 in %4ms")
                              .arg(exchange_id_)
                              .arg(ws_restart_attempts_)
                              .arg(kMaxWsRestartAttempts)
                              .arg(backoff_ms));

    QPointer<ExchangeSession> self = this;
    const QString primary = ws_primary_symbol_;
    const QStringList all = ws_all_symbols_;
    const bool hub_owned = ws_hub_owned_; // a respawn must not change who owns the stream
    QTimer::singleShot(backoff_ms, this, [self, primary, all, hub_owned]() {
        if (!self || !self->ws_should_run_)
            return;
        if (self->is_ws_active())
            return; // already back up (e.g. a manual restart beat us to it)
        self->start_ws(primary, all, hub_owned);
    });
}

void ExchangeSession::set_ws_primary_symbol(const QString& symbol) {
    {
        QMutexLocker lock(&mutex_);
        if (ws_primary_symbol_ == symbol)
            return;
        ws_primary_symbol_ = symbol;
    }
    // The re-pointed primary is now part of what the process streams. Record it so
    // the launch list stays truthful — a later respawn starts from it, and the
    // DataHub demand path (ExchangeSessionManager) compares against it to decide
    // whether the stream already covers a pair, instead of relaunching on every
    // symbol switch.
    if (!ws_all_symbols_.contains(symbol))
        ws_all_symbols_.append(symbol);
    // Re-point the live stream in place — ws_stream.py switches its
    // orderbook/trades/ohlc tasks to the new primary (stdin command) without
    // dropping the watchlist ticker streams. Fall back to a relaunch only if
    // the process isn't up yet, or this is a broker bridge (no cmd protocol).
    if (ws_process_ && ws_process_->state() != QProcess::NotRunning && !session_is_broker_stream(exchange_id_)) {
        QJsonObject cmd;
        cmd[QStringLiteral("cmd")] = QStringLiteral("set_primary");
        cmd[QStringLiteral("symbol")] = symbol;
        ws_process_->write(QJsonDocument(cmd).toJson(QJsonDocument::Compact) + '\n');
        return;
    }
    if (ws_process_) {
        QStringList syms = ws_all_symbols_;
        syms.removeAll(symbol);
        syms.prepend(symbol);
        start_ws(symbol, syms);
    }
}

void ExchangeSession::set_ws_timeframe(const QString& timeframe) {
    // Re-point only the OHLC stream to a new chart timeframe, in place.
    if (!ws_process_ || ws_process_->state() == QProcess::NotRunning || session_is_broker_stream(exchange_id_))
        return;
    QJsonObject cmd;
    cmd[QStringLiteral("cmd")] = QStringLiteral("set_timeframe");
    cmd[QStringLiteral("timeframe")] = timeframe;
    ws_process_->write(QJsonDocument(cmd).toJson(QJsonDocument::Compact) + '\n');
}

QString ExchangeSession::get_ws_primary_symbol() const {
    QMutexLocker lock(&mutex_);
    return ws_primary_symbol_;
}

// ── WS line handler (ported from ExchangeService::handle_ws_line) ──────────

QStringList ExchangeSession::publish_symbols(const QString& exchange_symbol) const {
    QStringList out;
    {
        QMutexLocker lock(&mutex_);
        out = ws_symbol_map_.value(exchange_symbol);
    }
    // The market's own symbol too: a screen that selected the exact market
    // (e.g. "BTC/USDC:USDC" after choosing it over "BTC/USDT") subscribes to it.
    if (!out.contains(exchange_symbol))
        out.append(exchange_symbol);
    return out;
}

void ExchangeSession::handle_ws_line(const QString& line) {
    if (line.isEmpty() || line[0] != '{')
        return;
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(line.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) {
        LOG_DEBUG_RL(kSessionTag, QStringLiteral("ws_line|") + exchange_id_, 60000,
                     logf("Unparseable line from the stream process", {{"exchange", exchange_id_},
                                                                       {"bytes", line.size()},
                                                                       {"error", err.errorString()}}));
        return;
    }
    const auto msg = doc.object();
    const QString type = msg.value("type").toString();

    if (type == "status") {
        const bool connected = msg.value("connected").toBool(false);
        const bool prev = ws_connected_.exchange(connected);
        if (connected != prev) {
            LOG_INFO(kSessionTag,
                     QString("[%1] WS status: %2").arg(exchange_id_, connected ? "CONNECTED" : "DISCONNECTED"));
            emit ws_state_changed(exchange_id_);
        }
        if (!connected) {
            QMutexLocker lock(&mutex_);
            ws_symbol_map_.clear();
        }
        return;
    }

    if (type == "error") {
        // Per-symbol / per-channel errors carry a "source" and the stream keeps
        // running; one without it is the process giving up (see ws_stream.py main()).
        if (msg.contains("source"))
            return;
        ws_last_error_ = msg.value("message").toString();
        const QString code = msg.value("code").toString();
        ws_error_fatal_ = code == QLatin1String("missing_dependency") || code == QLatin1String("unknown_exchange");
        LOG_WARN(kSessionTag, QString("[%1] WS error: %2").arg(exchange_id_, ws_last_error_));
        return;
    }

    if (type == "symbol_map") {
        // MERGE, don't replace: the stream sends the full map once at startup, but
        // a set_primary switch sends a ONE-entry map for the new primary only.
        // Replacing dropped every other watchlist remap, so those tickers arrived
        // under the exchange's symbol ("ETH/USDC:USDC"), were published on a topic
        // nobody subscribes to, and their rows froze. The map is reset on a
        // disconnect (status connected=false above), when the stream restarts.
        // "aliases" lists every requested symbol shown from one market (two pairs
        // can resolve to the same market); "map" is the older one-alias form.
        QMutexLocker lock(&mutex_);
        auto add = [this](const QString& market, const QString& alias) {
            if (market.isEmpty() || alias.isEmpty())
                return;
            QStringList& list = ws_symbol_map_[market];
            if (!list.contains(alias))
                list.append(alias);
        };
        const auto map_obj = msg.value("map").toObject();
        for (auto it = map_obj.constBegin(); it != map_obj.constEnd(); ++it)
            add(it.key(), it.value().toString());
        const auto aliases = msg.value("aliases").toObject();
        for (auto it = aliases.constBegin(); it != aliases.constEnd(); ++it) {
            for (const auto& v : it.value().toArray())
                add(it.key(), v.toString());
        }
        LOG_DEBUG(kSessionTag,
                  QString("[%1] Symbol map updated: %2 remaps").arg(exchange_id_).arg(ws_symbol_map_.size()));
        return;
    }

    if (type == "ticker" || type == "tick") {
        TickerData ticker;
        if (type == "tick") {
            ticker.symbol = msg.value("symbol").toString();
            ticker.last = msg.value("ltp").toDouble();
            ticker.bid = msg.value("bid").toDouble();
            ticker.ask = msg.value("ask").toDouble();
            ticker.high = msg.value("high").toDouble();
            ticker.low = msg.value("low").toDouble();
            ticker.open = msg.value("open").toDouble();
            ticker.close = msg.value("close").toDouble();
            ticker.base_volume = msg.value("volume").toDouble();
            ticker.timestamp = msg.value("timestamp").toVariant().toLongLong();
        } else {
            QJsonObject normalised = msg;
            if (!msg.contains("baseVolume") && msg.contains("base_volume"))
                normalised["baseVolume"] = msg.value("base_volume");
            if (!msg.contains("quoteVolume") && msg.contains("quote_volume"))
                normalised["quoteVolume"] = msg.value("quote_volume");
            ticker = parse_ticker(normalised);
        }
        if (ticker.symbol.isEmpty() || ticker.last <= 0.0)
            return;
        // Publish under every symbol shown from this market so consumers (cache,
        // hub topics) key by the symbol they asked for — on Hyperliquid
        // "BTC/USDC:USDC" reaches both "BTC/USDT" and "BTC/USDC:USDC".
        for (const QString& sym : publish_symbols(ticker.symbol)) {
            TickerData t = ticker;
            t.symbol = sym;
            {
                QMutexLocker lock(&mutex_);
                price_cache_[sym] = t;
            }
            if (publisher_.publish_ticker)
                publisher_.publish_ticker(exchange_id_, sym, t);
        }
        return;
    }

    if (type == "orderbook") {
        const OrderBookData parsed = parse_orderbook(msg);
        for (const QString& sym : publish_symbols(parsed.symbol)) {
            OrderBookData ob = parsed;
            ob.symbol = sym;
            if (publisher_.publish_orderbook)
                publisher_.publish_orderbook(exchange_id_, sym, ob);
        }
        return;
    }

    if (type == "candle" || type == "ohlcv") {
        QJsonObject candle_obj = (type == "ohlcv") ? msg.value("candle").toObject() : msg;
        Candle c = parse_candle(candle_obj);
        const QString raw_sym = msg.value("symbol").toString();
        if (raw_sym.isEmpty() || c.timestamp == 0)
            return;
        const QString interval = msg.value("interval").toString("1m");
        for (const QString& sym : publish_symbols(raw_sym)) {
            if (publisher_.publish_candle)
                publisher_.publish_candle(exchange_id_, sym, interval, c);
        }
        return;
    }

    if (type == "trade") {
        TradeData parsed;
        const QString raw_sym = msg.value("symbol").toString();
        parsed.side = msg.value("side").toString();
        parsed.price = msg.value("price").toDouble();
        parsed.amount = msg.value("amount").toDouble();
        parsed.timestamp = msg.value("timestamp").toVariant().toLongLong();
        if (raw_sym.isEmpty())
            return;

        for (const QString& sym : publish_symbols(raw_sym)) {
            TradeData td = parsed;
            td.symbol = sym;
            // Fast-path ticker update from trade — on exchanges where watch_ticker
            // is slow (e.g. Kraken spot ~2/15s), trade flow keeps price_cache_
            // current. Publish a synthetic ticker alongside so the hub's
            // ticker:<pair> topic stays fresh.
            TickerData fast_ticker;
            bool emit_ticker = false;
            if (td.price > 0.0) {
                QMutexLocker lock(&mutex_);
                auto& cached = price_cache_[td.symbol];
                cached.symbol = td.symbol;
                cached.last = td.price;
                if (cached.bid <= 0.0)
                    cached.bid = td.price;
                if (cached.ask <= 0.0)
                    cached.ask = td.price;
                fast_ticker = cached;
                emit_ticker = true;
            }

            if (publisher_.publish_trade)
                publisher_.publish_trade(exchange_id_, td.symbol, td);
            if (emit_ticker && publisher_.publish_ticker)
                publisher_.publish_ticker(exchange_id_, fast_ticker.symbol, fast_ticker);
        }
    }
}

void ExchangeSession::drain_ws_buffer() {
    if (!ws_process_)
        return;
    int processed = 0;
    constexpr int MAX_LINES_PER_CYCLE = 50;
    constexpr qint64 MAX_MS_PER_CYCLE = 4;
    QElapsedTimer elapsed;
    elapsed.start();
    while (ws_process_->canReadLine() && processed < MAX_LINES_PER_CYCLE) {
        const QString line = QString::fromUtf8(ws_process_->readLine()).trimmed();
        if (!line.isEmpty())
            handle_ws_line(line);
        ++processed;
        if (elapsed.elapsed() >= MAX_MS_PER_CYCLE)
            break;
    }
    if (ws_process_ && ws_process_->canReadLine()) {
        QPointer<ExchangeSession> self = this;
        QTimer::singleShot(1, this, [self]() {
            if (self)
                self->drain_ws_buffer();
        });
    }
}

// ── Parsers ────────────────────────────────────────────────────────────────

TickerData ExchangeSession::parse_ticker(const QJsonObject& j) {
    TickerData t;
    t.symbol = j.value("symbol").toString();
    t.last = j.value("last").toDouble();
    t.bid = j.value("bid").toDouble();
    t.ask = j.value("ask").toDouble();
    t.high = j.value("high").toDouble();
    t.low = j.value("low").toDouble();
    t.open = j.value("open").toDouble();
    t.close = j.value("close").toDouble();
    t.change = j.value("change").toDouble();
    t.percentage = j.value("percentage").toDouble();
    t.base_volume = j.value("baseVolume").toDouble();
    t.quote_volume = j.value("quoteVolume").toDouble();
    t.timestamp = j.value("timestamp").toVariant().toLongLong();
    return t;
}

OrderBookData ExchangeSession::parse_orderbook(const QJsonObject& j) {
    OrderBookData ob;
    ob.symbol = j.value("symbol").toString();
    const auto bids = j.value("bids").toArray();
    for (const auto& b : bids) {
        const auto arr = b.toArray();
        if (arr.size() >= 2)
            ob.bids.append({arr[0].toDouble(), arr[1].toDouble()});
    }
    const auto asks = j.value("asks").toArray();
    for (const auto& a : asks) {
        const auto arr = a.toArray();
        if (arr.size() >= 2)
            ob.asks.append({arr[0].toDouble(), arr[1].toDouble()});
    }
    if (!ob.bids.isEmpty())
        ob.best_bid = ob.bids.first().first;
    if (!ob.asks.isEmpty())
        ob.best_ask = ob.asks.first().first;
    if (ob.best_bid > 0 && ob.best_ask > 0) {
        ob.spread = ob.best_ask - ob.best_bid;
        ob.spread_pct = (ob.spread / ob.best_ask) * 100.0;
    }
    return ob;
}

Candle ExchangeSession::parse_candle(const QJsonObject& j) {
    Candle c;
    c.timestamp = j.value("timestamp").toVariant().toLongLong();
    c.open = j.value("open").toDouble();
    c.high = j.value("high").toDouble();
    c.low = j.value("low").toDouble();
    c.close = j.value("close").toDouble();
    c.volume = j.value("volume").toDouble();
    return c;
}

MarketInfo ExchangeSession::parse_market(const QJsonObject& j) {
    MarketInfo m;
    m.symbol = j.value("symbol").toString();
    m.base = j.value("base").toString();
    m.quote = j.value("quote").toString();
    m.type = j.value("type").toString();
    m.active = j.value("active").toBool(true);
    m.maker_fee = j.value("maker").toDouble();
    m.taker_fee = j.value("taker").toDouble();
    m.min_amount = j.value("minAmount").toDouble();
    m.min_cost = j.value("minCost").toDouble();
    return m;
}

// ── One-shot REST fetches — all via the shared daemon pool ─────────────────

TickerData ExchangeSession::fetch_ticker(const QString& symbol) {
    const auto j = daemon_call("fetch_ticker", {{"symbol", symbol}});
    if (j.contains("error"))
        return {};
    return parse_ticker(j);
}

QVector<TickerData> ExchangeSession::fetch_tickers(const QStringList& symbols) {
    QJsonArray sym_arr;
    for (const auto& s : symbols)
        sym_arr.append(s);
    const auto j = daemon_call("fetch_tickers", {{"symbols", sym_arr}});
    QVector<TickerData> result;
    if (j.contains("error") || !j.contains("tickers"))
        return result;
    const auto arr = j.value("tickers").toArray();
    for (const auto& item : arr)
        result.append(parse_ticker(item.toObject()));
    return result;
}

OrderBookData ExchangeSession::fetch_orderbook(const QString& symbol, int limit) {
    const auto j = daemon_call("fetch_orderbook", {{"symbol", symbol}, {"limit", limit}});
    if (j.contains("error"))
        return {};
    return parse_orderbook(j);
}

QVector<Candle> ExchangeSession::fetch_ohlcv(const QString& symbol, const QString& timeframe, int limit) {
    const auto j = daemon_call("fetch_ohlcv", {{"symbol", symbol}, {"timeframe", timeframe}, {"limit", limit}});
    QVector<Candle> result;
    if (j.contains("error") || !j.contains("candles")) {
        // Swallowing this left the chart silently empty and the terminal
        // showing live-only bars with no way to tell why (#338).
        // Daemon down: the pool reports the outage once; repeats of one failure once a minute.
        const QString error = j.value("error").toString(QStringLiteral("no candles in response"));
        if (j.value("code").toString() == QLatin1String("DAEMON_NOT_READY"))
            LOG_DEBUG(kSessionTag, logf("Candles could not be fetched", {{"exchange", exchange_id_},
                                                                        {"symbol", symbol},
                                                                        {"timeframe", timeframe},
                                                                        {"error", error}}));
        else
            LOG_WARN_RL(kSessionTag, QStringLiteral("ohlcv|") + exchange_id_, 60000,
                        logf("Candles could not be fetched", {{"exchange", exchange_id_},
                                                             {"symbol", symbol},
                                                             {"timeframe", timeframe},
                                                             {"error", error}}));
        return result;
    }
    const auto arr = j.value("candles").toArray();
    for (const auto& item : arr)
        result.append(parse_candle(item.toObject()));
    return result;
}

QVector<MarketInfo> ExchangeSession::fetch_markets(const QString& type, const QString& query) {
    QJsonObject args;
    if (!type.isEmpty())
        args["type"] = type;
    if (!query.isEmpty()) {
        args["query"] = query;
        args["limit"] = 100;
    }
    const auto j = daemon_call("fetch_markets", args);
    QVector<MarketInfo> result;
    if (j.contains("error") || !j.contains("markets"))
        return result;
    const auto arr = j.value("markets").toArray();
    for (const auto& item : arr)
        result.append(parse_market(item.toObject()));
    return result;
}

MarketInfo ExchangeSession::fetch_market(const QString& symbol) {
    const auto j = daemon_call("fetch_markets", {{"symbol", symbol}});
    if (j.contains("error"))
        return {};
    const auto arr = j.value("markets").toArray();
    return arr.isEmpty() ? MarketInfo{} : parse_market(arr.first().toObject());
}

QVector<TradeData> ExchangeSession::fetch_trades(const QString& symbol, int limit) {
    const auto j = daemon_call("fetch_trades", {{"symbol", symbol}, {"limit", limit}});
    QVector<TradeData> result;
    if (j.contains("error") || !j.contains("trades"))
        return result;
    const auto arr = j.value("trades").toArray();
    for (const auto& item : arr) {
        const auto o = item.toObject();
        TradeData td;
        td.id = o.value("id").toString();
        td.symbol = o.value("symbol").toString();
        td.side = o.value("side").toString();
        td.price = o.value("price").toDouble();
        td.amount = o.value("amount").toDouble();
        td.cost = o.value("cost").toDouble();
        td.timestamp = o.value("timestamp").toVariant().toLongLong();
        result.append(td);
    }
    return result;
}

FundingRateData ExchangeSession::fetch_funding_rate(const QString& symbol) {
    const auto j = daemon_call("fetch_funding_rate", {{"symbol", symbol}});
    FundingRateData fr;
    if (!j.contains("error")) {
        fr.symbol = j.value("symbol").toString();
        fr.funding_rate = j.value("funding_rate").toDouble();
        fr.mark_price = j.value("mark_price").toDouble();
        fr.index_price = j.value("index_price").toDouble();
        fr.funding_timestamp = j.value("funding_timestamp").toVariant().toLongLong();
        fr.next_funding_timestamp = j.value("next_funding_timestamp").toVariant().toLongLong();
    }
    return fr;
}

OpenInterestData ExchangeSession::fetch_open_interest(const QString& symbol) {
    const auto j = daemon_call("fetch_open_interest", {{"symbol", symbol}});
    OpenInterestData oi;
    if (!j.contains("error")) {
        oi.symbol = j.value("symbol").toString();
        oi.open_interest = j.value("open_interest").toDouble();
        oi.open_interest_value = j.value("open_interest_value").toDouble();
        oi.timestamp = j.value("timestamp").toVariant().toLongLong();
    }
    return oi;
}

MarkPriceData ExchangeSession::fetch_mark_price(const QString& symbol) {
    const auto j = daemon_call("fetch_mark_price", {{"symbol", symbol}});
    MarkPriceData mp;
    mp.symbol = symbol;
    if (!j.contains("error")) {
        mp.mark_price = j.value("mark_price").toDouble();
        mp.index_price = j.value("index_price").toDouble();
        mp.timestamp = j.value("timestamp").toVariant().toLongLong();
    }
    return mp;
}

QJsonObject ExchangeSession::fetch_balance() {
    return daemon_call("fetch_balance", {});
}

// Orders: there is deliberately no one-call "place order" here any more. An
// order is two daemon calls made by the caller with request_as():
// "prepare_order" (decides and returns exactly what would be sent: market,
// size in the exchange's units, price, params, confirm_hash) and, after the
// user confirmed that, "place_order" with the confirm_hash, ref_price and
// account_id — the daemon rebuilds the order and refuses if it differs.

QJsonObject ExchangeSession::cancel_exchange_order(const QString& order_id, const QString& symbol) {
    return daemon_call("cancel_order", {{"order_id", order_id}, {"symbol", symbol}});
}

QJsonObject ExchangeSession::fetch_positions_live(const QString& symbol) {
    QJsonObject args;
    if (!symbol.isEmpty())
        args["symbol"] = symbol;
    return daemon_call("fetch_positions", args);
}

QJsonObject ExchangeSession::fetch_open_orders_live(const QString& symbol) {
    QJsonObject args;
    if (!symbol.isEmpty())
        args["symbol"] = symbol;
    return daemon_call("fetch_open_orders", args);
}

QJsonObject ExchangeSession::fetch_my_trades(const QString& symbol, int limit) {
    return daemon_call("fetch_my_trades", {{"symbol", symbol}, {"limit", limit}});
}

QJsonObject ExchangeSession::fetch_trading_fees(const QString& symbol) {
    QJsonObject args;
    if (!symbol.isEmpty())
        args["symbol"] = symbol;
    return daemon_call("fetch_trading_fees", args);
}

QJsonObject ExchangeSession::set_leverage(const QString& symbol, int leverage) {
    return daemon_call("set_leverage", {{"symbol", symbol}, {"leverage", leverage}});
}

QJsonObject ExchangeSession::set_margin_mode(const QString& symbol, const QString& mode) {
    return daemon_call("set_margin_mode", {{"symbol", symbol}, {"mode", mode}});
}

// ── Cache accessors ────────────────────────────────────────────────────────

TickerData ExchangeSession::get_cached_price(const QString& symbol) const {
    QMutexLocker lock(&mutex_);
    return price_cache_.value(symbol);
}

QHash<QString, TickerData> ExchangeSession::snapshot_price_cache() const {
    QMutexLocker lock(&mutex_);
    return price_cache_;
}

} // namespace fincept::trading
