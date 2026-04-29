#pragma once

#include "datahub/Producer.h"

#include <QObject>
#include <QString>

namespace fincept::billing {

/// Producer for `billing:fncpt_discount:<pubkey>`.
///
/// Phase 2 §2C — derives eligibility from `wallet:balance:<pubkey>` rather
/// than fetching anything external. When the hub asks us to refresh a topic,
/// we peek the corresponding balance topic, compute `eligible = fncpt_ui >=
/// threshold`, and publish a `FncptDiscount`. The service also subscribes
/// to balance updates so eligibility flips re-publish immediately, not
/// only on hub TTL ticks.
///
/// Per CLAUDE.md D2 this is a real Producer because it owns a topic family.
/// The "no external fetch" property just means `refresh()` is cheap — no
/// HTTP, no cadence pacing required.
class FeeDiscountService : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    explicit FeeDiscountService(QObject* parent = nullptr);
    ~FeeDiscountService() override;

    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override { return 0; }

  private:
    /// `billing:fncpt_discount:<pubkey>` → pubkey, or "" if invalid.
    static QString pubkey_from_topic(const QString& topic);

    /// `wallet:balance:<pubkey>` for the given pubkey.
    static QString balance_topic_for(const QString& pubkey);

    /// Compute + publish the FncptDiscount payload by peeking the latest
    /// balance from the hub. Idempotent — safe to call repeatedly.
    void publish_for(const QString& topic, const QString& pubkey);

    /// Set up cross-subscriptions so balance changes re-emit eligibility
    /// immediately rather than waiting on the hub TTL tick.
    void wire_balance_listener();

    bool listener_wired_ = false;
};

} // namespace fincept::billing
