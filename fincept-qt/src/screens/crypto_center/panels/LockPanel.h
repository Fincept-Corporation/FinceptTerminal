#pragma once

#include "services/wallet/StakingService.h"
#include "services/wallet/WalletTypes.h"

#include <QEvent>
#include <QString>
#include <QVariant>
#include <QWidget>

class QButtonGroup;
class QFrame;
class QHideEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QShowEvent;

namespace fincept::screens::panels {

/// LockPanel — top section of the STAKE tab (Phase 3 §3.2).
///
/// Plan ASCII spec:
///   AMOUNT     [   1,000 ] $FNCPT       AVAILABLE   12,304
///   DURATION   ◯ 3 MO  ◯ 6 MO  ● 1 YR  ◯ 2 YR  ◯ 4 YR
///   WEIGHT     1,000 veFNCPT × 0.25 (1 yr) = 250 veFNCPT
///   EST. YIELD $42 / week (USDC) — 2.18% weekly real yield at $341 stake
///   TIER       Silver → Gold (after lock)
///   [PREVIEW LOCK]            [LOCK]
///
/// Subscribes (visibility-driven, P3):
///   - `wallet:balance:<pubkey>`     for AVAILABLE
///   - `market:price:fncpt`          for $FNCPT → USD conversion
/// and, only while `StakingService::staking_live()`:
///   - `wallet:vefncpt:<pubkey>`     for current weight (tier preview)
///   - `treasury:revenue`            for EST. YIELD (terminal-wide)
///   - `billing:tier:<pubkey>`       for current tier
///
/// While staking is not live (this build) the panel is a calculator: WEIGHT
/// and the planned tier for the typed amount are shown, EST. YIELD reads
/// "n/a", and LOCK is disabled and labelled COMING SOON — no number on the
/// panel is presented as the wallet's own staking position.
///
/// LOCK click (live mode):
///   1. Calls `StakingService::build_lock_tx(...)`.
///   2. On success, opens `WalletActionConfirmDialog` with decoded summary.
///   3. On confirm, forwards to `WalletService::sign_and_send`.
///   4. Polls `getSignatureStatuses` (`start_status_poll`, same cadence as
///      `SwapPanel`) until the lock is confirmed, reverted on-chain, or the
///      attempt cap is hit. The panel stays busy for the duration so a second
///      lock can't be queued behind an unconfirmed one; on confirmation the
///      balance and the locks / veFNCPT topics are force-refreshed.
class LockPanel : public QWidget {
    Q_OBJECT
  public:
    explicit LockPanel(QWidget* parent = nullptr);
    ~LockPanel() override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* event) override;

  private:
    using Duration = fincept::wallet::StakingService::Duration;

    void build_ui();
    void apply_theme();
    void retranslateUi();

    void on_wallet_connected(const QString& pubkey, const QString& label);
    void on_wallet_disconnected();
    void on_balance_update(const QVariant& v);
    void on_vefncpt_update(const QVariant& v);
    void on_revenue_update(const QVariant& v);
    void on_price_update(const QVariant& v);
    void on_tier_update(const QVariant& v);
    void on_amount_changed(const QString& s);
    void on_duration_changed(int id);
    void on_max_clicked();
    void on_lock_clicked();

    void resubscribe();
    void recompute_preview();
    /// Poll `getSignatureStatuses` after `sign_and_send` until the lock tx is
    /// confirmed, reverted, or the attempt cap is reached. Timer and RPC client
    /// are parented to the panel and torn down on every terminal outcome.
    void start_status_poll(const QString& sig);
    void show_error_strip(const QString& msg);
    void clear_error_strip();
    void set_busy(bool busy);

    /// Resolve the currently-selected duration from the radio group.
    Duration current_duration() const;

    // UI
    QLabel* head_title_ = nullptr;        // "STAKE / LOCK"
    QLabel* head_subtitle_ = nullptr;     // "veFNCPT — locked $FNCPT earns USDC yield"
    QLabel* amount_caption_ = nullptr;    // "AMOUNT"
    QLabel* token_caption_ = nullptr;     // "TOKEN"
    QLabel* token_chip_ = nullptr;        // "$FNCPT"
    QLabel* duration_caption_ = nullptr;  // "DURATION"
    QLabel* weight_caption_ = nullptr;    // "WEIGHT"
    QLabel* est_yield_caption_ = nullptr; // "EST. YIELD"
    QLabel* tier_caption_ = nullptr;      // "TIER"
    QLineEdit* amount_input_ = nullptr;
    QLabel* available_label_ = nullptr;
    QPushButton* max_button_ = nullptr;
    QButtonGroup* duration_group_ = nullptr;
    QRadioButton* dur_3mo_ = nullptr;
    QRadioButton* dur_6mo_ = nullptr;
    QRadioButton* dur_1yr_ = nullptr;
    QRadioButton* dur_2yr_ = nullptr;
    QRadioButton* dur_4yr_ = nullptr;

    QLabel* weight_calc_ = nullptr;
    QLabel* est_yield_ = nullptr;
    QLabel* tier_preview_ = nullptr;

    QFrame* error_strip_ = nullptr;
    QLabel* error_text_ = nullptr;
    QPushButton* lock_button_ = nullptr;
    QLabel* status_label_ = nullptr;

    // Hub state — last-seen values, used during preview math.
    QString current_pubkey_;
    QString current_balance_topic_;
    QString current_vefncpt_topic_;
    QString current_tier_topic_;
    double fncpt_balance_ui_ = 0.0;
    int fncpt_decimals_ = 6;
    double fncpt_usd_price_ = 0.0;
    double weekly_revenue_usd_ = 0.0;
    /// True when the `treasury:revenue` payload the yield projection is built
    /// from is the producer's built-in mock (worker endpoint unconfigured).
    /// Drives the "DEMO" prefix on the EST. YIELD line — a fabricated revenue
    /// figure must never be rendered as if it were a real projection.
    bool revenue_is_mock_ = false;
    quint64 current_user_weight_raw_ = 0;
    fincept::wallet::TierStatus::Tier current_tier_ = fincept::wallet::TierStatus::Tier::Free;

    bool busy_ = false;
};

} // namespace fincept::screens::panels
