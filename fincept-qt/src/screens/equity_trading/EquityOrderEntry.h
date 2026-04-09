#pragma once
// Equity Order Entry — BUY/SELL form configurable per broker profile

#include "trading/BrokerInterface.h"
#include "trading/TradingTypes.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QWidget>

#include <atomic>

namespace fincept::screens::equity {

class EquityOrderEntry : public QWidget {
    Q_OBJECT
  public:
    explicit EquityOrderEntry(QWidget* parent = nullptr);

    // Called when broker changes — rebuilds product/exchange combos, resets state
    void configure_for_broker(const trading::BrokerProfile& profile);

    void set_balance(double balance);
    void set_current_price(double price);
    void set_mode(bool is_paper);
    void set_symbol(const QString& symbol);
    void set_exchange(const QString& exchange);
    void set_broker_id(const QString& broker_id);
    void show_order_status(const QString& msg, bool success); // show result after order placed

  signals:
    void order_submitted(const trading::UnifiedOrder& order);
    void broadcast_requested(const trading::UnifiedOrder& order);

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

    // Product type combo — rebuilt by configure_for_broker()
    QComboBox* product_combo_ = nullptr;

    // Exchange combo — rebuilt by configure_for_broker()
    QComboBox* exchange_combo_ = nullptr;

    // Brokerage info label
    QLabel* brokerage_label_ = nullptr;

    // Form
    QLineEdit* qty_edit_ = nullptr;
    QLineEdit* price_edit_ = nullptr;
    QLineEdit* stop_price_edit_ = nullptr;
    QLineEdit* sl_edit_ = nullptr;
    QLineEdit* tp_edit_ = nullptr;
    QPushButton* submit_btn_ = nullptr;
    QPushButton* broadcast_btn_ = nullptr;
    QWidget* advanced_section_ = nullptr;
    QPushButton* advanced_toggle_ = nullptr;

    // Labels
    QLabel* balance_label_ = nullptr;
    QLabel* market_price_label_ = nullptr;
    QLabel* cost_label_ = nullptr;
    QLabel* margin_label_ = nullptr; // required margin from broker API
    QLabel* status_label_ = nullptr;
    QLabel* mode_label_ = nullptr;

    // State
    double balance_ = 0;
    double current_price_ = 0;
    bool is_paper_ = true;
    bool is_buy_side_ = true;
    QString current_symbol_ = "RELIANCE";
    QString current_exchange_ = "NSE";
    QString current_currency_ = "INR";
    QString broker_id_;

    // Margin fetch debounce timer — fires 500ms after last qty/price change
    QTimer* margin_timer_ = nullptr;
    std::atomic<bool> margin_fetching_{false};

    void fetch_margin_async();

    // Cached product types from profile (for order construction)
    QVector<trading::ProductTypeDef> product_types_;
};

} // namespace fincept::screens::equity
