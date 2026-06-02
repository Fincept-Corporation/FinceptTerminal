#pragma once
// ExchangeSessionManager — registry of live ExchangeSession objects.
//
// Lazy-creates one ExchangeSession per exchange id on demand. Sessions stay
// warm for the application lifetime (Fincept-style) — switching exchanges
// does not tear down the previous session's WS stream. This buys sub-500ms
// exchange switches because the daemon + WS handshake were already paid.
//
// This class (not the sessions) is the DataHub `Producer` for the whole
// `ws:*:*` family. When a session has fan-out data, it hands it to the
// manager via the `SessionPublisher` seam; the manager applies hub policies
// and publishes. Keeps topic semantics in one place.

#include "datahub/Producer.h"
#include "trading/ExchangeSession.h"

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>

namespace fincept::trading {

class ExchangeSessionManager : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    static ExchangeSessionManager& instance();

    /// Register with DataHub + install `ws:*:*` policies for supported
    /// exchanges. Idempotent.
    void ensure_registered_with_hub();

    /// Lazily create (if needed) and return the session for `exchange_id`.
    /// Thread-safe; the session outlives the caller's reference (owned by
    /// the manager).
    ExchangeSession* session(const QString& exchange_id);

    /// Snapshot of currently-live exchange ids.
    QStringList active_exchange_ids() const;

    /// Canonical list of crypto exchanges wired into the terminal — the single
    /// source of truth for the Crypto Trading dropdown, the hub allow-list,
    /// `topic_patterns()`, and the per-exchange hub policies. Display order ==
    /// list order (native Kraken + Hyperliquid DEX first, then majors).
    /// Adding an exchange is a one-line edit here; the daemon (ccxt.pro
    /// `ws_stream.py` + `exchange_daemon.py`) already handles any ccxt id.
    static const QStringList& supported_exchange_ids();

    // ── Producer ───────────────────────────────────────────────────────────
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override; // no-op: push_only
    int max_requests_per_sec() const override;         // 0 (unlimited: WS)

    ExchangeSessionManager(const ExchangeSessionManager&) = delete;
    ExchangeSessionManager& operator=(const ExchangeSessionManager&) = delete;

  private:
    ExchangeSessionManager();
    ~ExchangeSessionManager() override;

    // Publisher callbacks injected into each session. Applies per-topic policy
    // gating so unsupported exchanges simply drop to the floor.
    SessionPublisher build_publisher();

    bool hub_registered_ = false;

    mutable QMutex mutex_;
    // Owning pointers. Sessions are parented to `this` (the manager) so Qt
    // parent/child cleanup reaps them at manager destruction if the map
    // isn't explicitly cleared first.
    QHash<QString, ExchangeSession*> sessions_;
};

} // namespace fincept::trading
