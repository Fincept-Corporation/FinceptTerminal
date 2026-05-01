#pragma once

#include "datahub/Producer.h"
#include "services/wallet/WalletTypes.h"

#include <QObject>
#include <QString>

class QNetworkAccessManager;

namespace fincept::wallet {

/// Producer for the terminal-wide buyback & burn dashboard topics
/// (Phase 5 §5.3):
///
///   - `treasury:buyback_epoch`   →  `BuybackEpoch`           (TTL 60 s)
///   - `treasury:burn_total`      →  `BurnTotal`              (TTL 5 min)
///   - `treasury:supply_history`  →  `QVector<SupplyHistoryPoint>` (TTL 1 h)
///
/// **No on-chain code.** The Fincept-operated buyback worker
/// (`services/buyback-worker/`) tallies revenue, executes Jupiter buys,
/// burns the bought $FNCPT via SPL Token `burn_checked` from the treasury
/// account, then publishes per-epoch summaries to a Fincept HTTP endpoint
/// configured via SecureStorage `fincept.treasury_endpoint`.
///
/// **Mock mode.** When `fincept.treasury_endpoint` is unset, the service
/// publishes a built-in mock payload (flagged via `is_mock=true` on every
/// POD) so the dashboard renders before the worker is deployed. This is
/// the explicit "demo before infra" path called out in the Phase 5 plan.
class BuybackBurnService : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    explicit BuybackBurnService(QObject* parent = nullptr);
    ~BuybackBurnService() override;

    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;

    /// Treasury endpoint dispatches three round-trips per refresh — each
    /// topic family in the pattern list. Cap at 3/s so a flurry of
    /// re-subscribes doesn't hammer.
    int max_requests_per_sec() const override { return 3; }

  private:
    /// Resolve the configured Fincept treasury endpoint, or empty string.
    /// When empty, the service uses `publish_mock_*` paths instead of HTTP.
    QString resolve_endpoint() const;

    /// Per-topic-family fetch handlers. Each builds a GET against
    /// `<endpoint>/<path>`, parses the JSON response, and calls
    /// `DataHub::publish` (or `publish_error` on failure).
    void refresh_buyback_epoch(const QString& endpoint);
    void refresh_burn_total(const QString& endpoint);
    void refresh_supply_history(const QString& endpoint);

    /// Mock-mode publishers — used when the endpoint isn't configured.
    /// All payloads carry `is_mock=true` so the UI can show a chip.
    void publish_mock_buyback_epoch();
    void publish_mock_burn_total();
    void publish_mock_supply_history();

    QNetworkAccessManager* nam_ = nullptr;
};

} // namespace fincept::wallet
