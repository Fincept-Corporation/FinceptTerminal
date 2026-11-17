#pragma once
// Crypto Order Entry — BUY/SELL tabs with toggle type buttons

#include "screens/crypto_trading/CryptoTypes.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>

namespace fincept::screens::crypto {

class CryptoOrderEntry : public QWidget {
    Q_OBJECT
  public:
    explicit CryptoOrderEntry(QWidget* parent = nullptr);

    /// The account's free balance and the currency it is held in (PAPER: the
    /// paper portfolio's currency; LIVE: the exchange balance shown). An order is
    /// priced in the pair's quote asset: where the two differ the ticket converts
    /// with a real rate (named in the tooltip) and shows "—" while there is none.
    /// Empty `ccy` = the pair's quote asset.
    void set_balance(double balance, const QString& ccy = QString());
    void set_current_price(double price);
    /// Load a price into the ticket's limit-price box (order-book click). A plain
    /// MARKET / STOP ticket is promoted to LIMIT / STOP-LMT so the price is shown.
    void set_limit_price(double price);
    void set_mode(bool is_paper);
    void set_symbol(const QString& symbol);
    void set_futures_mode(bool is_futures); // show/hide leverage + margin controls
    bool reduce_only() const;               // reduce-only flag (perp/futures only)
    /// Ticket's margin mode ("cross" / "isolated") and leverage. Sent with each
    /// order where the exchange takes them per order (OKX, Bitget, KuCoin, MEXC,
    /// HTX); applied to the account only through APPLY (apply_leverage_requested).
    QString margin_mode() const;
    int leverage() const;
    /// The exchange's current leverage / margin mode for the market, shown next
    /// to the controls and loaded into them (no signal). 0 / empty: unknown.
    void set_account_leverage(int leverage, const QString& margin_mode);

    /// The market the selected pair resolves to and what may be sent there
    /// (see ResolvedMarket). Shown in the ticket; in LIVE mode it gates the
    /// order types, SL / TP and reduce-only, and on_submit refuses with the
    /// reason instead of sending anything the exchange would get in a
    /// degraded form.
    void set_market(const ResolvedMarket& market);
    void set_venue(const QString& exchange_id);

    // In-flight guard for live order submission. While an order is being
    // checked or sent the submit button is disabled and relabelled (`label`,
    // default "SENDING…") so a double-click / held Ctrl+Enter can't fire a
    // second real exchange order. Re-enabling restores the BUY/SELL label.
    void set_submit_busy(bool busy, const QString& label = QString());
    bool submit_busy() const { return submit_busy_; }

    /// A refusal for the last BUY / SELL press (from the exchange service or the
    /// screen): shown in the status line until the ticket is edited.
    void show_order_refusal(const QString& message);
    /// A message about an earlier order (e.g. the look-up of an unconfirmed one)
    /// that must not clear what the user is typing now; stays until the ticket
    /// is edited. `severity`: "info" | "warning" | "error".
    void show_notice(const QString& message, const QString& severity);

    /// Clear quantity / prices / SL / TP after an order was accepted (paper fill or
    /// live order acknowledged), so the next order can't silently reuse them.
    /// A non-empty `message` is shown in the inline status line as confirmation;
    /// `severity` is the status line's style ("info", "warning", "error").
    void reset_after_fill(const QString& message = {}, const QString& severity = QStringLiteral("info"));

    /// The exchange's order minimums for the market the ticket's pair trades on
    /// (`market` is that market's symbol, `cost_ccy` the currency min_cost is in).
    /// 0 = no minimum / unknown. Checked in on_submit() so an order the exchange
    /// would reject for its size never goes out. Cleared on a symbol change.
    void set_market_limits(double min_amount, double min_cost, const QString& market = {},
                           const QString& cost_ccy = {});

    /// Put the keyboard focus in the quantity box (the "NEW ORDER" action of an
    /// empty positions / orders / history tab). Places nothing.
    void focus_quantity();

    /// An external prefill (the header search box's "buy 0.5 btc limit 60000"): a fresh
    /// ticket filled in exactly as typing would — side, order type ("market" | "limit" |
    /// "stop" | "stop_limit"; "" = LIMIT when a limit price is given, else MARKET), quantity
    /// and limit price (<= 0 / non-finite = empty). Values are never rounded or clamped: one
    /// the boxes would not accept as typed is shown as it is, marked, with the reason in the
    /// status line. The usual checks run; nothing is submitted.
    void prefill(bool is_buy, double quantity, const QString& order_type, double limit_price);

  signals:
    void order_submitted(const QString& side, const QString& order_type, double quantity, double price,
                         double stop_price, double sl_price, double tp_price);
    /// APPLY pressed: change the account's leverage / margin mode for the market
    /// (the screen confirms first). Editing the controls alone changes nothing.
    void apply_leverage_requested(int leverage, const QString& margin_mode);
    /// The user chose another market for the pair (substitute / alternative).
    void market_choice_requested(const QString& symbol);

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_submit();
    void on_pct_clicked(int pct);

