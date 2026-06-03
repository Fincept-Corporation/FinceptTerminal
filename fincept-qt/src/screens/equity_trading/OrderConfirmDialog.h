#pragma once
// OrderConfirmDialog — synchronous "review before send" for manual Semi-Auto
// equity orders. Manual orders are NOT queued in the pending-orders table; the
// trader is already present, so approval is an inline modal rather than the
// status-bar popover used for headless (AI/workflow/broadcast) orders.

#include "trading/TradingTypes.h"

#include <QDialog>
#include <QString>

namespace fincept::screens {

class OrderConfirmDialog : public QDialog {
    Q_OBJECT
  public:
    // Returns true if the user pressed "Send Order". `ref_price` is used only to
    // show an estimated value line (<= 0 hides it).
    static bool confirm(QWidget* parent, const trading::UnifiedOrder& order,
                        const QString& account_label, double ref_price);

  private:
    OrderConfirmDialog(QWidget* parent, const trading::UnifiedOrder& order,
                       const QString& account_label, double ref_price);
};

} // namespace fincept::screens
