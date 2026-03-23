#pragma once
// Equity Order Entry — BUY/SELL tabs with equity product types (Intraday/Delivery/Margin)

#include "trading/TradingTypes.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

namespace fincept::screens::equity {

class EquityOrderEntry : public QWidget {
    Q_OBJECT
  public:
    explicit EquityOrderEntry(QWidget* parent = nullptr);

    void set_balance(double balance);
    void set_current_price(double price);
    void set_mode(bool is_paper);
    void set_symbol(const QString& symbol);
    void set_exchange(const QString& exchange);

  signals:
    void order_submitted(const trading::UnifiedOrder& order);

  private slots:
    void on_submit();

  private:
    void update_cost_preview();
    void set_buy_side(bool is_buy);
    void set_order_type(int idx);

    // Side tabs
    QPushButton* buy_tab_ = nullptr;
    QPushButton* sell_tab_ = nullptr;

    // Type buttons: MKT, LMT, SL, SL-L
    QPushButton* type_btns_[4] = {};
    int active_type_ = 0;

    // Product type combo: Intraday, Delivery, Margin
    QComboBox* product_combo_ = nullptr;

    // Exchange combo: NSE, BSE, NYSE, etc.
    QComboBox* exchange_combo_ = nullptr;

    // Form
    QLineEdit* qty_edit_ = nullptr;
    QLineEdit* price_edit_ = nullptr;
    QLineEdit* stop_price_edit_ = nullptr;
    QLineEdit* sl_edit_ = nullptr;
    QLineEdit* tp_edit_ = nullptr;
    QPushButton* submit_btn_ = nullptr;
    QWidget* advanced_section_ = nullptr;
    QPushButton* advanced_toggle_ = nullptr;

    // Labels
    QLabel* balance_label_ = nullptr;
    QLabel* market_price_label_ = nullptr;
    QLabel* cost_label_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* mode_label_ = nullptr;

    // State
    double balance_ = 0;
    double current_price_ = 0;
    bool is_paper_ = true;
    bool is_buy_side_ = true;
    QString current_symbol_ = "RELIANCE";
    QString current_exchange_ = "NSE";
};

} // namespace fincept::screens::equity
