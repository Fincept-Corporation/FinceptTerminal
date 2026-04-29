#pragma once

#include "datahub/Producer.h"
#include "services/wallet/WalletTypes.h"

#include <QObject>
#include <QStringList>

class QNetworkAccessManager;

namespace fincept::wallet {

/// Owns the `market:price:token:<mint>` topic family and the legacy
/// `market:price:fncpt` alias. Phase 2 §2A.5 — replaces `FncptPriceProducer`.
///
/// Topic policy:
///   - TTL 60 s, min_interval 30 s, coalesce 250 ms
///   - max_requests_per_sec = 2 (one batched call per refresh cycle is plenty)
///
/// Batching: one Jupiter Lite Price API call covers every mint that has at
/// least one subscriber, regardless of how many topics fire at once. The
/// producer's `refresh(topics)` argument is the union of stale topics; we
/// extract their mints, build `?ids=<m1>,<m2>,…`, and publish per-mint.
class TokenPriceProducer : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    explicit TokenPriceProducer(QObject* parent = nullptr);
    ~TokenPriceProducer() override;

    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override { return 2; }

  private:
    /// Extract the mint from `market:price:token:<mint>` or return the
    /// canonical FNCPT mint for the legacy `market:price:fncpt` alias.
    /// Empty string for anything else.
    static QString mint_for_topic(const QString& topic);

    /// Inverse of `mint_for_topic`. Always returns the new family form.
    static QString topic_for_mint(const QString& mint);

    QNetworkAccessManager* nam_ = nullptr;
};

} // namespace fincept::wallet
