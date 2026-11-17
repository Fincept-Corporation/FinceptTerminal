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

class QFrame;
class QMenu;
class QToolButton;

namespace fincept::screens::equity {

class EquityOrderEntry : public QWidget {
    Q_OBJECT
  public:
    explicit EquityOrderEntry(QWidget* parent = nullptr);

    // Called when broker changes — rebuilds product/exchange combos, resets state
    void configure_for_broker(const trading::BrokerProfile& profile);

    // Available cash / margin. `currency` = ISO code of `balance` (the account's);
    // empty keeps the one already known (broker profile, or the built-in paper
    // account's from set_builtin_mode()).
    void set_balance(double balance, const QString& currency = QString());
    void set_current_price(double price);
    void set_mode(bool is_paper);
    void set_symbol(const QString& symbol);
    void set_exchange(const QString& exchange);
    void set_broker_id(const QString& broker_id);
    // Focused account: the pre-trade margin line reads THIS account's credentials.
    void set_account_id(const QString& account_id);
    // Exchange currently selected in the ticket's EXCHANGE combo.
    QString current_exchange() const;
    void show_order_status(const QString& msg, bool success); // show result after order placed
    // Paint a status line WITHOUT touching the send lock — for outcomes that are not
    // the ticket's own in-flight order (modify / cancel / square-off results), so they
    // can never re-enable SEND while a placement is still in flight.
    void show_status_message(const QString& msg, bool success);

    // Fill the LIMIT price box from an external price gesture (clicking a level in
    // the market-depth ladder). Switches the ticket to LMT when it is on MKT, so
    // the value the user just picked is the one that will actually be sent.
    void set_limit_price(double price);

    // Move keyboard focus to the quantity box (the trader's entry point).
    void focus_quantity();
    // BUY / SELL from the screen (command bar, B / S): take that side and focus the
    // quantity. Never sends anything.
    void focus_side(bool is_buy);

    // Broker-availability banner at the top of the ticket, with an OPEN ACCOUNTS
    // button. `blocking` disables every send control (BUY/SELL, ALL, strategy)
    // until the banner is cleared — used when no order can be placed at all, so
    // the ticket never offers buttons that silently do nothing. A non-blocking
    // banner only informs. An empty title clears the banner.
    void set_broker_notice(const QString& title, const QString& detail, bool blocking);

    // ── Built-in paper account / delayed public prices ──────────────────────
    // Simulated ticket: products Intraday / Delivery, no exchange picker (the
    // instrument names its venue), no multi-broker / basket / options-strategy rows
    // (they address broker accounts), a plain "simulated fills at delayed prices"
    // line above the submit button.
    void set_builtin_mode(bool on, const QString& account_currency = QString());
    // The LTP shown is a delayed public price: label it with its delay and the
    // exchange time it printed at; staleness is judged against `max_age_ms` of
    // receive time instead of the live-tick threshold. `delayed` false restores the
    // live behaviour.
    void set_price_source(bool delayed, int delay_min = -1, qint64 printed_ms = 0, const QString& tz_label = QString(),
                          qint64 max_age_ms = 0);
    // Price currency (as quoted, minor units kept), price decimals (-1 = from the
    // value) and lot size (> 1 enforces whole lots when `lot_known`).
    void set_instrument_meta(const QString& currency, int digits, double lot_size, bool lot_known);
    // How the symbol is shown on the ticket ("RELIANCE · NSE"); the order carries
    // the symbol set by set_symbol().
    void set_symbol_display(const QString& text);
    // One line about the instrument's session ("NSE CLOSED · MARKET orders queue for
    // the open"); empty hides it. `open` picks the colour.
    void set_session_line(const QString& text, bool open);
    // Grey the side / submit controls with `reason` (e.g. the focused broker cannot
    // trade this instrument); empty re-enables.
    void set_trade_block(const QString& reason);

  signals:
    // OPEN ACCOUNTS on the broker banner — the screen opens its accounts dialog.
    void accounts_requested();
    void order_submitted(const trading::UnifiedOrder& order);
    void broadcast_requested(const trading::UnifiedOrder& order);
    /// Emitted instead of order_submitted when the inline BROKERS selector has
    /// ≥1 account checked — the same order goes to every selected account.
    void multi_broker_submit(const trading::UnifiedOrder& order, const QStringList& account_ids);
    void strategy_order_submitted(const trading::BasketOrderRequest& basket);
    // The BASKETS dialog was closed — baskets may have been executed, so the screen
    // refreshes its blotter.
    void baskets_closed();

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

