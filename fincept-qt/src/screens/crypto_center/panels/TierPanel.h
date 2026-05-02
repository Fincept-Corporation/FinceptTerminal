#pragma once

#include "services/wallet/WalletTypes.h"

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
/// Three rows:
///   BRONZE   100+ veFNCPT       basic API quota
///   SILVER   1k+ veFNCPT        premium screens
///   GOLD     10k+ veFNCPT       all agents + arena
///
/// Each row shows an [achieved] / [locked] chip on the right, and the
/// current tier's row is highlighted (amber border). Subscribes
/// `billing:tier:<pubkey>`. Empty state (no wallet) shows all rows in
/// "[locked]" with a small "Connect a wallet to see your tier" footnote.
class TierPanel : public QWidget {
    Q_OBJECT
  public:
    explicit TierPanel(QWidget* parent = nullptr);
    ~TierPanel() override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void build_ui();
    void apply_theme();

    void on_wallet_connected(const QString& pubkey, const QString& label);
    void on_wallet_disconnected();
    void on_tier_update(const QVariant& v);

    void render_state(fincept::wallet::TierStatus::Tier current,
                      const QString& weight_ui_str,
                      const QString& next_threshold_ui_str,
                      bool is_mock);

    struct TierRow {
        QFrame* host = nullptr;
        QLabel* label = nullptr;
        QLabel* threshold = nullptr;
        QLabel* unlocks = nullptr;
        QLabel* chip = nullptr;
    };

    // Head
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
