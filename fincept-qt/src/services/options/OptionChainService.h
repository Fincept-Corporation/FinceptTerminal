#pragma once
// OptionChainService — DataHub Producer for `option:chain:*` and friends.
//
// Phase 1 scope:
//   - Owns the option chain assembly path. Reads instruments from
//     InstrumentService, fetches quotes via the broker's IBroker::get_quotes,
//     enriches each strike with OI/IV/Greeks, and publishes the assembled
//     OptionChain on `option:chain:<broker>:<underlying>:<expiry>`.
//   - Greeks/IV computation is stubbed in Phase 1 (returns 0). Wired to the
//     PythonWorker `option_greeks_batch` action in Phase 3.
//   - WebSocket OI fan-out is wired in Phase 3; Phase 1 ships polled
//     refresh through DataHub.
//
// The service is a singleton so it can be registered from main.cpp and
// looked up from any consumer screen. It DOES NOT spawn Python directly
// (D1) — Greeks routing goes through PythonWorker. It is the sole hub
// producer for the option:* topic family (D2).
//
// Not thread-safe in the API surface; all entry points must be called
// from the UI thread. Internal QtConcurrent work is marshalled back via
// QMetaObject::invokeMethod / QPointer guards.

#include "datahub/Producer.h"
#include "services/options/OptionChainTypes.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::services::options {

class OptionChainService : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    static OptionChainService& instance();

    /// Idempotent — registers as Producer for the option:* topic families and
    /// installs default policies. Called from main.cpp after datahub
    /// metatype registration.
    void ensure_registered_with_hub();

    // ── fincept::datahub::Producer ──────────────────────────────────────────
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override;

    // ── Consumer-facing helpers ─────────────────────────────────────────────

    /// One-shot read of the current cached chain, or std::nullopt when no
    /// snapshot exists. Triggers no fetch — wrapper around DataHub::peek.
    /// Streaming consumers should subscribe to the topic instead.
    QString chain_topic(const QString& broker_id, const QString& underlying, const QString& expiry) const;

    /// List underlyings + expiries from InstrumentService for picker UIs.
    QStringList list_underlyings(const QString& broker_id) const;
    QStringList list_expiries(const QString& broker_id, const QString& underlying) const;

  signals:
    /// Emitted alongside the hub publish so callers can connect via Qt
    /// signals if they prefer that to subscribing on the hub.
    void chain_published(const fincept::services::options::OptionChain& chain);
    void chain_failed(const QString& topic, const QString& error);

  private:
    OptionChainService();
    OptionChainService(const OptionChainService&) = delete;
    OptionChainService& operator=(const OptionChainService&) = delete;

    /// Decompose a topic of form `option:chain:<broker>:<underlying>:<expiry>`
    /// into its three keys. Returns false when the topic doesn't parse.
    static bool parse_chain_topic(const QString& topic, QString& broker, QString& underlying, QString& expiry);

    /// Drive a single chain refresh — runs broker->get_quotes() on a worker
    /// thread, assembles rows, computes derived stats (PCR, max pain, ATM),
    /// then publishes the result on the hub from the UI thread.
    void refresh_chain(const QString& broker_id, const QString& underlying, const QString& expiry);

    /// Compute max pain strike from a fully-populated chain.
    static double compute_max_pain(const QVector<OptionChainRow>& rows);
    /// Compute PCR = sum(PE OI) / sum(CE OI), or 0 when total CE OI is 0.
    static double compute_pcr(const QVector<OptionChainRow>& rows, double& total_ce_oi, double& total_pe_oi);
    /// Strike with smallest |strike - spot|.
    static double compute_atm(const QVector<OptionChainRow>& rows, double spot);

    /// Per-token last-known spot lookup. Populated from the hub
    /// (DataHub::peek("market:quote:<sym>")) on demand. Falls back to the
    /// broker's get_quotes for the underlying on first call per session.
    double resolve_spot(const QString& broker_id, const QString& underlying);

    bool hub_registered_ = false;
    /// In-flight guard per topic to avoid duplicate refresh fan-out when the
    /// hub scheduler races with a manual request.
    QHash<QString, bool> in_flight_;
    /// Last spot snapshot per (broker, underlying) — refreshed each tick.
    QHash<QString, double> spot_cache_;
};

} // namespace fincept::services::options