    // Enable SL / TP only where they will actually be honoured (local paper, or a broker
    // product for which entry_sl_tp_honoured() is true); otherwise disable + clear them.
    void apply_sl_tp_availability();

    // Repaint the LTP line, including the stale marker.
    void render_price_label();
    void update_stale_state();
    // Re-format the balance / margin lines from the values already held (format change).
    void render_balance_label();
    void render_margin_label();
    // Broker ticket: the price currency follows the selected exchange (venue table;
    // the account currency for a segment it does not list), until
    // set_instrument_meta() names the instrument's own.
    void sync_price_currency();

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
    QLabel* mode_label_ = nullptr;     // PAPER / LIVE chip in the header (state tone)
    QFrame* mode_rule_ = nullptr;      // 2 px state-toned rule under the header (paper / live)
    QString mode_tone_;                // "paper" / "live" last applied (re-polish on change only)
    void apply_mode_tone();

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
    // Price currency of the instrument, as quoted (minor units kept: "GBp"); empty = unknown.
    QString current_currency_;
    bool instrument_ccy_known_ = false; // current_currency_ came from set_instrument_meta()
    QString broker_id_;
    QString account_id_; // focused account (margin credentials)
    // Focused broker's profile, for entry_sl_tp_honoured() (SL / TP availability per product).
    trading::BrokerProfile sl_tp_profile_;

    // Margin fetch debounce timer — fires 500ms after last qty/price change
    QTimer* margin_timer_ = nullptr;
    std::atomic<bool> margin_fetching_{false};

    // Send lock: the send controls stay disabled from click until the placement
    // result comes back through show_order_status(). A failsafe timer releases
    // the lock if a result never arrives, so the ticket can never wedge.
    bool submit_locked_ = false;
    QTimer* submit_failsafe_ = nullptr;

    // Broker banner (set_broker_notice). broker_blocked_ keeps the send controls
    // disabled independently of the in-flight submit lock above.
    QWidget* notice_box_ = nullptr;
    QLabel* notice_title_ = nullptr;
    QLabel* notice_detail_ = nullptr;
    bool broker_blocked_ = false;
    bool notice_blocking_style_ = false; // style applied to notice_box_ (restyle on change only)

    // Stale-quote guard: a price with no indication of its age is dangerous to
    // trade on. last_price_ms_ is stamped on every real quote; the timer (running
    // only while visible, P3) flips the LTP line to a dim "STALE" reading.
    qint64 last_price_ms_ = 0;
    bool price_is_stale_ = false;
    QTimer* stale_timer_ = nullptr;

    void fetch_margin_async();
    QString fmt_px(double v) const;     // bare price, as quoted
    QString fmt_px_sym(double v) const; // price with its currency unit ("$351.71", "117.70p")
    // An amount quoted in the price currency (qty x price, strategy P/L), shown in
    // its major unit ("£117.70" for 11,770 GBp).
    QString fmt_value(double v) const;

    bool balance_set_ = false;     // set_balance() since the last account switch
    double margin_total_ = 0.0;    // last broker margin shown (account currency)
    bool est_converts_ = false;    // the Est line shows an FX-converted figure: refresh on new rates

    // Cached product types from profile (for order construction)
    QVector<trading::ProductTypeDef> product_types_;

    // Built-in paper / delayed-price state (see set_builtin_mode / set_price_source).
    bool builtin_mode_ = false;
    bool delayed_price_ = false;
    int delay_min_ = -1;
    qint64 printed_ms_ = 0;
    QString printed_tz_;
    qint64 delayed_max_age_ms_ = 0;
    int price_digits_ = -1;
    double lot_size_ = 1.0;
    bool lot_known_ = false;
    QString symbol_display_;
    QString trade_block_;
    QString balance_currency_;        // account currency: built-in paper account's, or the broker profile's
    QLabel* sim_note_ = nullptr;      // "simulated fills at delayed prices" above submit
    QLabel* session_label_ = nullptr; // venue session line
    QWidget* brokers_row_ = nullptr;  // BROKERS selector + BASKETS
};

} // namespace fincept::screens::equity
