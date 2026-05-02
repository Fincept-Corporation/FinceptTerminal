#pragma once

#include "core/result/Result.h"
#include "datahub/Producer.h"
#include "services/wallet/WalletTypes.h"

#include <QObject>
#include <QString>

#include <functional>

namespace fincept::wallet {

class SolanaRpcClient;

/// Producer for the veFNCPT lock topics (Phase 3 §3.4):
///
///   - `wallet:locks:<pubkey>`     →  `QVector<LockPosition>`  (TTL 60 s)
///   - `wallet:vefncpt:<pubkey>`   →  `VeFncptAggregate`       (TTL 60 s)
///
/// **Real path** (deferred — needs the on-chain Anchor program): query
/// `getProgramAccounts` against the configured `fincept.lock_program_id`,
/// filter by `owner = pubkey`, deserialise position accounts, and publish.
/// The terminal-side surface ships first; the Anchor program lives in
/// `solana/programs/fincept_lock/` (out-of-tree) and lands separately.
///
/// **Mock path** (active until program ID is configured): publishes the
/// 3 demo positions from plan §3.2 — 2,000 @ 4yr, 1,000 @ 1yr, 500 @ 6mo —
/// for any pubkey, with `is_mock=true` on every payload. Lets the dashboard
/// be reviewed before the program ships.
///
/// **Lock / extend / withdraw** transaction-building helpers: in real mode
/// these construct unsigned versioned txs that `WalletService::sign_and_send`
/// can forward to the user's wallet. In mock mode they return
/// `Result::err("…not yet deployed…")` so `LockPanel` can disable the
/// LOCK button with a clear message.
///
/// Duration → veCRV-style multiplier table (4-yr lock = 1.0× the amount):
///   3 mo  → 0.0625
///   6 mo  → 0.125
///   1 yr  → 0.25
///   2 yr  → 0.5
///   4 yr  → 1.0
class StakingService : public QObject, public fincept::datahub::Producer {
    Q_OBJECT
  public:
    static StakingService& instance();

    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override { return 3; }

    /// Duration enum — the only five locks the program accepts. Stored as
    /// seconds in the Anchor program; the multiplier table is derived from
    /// the seconds and a 4-yr max-duration constant.
    enum class Duration {
        ThreeMonths,
        SixMonths,
        OneYear,
        TwoYears,
        FourYears,
    };

    /// Multiplier in [0, 1]. Plan §3.2.
    static double multiplier_for(Duration d) noexcept;

    /// Human-readable label for the duration radio buttons.
    static QString label_for(Duration d);

    /// Seconds count.
    static qint64 seconds_for(Duration d) noexcept;

    /// Build an unsigned `lock` transaction. Async; callback receives the
    /// base64 versioned tx body, or an error if the on-chain program isn't
    /// deployed (mock mode).
    void build_lock_tx(const QString& pubkey,
                       const QString& amount_raw,
                       Duration duration,
                       std::function<void(Result<QString>)> callback);

    /// Build an unsigned `extend` transaction for an existing position.
    void build_extend_tx(const QString& pubkey,
                         const QString& position_id,
                         Duration new_duration,
                         std::function<void(Result<QString>)> callback);

    /// Build an unsigned `withdraw` transaction. Only valid after the
    /// position's `unlock_ts`; the Anchor program rejects early withdrawals.
    void build_withdraw_tx(const QString& pubkey,
                           const QString& position_id,
                           std::function<void(Result<QString>)> callback);

    /// True iff the on-chain program ID is configured. UI uses this to
    /// disable LOCK/EXTEND/WITHDRAW buttons in mock mode and show
    /// "Anchor program not deployed yet" guidance.
    bool program_is_configured() const;

  signals:
    /// Emitted on every successful publish for a pubkey's locks topic.
    /// Used by `RealYieldService` to recompute projections without
    /// pulling them from the hub.
    void locks_published(QString pubkey_b58);

  private:
    explicit StakingService(QObject* parent = nullptr);
    ~StakingService() override;
    StakingService(const StakingService&) = delete;
    StakingService& operator=(const StakingService&) = delete;

    /// `wallet:locks:<pubkey>` → pubkey.
    static QString pubkey_from_locks_topic(const QString& topic);

    /// `wallet:vefncpt:<pubkey>` → pubkey.
    static QString pubkey_from_vefncpt_topic(const QString& topic);

    /// Resolve the Anchor program ID from SecureStorage. Empty string if
    /// not configured; producer falls back to mock data for both topics.
    QString resolve_program_id() const;

    /// Real-path producers — query `getProgramAccounts` and parse positions.
    void refresh_locks_real(const QString& topic, const QString& pubkey,
                            const QString& program_id);
    void refresh_vefncpt_real(const QString& topic, const QString& pubkey,
                              const QString& program_id);

    /// Mock-path producers — publish the 3-position demo from plan §3.2.
    void publish_mock_locks(const QString& topic, const QString& pubkey);
    void publish_mock_vefncpt(const QString& topic, const QString& pubkey);

    SolanaRpcClient* rpc_ = nullptr;
};

} // namespace fincept::wallet
