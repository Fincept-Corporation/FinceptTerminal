#pragma once
// ExchangeDaemonPool — shared persistent ccxt daemon (Phase 1 of multi-broker refactor).
//
// Owns the single long-running `exchange_daemon.py` subprocess that serves
// ccxt requests for every exchange id. Keeping one daemon (not one per
// exchange) matches the daemon's own design — each JSON request carries an
// `"exchange"` field, and the daemon reuses the imported `ccxt` module across
// calls, paying the 600–1200ms import cost exactly once per app run.
//
// This class is intentionally exchange-agnostic: callers pass the exchange
// id and credentials per call, so multiple `ExchangeSession` instances can
// share one daemon without stepping on each other.
//
// Threading model: the `QProcess` lives on the thread that created the
// singleton (the main thread via `instance()`). Worker threads call `call()`
// synchronously; writes are marshalled to the owner thread via
// `QMetaObject::invokeMethod(Qt::QueuedConnection)` and responses are
// delivered back through a `QWaitCondition`.

#include "trading/TradingTypes.h"

#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QProcess>
#include <QWaitCondition>

#include <atomic>

namespace fincept::trading {

class ExchangeDaemonPool : public QObject {
    Q_OBJECT
  public:
    static ExchangeDaemonPool& instance();

    /// Non-blocking start. Caller may hook `ready()` or call `wait_for_ready()`
    /// from a worker thread. Safe to call multiple times — no-op if already
    /// running. An explicit start() also re-arms a daemon that was given up on
    /// (see is_unavailable()); the automatic paths never do.
    ///
    /// Restart policy (automatic paths: the first warm-up and wait_for_ready()):
    /// a daemon that dies is started again on demand, after a back-off that
    /// doubles from 2 s to a 30 s cap. A run that reached `__init__` and stayed
    /// up for 60 s resets the count. After 6 consecutive deaths without such a
    /// run the pool stops restarting: call() answers at once with
    /// code "DAEMON_NOT_READY" (nothing was sent) and the reason.
    void start();

    /// Graceful shutdown — closes stdin, kills after 2s if still alive.
    void stop();

    /// Blocking wait on a worker thread. Returns true if daemon is ready
    /// before `timeout_ms`. NEVER call from the UI thread (P1) — this blocks.
    bool wait_for_ready(int timeout_ms = 8000);

    /// Snapshot state query.
    bool is_ready() const { return ready_.load(); }

    /// True once the pool gave up restarting a daemon that keeps dying at start.
    bool is_unavailable() const { return gave_up_.load(); }

    /// Synchronous RPC. Call from a worker thread only (may block up to
    /// `timeout_ms`). Returns the daemon's reply object verbatim — caller
    /// checks `success`/`error` fields per the daemon protocol. Failures made
    /// here carry a `code` too: "DAEMON_NOT_READY" (nothing was sent) and
    /// "TIMEOUT" (no reply within `timeout_ms`; the request may still complete,
    /// so for an order the outcome is unknown, not failed).
    ///
    /// `exchange` is the ccxt id (e.g. "kraken"). `credentials`, if set, are
    /// sent to the daemon once per `(exchange, credentials-fingerprint)`
    /// pair — passing the same creds on later calls is free.
    QJsonObject call(const QString& exchange, const QString& method, const QJsonObject& args,
                     const ExchangeCredentials& credentials = {}, int timeout_ms = 15000);

    /// Drop cached credential-sent state for a given exchange — use this
    /// after an API-key rotation so the next `call()` re-sends creds (or, if the
    /// caller's credentials are now empty, tells the daemon to drop its copy).
    void forget_credentials(const QString& exchange);

    /// Credentials for `exchange` were cleared: tell the daemon NOW to forget its
    /// copy and its authenticated client, so no further authenticated request
    /// (order, cancel, leverage, balance) can run on the old keys. Any thread.
    void clear_credentials(const QString& exchange);

    /// Identity of a credential set: the first 16 hex chars of
    /// SHA-256(api_key|secret|password|wallet|private_key); empty for no
    /// credentials. The daemon computes the same value
    /// (exchange_client.credential_fingerprint) and refuses an order whose
    /// confirmed account no longer matches the keys it holds.
    static QString credential_fingerprint(const ExchangeCredentials& creds);

    ExchangeDaemonPool(const ExchangeDaemonPool&) = delete;
    ExchangeDaemonPool& operator=(const ExchangeDaemonPool&) = delete;

  signals:
    /// Emitted (from the owner thread) when the daemon's `__init__` line
    /// arrives. Useful for screens that want to fire opening fetches only
    /// after the daemon is warm.
    void ready();

  private:
    ExchangeDaemonPool();
    ~ExchangeDaemonPool();

    void drain_buffer();
    void send_credentials_if_needed(const QString& exchange, const ExchangeCredentials& creds);
    /// start() for the automatic paths: honours the back-off (schedules the start
    /// when it is early) and does nothing once the pool gave up. Owner thread.
    void start_when_allowed();
    /// Spawn the daemon. Owner thread.
    void launch();
    /// A daemon this pool did not stop on purpose has gone: count it, set the
    /// back-off or give up, and log the change. Owner thread.
    void note_daemon_lost(const QString& why);
    /// The error call() returns while no daemon can serve it.
    QString unavailable_text();

    QProcess* process_ = nullptr;
    std::atomic<bool> ready_{false};

    // Restart policy — owner thread, except gave_up_ (atomic) and last_failure_ (mutex_).
    int quick_failures_ = 0;          // consecutive deaths without a healthy run
    int backoff_ms_ = 0;              // wait after the last death before the next start
    QElapsedTimer since_failure_;     // started at the last death
    QElapsedTimer since_spawn_;       // started when the current daemon was launched
    bool ready_seen_ = false;         // the current daemon sent __init__
    bool start_scheduled_ = false;    // a back-off start timer is pending
    QString last_stderr_;             // last stderr line of the current daemon (for the reason)
    std::atomic<bool> gave_up_{false};
    QString last_failure_;            // guarded by mutex_
    std::atomic<int> next_req_id_{0};

    QMutex mutex_;                          // guards responses_ + creds_sent_
    QWaitCondition response_ready_;         // signalled when any response arrives
    QHash<QString, QJsonObject> responses_; // id -> full response obj
    QHash<QString, QString> creds_sent_;    // exchange_id -> fingerprint of last-sent creds
};

} // namespace fincept::trading
