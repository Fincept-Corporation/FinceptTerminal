#pragma once

#include "core/result/Result.h"
#include "datahub/Producer.h"
#include "services/wallet/WalletTypes.h"

#include <QObject>
#include <QString>

#include <functional>

namespace fincept::wallet {

/// Producer for the veFNCPT lock topics (Phase 3 §3.4):
///
///   - `wallet:locks:<pubkey>`     →  `QVector<LockPosition>`  (TTL 60 s)
///   - `wallet:vefncpt:<pubkey>`   →  `VeFncptAggregate`       (TTL 60 s)
///
/// **On-chain staking is not live in this build.** The `fincept_lock` Anchor
/// program is not deployed, the position-account decoder is not written and
/// the lock / extend / withdraw transaction builders do not exist. So:
///
///   - `staking_live()` is false; the STAKE tab renders a "coming soon" state
///     and never subscribes to the lock topics.
///   - `refresh()` publishes an explicit "not live" error for both topics (for
///     any other consumer, e.g. hub introspection) — it never publishes data.
///     It used to publish three fabricated positions (2,000 @ 4 yr, ...) for
///     whatever wallet was connected, which the tier system then treated as
///     that wallet's real weight.
///   - `build_lock_tx()` fails with the same "not live" reason.
///
/// The duration → multiplier table (veCRV-style, 4-yr lock = 1.0×) is the
/// planned program's and is exposed for the lock calculator:
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

    /// Duration enum — the only five locks the planned program accepts.
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

    /// False in this build: lock positions can neither be read nor created
    /// (see the class comment). UI gates every staking action and every
    /// staking-derived number on this.
    bool staking_live() const;

    /// User-facing reason staking is unavailable. Shown verbatim by the panels.
    static QString not_live_reason();

    /// Build an unsigned `lock` transaction. Async; the callback receives the
    /// base64 versioned tx body — or, in this build, the "not live" error.
    void build_lock_tx(const QString& pubkey, const QString& amount_raw, Duration duration,
                       std::function<void(Result<QString>)> callback);

  private:
    explicit StakingService(QObject* parent = nullptr);
    ~StakingService() override;
    StakingService(const StakingService&) = delete;
    StakingService& operator=(const StakingService&) = delete;
};

} // namespace fincept::wallet
