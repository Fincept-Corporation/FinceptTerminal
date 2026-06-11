#pragma once
// Equity Order Entry — BUY/SELL form configurable per broker profile

#include "trading/BrokerInterface.h"
#include "trading/OptionsStrategyBuilder.h"
#include "trading/TradingTypes.h"

#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QTimer>
#include <QWidget>

#include <atomic>

class QMenu;
class QToolButton;

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
    /// Emitted instead of order_submitted when the inline BROKERS selector has
    /// ≥1 account checked — the same order goes to every selected account.
    void multi_broker_submit(const trading::UnifiedOrder& order, const QStringList& account_ids);
    void strategy_order_submitted(const trading::BasketOrderRequest& basket);

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_submit();
    void on_place_strategy();

  private:
    void update_cost_preview();
    void retranslateUi();
    void set_buy_side(bool is_buy);
    void set_order_type(int idx);

    // Resolve the current form selections into trading enums (dedups the
    // order-type / product-type mapping repeated across submit/broadcast/margin).
    trading::OrderType selected_order_type() const;
    trading::ProductType selected_product_type() const;

    // Inline multi-broker selector (QWidgetAction checkboxes keep the menu open).
    void rebuild_brokers_menu();
    void update_brokers_btn();

    // Options strategy helpers
    bool build_strategy(trading::OptionsStrategy& out) const; // returns false when strategy == None or inputs invalid
    void refresh_strategy_preview();

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
    QToolButton* brokers_btn_ = nullptr;  // inline multi-broker selector
    QMenu* brokers_menu_ = nullptr;       // checkable account list (stays open)
    QSet<QString> broadcast_ids_;         // checked account_ids; empty = focused only
    QWidget* advanced_section_ = nullptr;
    QPushButton* advanced_toggle_ = nullptr;

    // Options strategy section (collapsible, like advanced_section_)
    QPushButton* strategy_toggle_ = nullptr;
    QWidget* strategy_section_ = nullptr;
    QComboBox* strategy_combo_ = nullptr;
    QLineEdit* strat_strike_edit_ = nullptr; // ATM strike
    QLineEdit* strat_expiry_edit_ = nullptr; // YYYY-MM-DD
    QLineEdit* strat_lot_edit_ = nullptr;    // lot size (default 1)
    QLineEdit* strat_width_edit_ = nullptr;  // width for strangle/condor/spreads
    QLabel* strategy_preview_ = nullptr;
    QPushButton* place_strategy_btn_ = nullptr;

    // Labels
    QLabel* symbol_label_ = nullptr; // "SYMBOL · EXCHANGE" context line
    QLabel* balance_label_ = nullptr;
    QLabel* market_price_label_ = nullptr;
    QLabel* cost_label_ = nullptr;
    QLabel* margin_label_ = nullptr; // required margin from broker API
    QLabel* status_label_ = nullptr;
    QLabel* mode_label_ = nullptr;

    // Static section / field titles (cached for retranslateUi)
    QLabel* title_label_ = nullptr;
    QLabel* product_title_ = nullptr;
    QLabel* exchange_title_ = nullptr;
    QLabel* qty_title_ = nullptr;
    QLabel* price_title_ = nullptr;
    QLabel* trigger_title_ = nullptr;
    QLabel* sl_title_ = nullptr;
    QLabel* tp_title_ = nullptr;
    QLabel* strat_title_ = nullptr;
    QLabel* strat_strike_title_ = nullptr;
    QLabel* strat_expiry_title_ = nullptr;
    QLabel* strat_lot_title_ = nullptr;
    QLabel* strat_width_title_ = nullptr;
    QLabel* strat_legs_title_ = nullptr;

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
