#pragma once

#include "datahub/Producer.h"
#include "services/wallet/WalletTypes.h"

#include <QObject>
#include <QString>

namespace fincept::billing {

/// Producer for `billing:tier:<pubkey>` (Phase 3 §3.6).
///
/// Like `FeeDiscountService`, this is a **derived producer** — eligibility
/// is computed from another hub topic (`wallet:vefncpt:<pubkey>`) rather
/// than fetched from anything external. The service:
///
///   1. Subscribes to `wallet:vefncpt:*` internally.
///   2. Whenever a veFNCPT aggregate publishes, recomputes the tier from
///      `total_weight_raw` against `TierConfig` thresholds.
///   3. Republishes `billing:tier:<pubkey>` on every weight change.
///
/// Cross-screen gating: emits `tier_changed(pubkey, tier)` so paid screens
/// (AI Quant Lab, Alpha Arena, etc.) can react without polling the hub.
/// `get_cached_tier(pubkey)` provides a synchronous lookup for nav-level
/// gating that runs before the hub has emitted.
class TierService : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    static TierService& instance();

    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override { return 0; }

    /// Synchronous tier lookup. Returns `Free` when no veFNCPT data is
    /// cached yet — paid screens should treat that as "not yet eligible"
    /// and re-check on the `tier_changed` signal.
    fincept::wallet::TierStatus::Tier get_cached_tier(const QString& pubkey) const;

  signals:
    /// Fires whenever the published tier for a pubkey changes. Cross-screen
    /// gates connect to this and re-evaluate.
    void tier_changed(QString pubkey, int new_tier);

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

    /// Set up cross-subscription so weight changes re-emit eligibility
    /// immediately rather than waiting on the hub TTL tick.
    void wire_vefncpt_listener();

    bool listener_wired_ = false;
};

} // namespace fincept::billing