  private:
    void update_cost_preview();
    void apply_capabilities();  // LIVE gating of order types / SL-TP / reduce-only from market_
    void update_market_banner();
    QString live_refusal(int order_type, bool has_sl_tp) const; // why on_submit must not send
    void clear_ticket_fields(); // qty / limit / stop / SL / TP + validation state
    void show_submit_error(const QString& message, QWidget* focus);
    void set_buy_side(bool is_buy);
    void set_order_type(int idx);
    void retranslateUi();
    /// The market whose prices the ticket shows: the resolved market, else the pair.
    QString price_pair() const;
    /// The pair's quote asset (what an order's value is in).
    QString quote_ccy() const;
    /// The balance's currency (set_balance), else the quote asset.
    QString balance_ccy() const;
    /// BALANCE / MARK labels and the price units, from the held values (format
    /// change, new pair, new balance).
    void render_account_card();

    // Title
    QLabel* title_label_ = nullptr;

    // Side tabs
    QPushButton* buy_tab_ = nullptr;
    QPushButton* sell_tab_ = nullptr;

    // Type buttons
    QPushButton* type_btns_[4] = {};
    int active_type_ = 0; // 0=Market 1=Limit 2=Stop 3=StopLimit

    // Form
    QLineEdit* qty_edit_ = nullptr;
    QLineEdit* price_edit_ = nullptr;
    QLineEdit* stop_price_edit_ = nullptr;
    QLineEdit* sl_edit_ = nullptr;
    QLineEdit* tp_edit_ = nullptr;
    QPushButton* submit_btn_ = nullptr;
    QWidget* advanced_section_ = nullptr;
    QPushButton* advanced_toggle_ = nullptr;

    // Container rows so price/stop can be shown/hidden as a unit when the
    // selected order type doesn't need them.
    QWidget* price_row_ = nullptr;
    QWidget* stop_row_ = nullptr;

    // Header / account card labels
    QLabel* balance_label_ = nullptr;
    QLabel* market_price_label_ = nullptr;
    QLabel* avail_label_ = nullptr;
    QLabel* mode_label_ = nullptr;

    // Static field/section titles (cached for retranslateUi)
    QLabel* balance_title_ = nullptr;
    QLabel* mark_title_ = nullptr;
    QLabel* avail_title_ = nullptr;
    QLabel* qty_title_ = nullptr;
    QLabel* price_title_ = nullptr;
    QLabel* stop_title_ = nullptr;
    QLabel* sl_title_ = nullptr;
    QLabel* tp_title_ = nullptr;
    QLabel* leverage_title_ = nullptr;
    QLabel* margin_title_ = nullptr;

    // Order breakdown
    QLabel* cost_label_ = nullptr;
    QLabel* fee_label_ = nullptr;
    QLabel* total_label_ = nullptr;
    QLabel* recv_label_ = nullptr;
    QLabel* pct_used_label_ = nullptr;

    // Order breakdown titles (cached for retranslateUi)
    QLabel* cost_title_ = nullptr;
    QLabel* fee_title_ = nullptr;
    QLabel* total_title_ = nullptr;
    QLabel* recv_title_ = nullptr;
    QLabel* pct_used_title_ = nullptr;

    // Submit subtitle (rendered as a second line under the main button label)
    QLabel* submit_subtitle_ = nullptr;
    QLabel* status_label_ = nullptr;

    // Futures controls
    QWidget* futures_section_ = nullptr;
    QSpinBox* leverage_spin_ = nullptr;
    QComboBox* margin_mode_combo_ = nullptr;
    QCheckBox* reduce_only_check_ = nullptr;
    QLabel* lev_current_label_ = nullptr; // the exchange's current leverage / mode
    QPushButton* apply_lev_btn_ = nullptr;
    int account_leverage_ = 0;
    QString account_margin_mode_;

    // Resolved market banner + gating
    QLabel* market_label_ = nullptr;
    QPushButton* use_market_btn_ = nullptr;
    QLabel* sltp_note_ = nullptr;
    ResolvedMarket market_;
    QString venue_;

    // State
    double balance_ = 0;
    QString balance_ccy_; // what balance_ is held in (empty: the quote asset)
    bool has_balance_ = false;
    double current_price_ = 0;
    bool is_paper_ = true;
    bool is_buy_side_ = true;
    bool is_futures_ = false;
    bool submit_busy_ = false; // true while a live order POST is in flight
    bool submit_error_pinned_ = false; // a validation message from the last submit press is showing
    QString current_symbol_ = "BTC/USDT";
    // Exchange minimums for current_symbol_'s market (0 = none / unknown).
    double min_amount_ = 0.0;
    double min_cost_ = 0.0;
    QString limits_market_;
    QString limits_cost_ccy_;
};

} // namespace fincept::screens::crypto
