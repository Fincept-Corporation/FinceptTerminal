#pragma once

#include "datahub/Producer.h"
#include "services/wallet/WalletTypes.h"

#include <QObject>
#include <QString>

namespace fincept::billing {

/// Producer for `billing:tier:<pubkey>` (Phase 3 §3.6).
///
/// Like `FeeDiscountService`, this is a **derived producer** — the tier is
/// computed from another hub topic (`wallet:vefncpt:<pubkey>`) rather than
/// fetched from anything external. The service:
///
///   1. Listens to `wallet:vefncpt:*` publishes internally.
///   2. Whenever a veFNCPT aggregate publishes, recomputes the tier from
///      `total_weight_raw` against `TierConfig` thresholds.
///   3. Republishes `billing:tier:<pubkey>` on every weight change.
///
/// The tier is informational only: no screen or service gates a feature on
/// it (the old `tier_changed` signal / `get_cached_tier()` gating hooks had
/// no consumer and were removed). And while on-chain staking is not live
/// (`StakingService::staking_live()`), there is no veFNCPT weight to derive
/// from, so every wallet reads Free.
class TierService : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    static TierService& instance();

    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override { return 0; }

  private:
    explicit TierService(QObject* parent = nullptr);
    ~TierService() override;
    TierService(const TierService&) = delete;
    TierService& operator=(const TierService&) = delete;

    /// `billing:tier:<pubkey>` → pubkey, or "" if invalid.
    static QString pubkey_from_topic(const QString& topic);

    /// `wallet:vefncpt:<pubkey>` for the given pubkey.
    static QString vefncpt_topic_for(const QString& pubkey);

    /// Publish a `TierStatus` for `topic`/`pubkey` by peeking the latest
    /// veFNCPT aggregate from the hub. Idempotent.
    void publish_for(const QString& topic, const QString& pubkey);

    /// Set up cross-subscription so weight changes re-emit the tier
    /// immediately rather than waiting on the hub TTL tick.
    void wire_vefncpt_listener();

    bool listener_wired_ = false;
};

} // namespace fincept::billing
