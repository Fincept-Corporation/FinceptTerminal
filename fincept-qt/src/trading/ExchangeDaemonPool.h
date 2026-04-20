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
    /// running.
    void start();

    /// Graceful shutdown — closes stdin, kills after 2s if still alive.
    void stop();

    /// Blocking wait on a worker thread. Returns true if daemon is ready
    /// before `timeout_ms`. NEVER call from the UI thread (P1) — this blocks.
    bool wait_for_ready(int timeout_ms = 8000);

    /// Snapshot state query.
    bool is_ready() const { return ready_.load(); }

    /// Synchronous RPC. Call from a worker thread only (may block up to
    /// `timeout_ms`). Returns the daemon's reply object verbatim — caller
    /// checks `success`/`error` fields per the daemon protocol.
    ///
    /// `exchange` is the ccxt id (e.g. "kraken"). `credentials`, if set, are
    /// sent to the daemon once per `(exchange, credentials-fingerprint)`
    /// pair — passing the same creds on later calls is free.
    QJsonObject call(const QString& exchange,
                     const QString& method,
                     const QJsonObject& args,
                     const ExchangeCredentials& credentials = {},
                     int timeout_ms = 15000);

    /// Drop cached credential-sent state for a given exchange — use this
    /// after an API-key rotation so the next `call()` re-sends creds.
    void forget_credentials(const QString& exchange);

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
    static QString credential_fingerprint(const ExchangeCredentials& creds);

    QProcess* process_ = nullptr;
    std::atomic<bool> ready_{false};
    std::atomic<int> next_req_id_{0};

    QMutex mutex_;                               // guards responses_ + creds_sent_
    QWaitCondition response_ready_;              // signalled when any response arrives
    QHash<QString, QJsonObject> responses_;      // id -> full response obj
    QHash<QString, QString> creds_sent_;         // exchange_id -> fingerprint of last-sent creds
};

} // namespace fincept::trading
