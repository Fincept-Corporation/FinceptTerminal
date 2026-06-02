#pragma once

#include "services/wallet/WalletTypes.h"

#include <QEvent>
#include <QString>
#include <QVariant>
#include <QWidget>

class QHideEvent;
class QLabel;
class QProgressBar;
class QShowEvent;

namespace fincept::screens::panels {

/// FeeDiscountPanel — sits on TradeTab; shows the user's eligibility for the
/// $FNCPT fee discount and the projected savings on a reference SKU.
///
/// Subscribes to two topics (visibility-driven, P3 + D3):
///   - `wallet:balance:<pubkey>`        for the held FNCPT amount
///   - `billing:fncpt_discount:<pubkey>` for the eligibility flag + threshold
///
/// The two are decoupled so the panel can render the progress bar even
/// before the discount producer's first publish: balance → bar fill,
/// discount → eligibility chip + applied-SKU list.
class FeeDiscountPanel : public QWidget {
    Q_OBJECT
  public:
    explicit FeeDiscountPanel(QWidget* parent = nullptr);
    ~FeeDiscountPanel() override;

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
    void on_balance_update(const QVariant& v);
    void on_discount_update(const QVariant& v);

    void refresh_subscriptions();
    void update_view();

    QLabel* title_ = nullptr;             // "FEE DISCOUNT"
    QLabel* holding_caption_ = nullptr;   // "HOLDING"
    QLabel* threshold_caption_ = nullptr; // "THRESHOLD"
    QLabel* skus_caption_ = nullptr;      // "APPLIED TO"
    QLabel* savings_caption_ = nullptr;   // "PROJECTED SAVINGS …"
    QLabel* heading_status_ = nullptr;
    QLabel* balance_value_ = nullptr;
    QLabel* threshold_value_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QLabel* skus_value_ = nullptr;
    QLabel* savings_value_ = nullptr;
    QLabel* hint_ = nullptr;

    QString current_pubkey_;
    QString balance_topic_;
    QString discount_topic_;
    double  fncpt_held_ = 0.0;
    fincept::wallet::FncptDiscount latest_;
    bool    have_discount_ = false;
};

} // namespace fincept::screens::panels
