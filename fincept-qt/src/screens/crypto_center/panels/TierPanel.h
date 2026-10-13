#pragma once

#include "services/wallet/WalletTypes.h"

#include <QEvent>
#include <QString>
#include <QVariant>
#include <QWidget>

class QFrame;
class QHideEvent;
class QLabel;
class QShowEvent;

namespace fincept::screens::panels {

/// TierPanel — bottom section of the STAKE tab (Phase 3 §3.2).
///
/// Three rows — tier · veFNCPT threshold · PLANNED benefit:
///   BRONZE   100+ veFNCPT       planned: basic API quota
///   SILVER   1k+ veFNCPT        planned: premium screens
///   GOLD     10k+ veFNCPT       planned: all agents + arena
///
/// The benefits are labelled as planned because nothing in the terminal is
/// gated on a tier: holding or reaching one unlocks nothing in this build.
///
/// While on-chain staking is not live (`StakingService::staking_live()`),
/// there is no veFNCPT weight to derive a tier from, so the panel does not
/// subscribe and shows the program as PLANNED. Once live it subscribes
/// `billing:tier:<pubkey>` (visibility-driven, P3) and marks reached tiers.
class TierPanel : public QWidget {
    Q_OBJECT
  public:
    explicit TierPanel(QWidget* parent = nullptr);
    ~TierPanel() override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void apply_theme();
    void retranslateUi();

    void on_wallet_connected(const QString& pubkey, const QString& label);
    void on_wallet_disconnected();
    void on_tier_update(const QVariant& v);
    void subscribe_if_live();

    /// Static copy (tier names, thresholds, planned benefits).
    void apply_row_texts();
    /// The not-live state: every row PLANNED, explanatory footer.
    void render_planned();
    /// `remaining_ui_str` is the veFNCPT weight still missing for the next tier
    /// (threshold minus current weight), empty at Gold / when unknown.
    void render_state(fincept::wallet::TierStatus::Tier current, const QString& weight_ui_str,
                      const QString& remaining_ui_str);

    struct TierRow {
        QFrame* host = nullptr;
        QLabel* label = nullptr;
        QLabel* threshold = nullptr;
        QLabel* unlocks = nullptr;
        QLabel* chip = nullptr;
    };

    // Head
    QLabel* title_ = nullptr; // "TIER"
    QLabel* current_label_ = nullptr;

    // Body
    TierRow bronze_row_;
    TierRow silver_row_;
    TierRow gold_row_;
    QLabel* footer_ = nullptr;

    // State
    QString current_pubkey_;
    QString current_topic_;
};

} // namespace fincept::screens::panels
