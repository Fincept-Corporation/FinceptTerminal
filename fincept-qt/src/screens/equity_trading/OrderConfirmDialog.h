#pragma once
// OrderConfirmDialog — synchronous "review before send" for manual Semi-Auto
// equity orders. Manual orders are NOT queued in the pending-orders table; the
// trader is already present, so approval is an inline modal rather than the
// status-bar popover used for headless (AI/workflow/broadcast) orders.
//
// Safety contract (mirrors screens/fno/OrderConfirmDialog):
//   • every field that reaches the broker is shown, including product type,
//     validity and the protective SL/TP legs — nothing silently rides along;
//   • the destination account is shown with an explicit LIVE / PAPER chip;
//   • the required margin is fetched in the background (live accounts) with a
//     hard timeout so a slow broker never blocks the trader;
//   • a limit price far away from the reference LTP raises a fat-finger banner;
//   • CANCEL is the default button — Enter can never send an order.

#include "trading/TradingTypes.h"

#include <QDialog>
#include <QString>

class QLabel;
class QPushButton;

namespace fincept::screens {

class OrderConfirmDialog : public QDialog {
    Q_OBJECT
  public:
    // Returns true if the user pressed "SEND ORDER". `ref_price` is the latest
    // known LTP (<= 0 = unknown) and drives the estimated value + the
    // fat-finger check. `account_id` is optional; when given, the dialog reads
    // the account's live/paper mode and fetches the broker's required margin.
    static bool confirm(QWidget* parent, const trading::UnifiedOrder& order, const QString& account_label,
                        double ref_price, const QString& account_id = QString());

  private:
    OrderConfirmDialog(QWidget* parent, const trading::UnifiedOrder& order, const QString& account_label,
                       double ref_price, const QString& account_id);

    void start_margin_fetch();

    trading::UnifiedOrder order_;
    QString account_id_;
    QLabel* margin_value_ = nullptr;
    QPushButton* send_btn_ = nullptr;
    bool margin_done_ = false;
};

} // namespace fincept::screens
