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
#include <QStringList>

class QCheckBox;
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
    // `price_currency` = the instrument's quote currency as quoted ("USD", "GBp");
    // empty = the exchange's from the venue table (else the broker account's).
    // `price_digits` = price decimals (-1 = from the value). Every price and amount
    // is shown with its currency; the estimated value is also shown converted into
    // the account currency when that differs (with the rate and its time; "—" when
    // no rate is known, never 1:1).
    static bool confirm(QWidget* parent, const trading::UnifiedOrder& order, const QString& account_label,
                        double ref_price, const QString& account_id = QString(),
                        const QString& price_currency = QString(), int price_digits = -1);

    // Simulated (built-in paper) order on delayed prices. Same review, plus what
    // is simulated: `price_note` ("Yahoo Finance · delayed 15 min · last trade
    // 15:29 IST"), `outcome_note` (fills now / queues for the open / rests) and the
    // cash the order books in the account currency (`cash_note`, may be empty).
    // `*skip_next` (optional) is set when the user ticks "don't ask again for paper
    // orders". `price_currency` / `price_digits` as for confirm() (empty = the
    // exchange's from the venue table, else no unit).
    static bool confirm_simulated(QWidget* parent, const trading::UnifiedOrder& order, const QString& account_label,
                                  double ref_price, const QString& price_note, const QString& outcome_note,
                                  const QString& cash_note, bool* skip_next = nullptr,
                                  const QString& price_currency = QString(), int price_digits = -1);

  private:
    OrderConfirmDialog(QWidget* parent, const trading::UnifiedOrder& order, const QString& account_label,
                       double ref_price, const QString& account_id, const QStringList& sim_notes = {},
                       const QString& price_currency = QString(), int price_digits = -1);

    QCheckBox* skip_box_ = nullptr;

    void start_margin_fetch();

    trading::UnifiedOrder order_;
    QString account_id_;
    QString account_currency_; // broker account's currency (margin, converted value); empty = unknown
    QLabel* margin_value_ = nullptr;
    QPushButton* send_btn_ = nullptr;
    bool margin_done_ = false;
};

} // namespace fincept::screens
