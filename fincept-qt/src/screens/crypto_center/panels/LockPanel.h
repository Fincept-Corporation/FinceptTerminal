#pragma once

#include "services/wallet/StakingService.h"
#include "services/wallet/WalletTypes.h"

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
///   - `wallet:vefncpt:<pubkey>`     for current weight (tier preview)
///   - `treasury:revenue`            for EST. YIELD (terminal-wide)
///   - `market:price:fncpt`          for $FNCPT → USD conversion
///   - `billing:tier:<pubkey>`       for current tier
///
/// LOCK click:
///   1. Calls `StakingService::build_lock_tx(...)` — fails fast in mock mode.
///   2. On success, opens `WalletActionConfirmDialog` with decoded summary.
///   3. On confirm, forwards to `WalletService::sign_and_send`.
///   4. Polls `getSignatureStatuses` until confirmed (reuses the SwapPanel
///      pattern; encapsulated in `start_status_poll`).
///
/// In mock mode the LOCK button is disabled with status "DEMO — fincept_lock
/// not deployed yet" and clicking it shows the explanation in the error strip.
class LockPanel : public QWidget {
    Q_OBJECT
  public:
    explicit LockPanel(QWidget* parent = nullptr);
    ~LockPanel() override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    using Duration = fincept::wallet::StakingService::Duration;

    void build_ui();
    void apply_theme();

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
    void show_error_strip(const QString& msg);
    void clear_error_strip();
    void set_busy(bool busy);

    /// Resolve the currently-selected duration from the radio group.
    Duration current_duration() const;

    // UI
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
    int    fncpt_decimals_ = 6;
    double fncpt_usd_price_ = 0.0;
    double weekly_revenue_usd_ = 0.0;
    quint64 current_user_weight_raw_ = 0;
    fincept::wallet::TierStatus::Tier current_tier_ =
        fincept::wallet::TierStatus::Tier::Free;

    bool busy_ = false;
};

} // namespace fincept::screens::panels
