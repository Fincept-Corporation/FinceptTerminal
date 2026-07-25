#pragma once
// Equity Order Entry — BUY/SELL form configurable per broker profile

#include "trading/BrokerInterface.h"
#include "trading/OptionsStrategyBuilder.h"
#include "trading/TradingTypes.h"

#include <QComboBox>
#include <QEvent>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QShowEvent>
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

    // Fill the LIMIT price box from an external price gesture (clicking a level in
    // the market-depth ladder). Switches the ticket to LMT when it is on MKT, so
    // the value the user just picked is the one that will actually be sent.
    void set_limit_price(double price);

    // Move keyboard focus to the quantity box (the trader's entry point).
    void focus_quantity();

  signals:
    void order_submitted(const trading::UnifiedOrder& order);
    void broadcast_requested(const trading::UnifiedOrder& order);
    /// Emitted instead of order_submitted when the inline BROKERS selector has
    /// ≥1 account checked — the same order goes to every selected account.
    void multi_broker_submit(const trading::UnifiedOrder& order, const QStringList& account_ids);
    void strategy_order_submitted(const trading::BasketOrderRequest& basket);

  protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_submit();
    void on_place_strategy();

  private:
    void update_cost_preview();
    void retranslateUi();
    void set_buy_side(bool is_buy);
    void set_order_type(int idx);

    // Shared entry validation for BOTH the focused submit and the broadcast
    // ("ALL") path. Returns false and paints the status line when the ticket is
    // not safe to send. `qty_out` receives the parsed quantity.
    bool validate_entry(double& qty_out);

    // Build the order from the current form. Fields that do not apply to the
    // selected order type are zeroed so a stale limit/trigger left in a disabled
    // box can never ride along to the broker.
    trading::UnifiedOrder build_order() const;

    // Lock/unlock the send controls around an in-flight submit (see submit_lock_
    // failsafe below).
    void set_send_locked(bool locked);

    // Repaint the LTP line, including the stale marker.
    void render_price_label();
    void update_stale_state();

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
    QToolButton* brokers_btn_ = nullptr; // inline multi-broker selector
    QMenu* brokers_menu_ = nullptr;      // checkable account list (stays open)
    QSet<QString> broadcast_ids_;        // checked account_ids; empty = focused only
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

    // Send lock: the send controls stay disabled from click until the placement
    // result comes back through show_order_status(). A failsafe timer releases
    // the lock if a result never arrives, so the ticket can never wedge.
    bool submit_locked_ = false;
    QTimer* submit_failsafe_ = nullptr;

    // Stale-quote guard: a price with no indication of its age is dangerous to
    // trade on. last_price_ms_ is stamped on every real quote; the timer (running
    // only while visible, P3) flips the LTP line to a dim "STALE" reading.
    qint64 last_price_ms_ = 0;
    bool price_is_stale_ = false;
    QTimer* stale_timer_ = nullptr;

    void fetch_margin_async();

    // Cached product types from profile (for order construction)
    QVector<trading::ProductTypeDef> product_types_;
};

} // namespace fincept::screens::equity
