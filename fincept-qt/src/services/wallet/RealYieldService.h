#pragma once

#include "datahub/Producer.h"
#include "services/wallet/WalletTypes.h"

#include <QObject>
#include <QString>

class QNetworkAccessManager;

namespace fincept::wallet {

/// Producer for the Phase 3 real-yield topics (plan §3.4):
///
///   - `wallet:yield:<pubkey>`  →  `YieldSnapshot`     (TTL 5 min)
///   - `treasury:revenue`       →  `TreasuryRevenue`   (TTL 1 h, terminal-wide)
///
/// **Real path** (deferred): polls a Fincept HTTP endpoint configured via
/// SecureStorage `fincept.yield_endpoint`. Endpoint shape is symmetrical to
/// the Phase 5 buyback worker — `<endpoint>/yield/<pubkey>` for per-pubkey
/// realised yield, `<endpoint>/revenue/current` for the terminal-wide
/// weekly bucket.
///
/// **Mock path** (active until endpoint is configured): publishes derived
/// numbers consistent with the Phase 5 mock buyback ($42,310 / 25 % staker
/// share = $10,577.50 distributed) and the StakingService mock locks
/// (3 demo positions, lifetime $1,296). Same `is_mock=true` discipline.
///
/// Cross-phase reuse: `treasury:revenue` is also consumed by the Phase 5
/// dashboard and any future revenue-driven UI. Bucketed weekly to match
/// the buyback worker's epoch cadence.
class RealYieldService : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    static RealYieldService& instance();

    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override { return 2; }

  private:
    explicit RealYieldService(QObject* parent = nullptr);
    ~RealYieldService() override;
    RealYieldService(const RealYieldService&) = delete;
    RealYieldService& operator=(const RealYieldService&) = delete;

    /// Resolve the configured Fincept yield endpoint, or empty string.
    /// When empty, the service uses `publish_mock_*` paths instead of HTTP.
    QString resolve_endpoint() const;

    /// `wallet:yield:<pubkey>` → pubkey.
    static QString pubkey_from_yield_topic(const QString& topic);

    /// Real-path handlers.
    void refresh_yield_real(const QString& endpoint, const QString& topic,
                            const QString& pubkey);
    void refresh_revenue_real(const QString& endpoint);

    /// Mock-path handlers.
    void publish_mock_yield(const QString& topic, const QString& pubkey);
    void publish_mock_revenue();

    QNetworkAccessManager* nam_ = nullptr;
};

} // namespace fincept::wallet
