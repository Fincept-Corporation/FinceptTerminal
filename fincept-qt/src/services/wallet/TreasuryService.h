#pragma once

#include "datahub/Producer.h"
#include "services/wallet/WalletTypes.h"

#include <QObject>
#include <QString>

namespace fincept::wallet {

class SolanaRpcClient;

/// Producer for the terminal-wide treasury reserves & runway topics
/// (Phase 5 §5.3):
///
///   - `treasury:reserves`  →  `TreasuryReserves`  (TTL 5 min)
///   - `treasury:runway`    →  `TreasuryRunway`    (derived from reserves)
///
/// **Source.** When `fincept.treasury_pubkey` is configured in SecureStorage,
/// the service fetches the multisig vault's SOL + USDC balances via our
/// existing `SolanaRpcClient`. When the key is absent, the service publishes
/// a built-in mock payload (`is_mock=true`) so the dashboard renders before
/// the multisig vault is set up.
///
/// **Runway.** Computed as `total_usd / monthly_opex_usd`. The monthly opex
/// is read from SecureStorage (`fincept.treasury_monthly_opex_usd`) — Phase 5
/// keeps it user-configurable until the buyback worker publishes a tracked
/// figure (which would belong on `treasury:reserves` proper, but that
/// requires payroll integration that's out of scope for the dashboard
/// phase).
class TreasuryService : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    explicit TreasuryService(QObject* parent = nullptr);
    ~TreasuryService() override;

    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override { return 2; }

  private:
    /// Fetch the treasury vault's SOL + USDC balances and publish reserves
    /// + derived runway. One round-trip per call.
    void refresh_real(const QString& treasury_pubkey);

    /// Mock paths — `is_mock=true` on every payload.
    void publish_mock_reserves();
    void publish_mock_runway(double total_usd_for_runway);

    /// Compute runway from a reserves total + the configured monthly opex.
    /// Re-publish whenever reserves change.
    void publish_runway(double total_usd, bool is_mock);

    /// USDC mint on Solana mainnet — used to filter
    /// `getTokenAccountsByOwner` to the USDC-only ATA. Constant; pinned
    /// here rather than in WalletTypes because it's only meaningful in the
    /// treasury context for now.
    static QString usdc_mint();

    SolanaRpcClient* rpc_ = nullptr;
};

} // namespace fincept::wallet
