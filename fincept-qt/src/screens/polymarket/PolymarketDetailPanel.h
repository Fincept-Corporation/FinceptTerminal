#pragma once

#include "screens/polymarket/ExchangePresentation.h"
#include "screens/polymarket/PolymarketViewState.h"
#include "services/polymarket/PolymarketTypes.h"
#include "services/prediction/PredictionTypes.h"

#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPushButton>
#include <QStackedWidget>
#include <QStringList>
#include <QTableWidget>
#include <QTimer>
#include <QWidget>

namespace fincept::ui {
class AutoLoadPane;
} // namespace fincept::ui

namespace fincept::screens::polymarket {

class PolymarketOrderBook;
class PolymarketPriceChart;
class PolymarketActivityFeed;

/// Right detail panel: 7-tab stacked widget with embedded sub-widgets.
///
/// Core tabs (Overview/OrderBook/Chart/Trades) consume unified
/// prediction::* types so both Polymarket and Kalshi render through the
/// same surface. The remaining tabs (Holders, Comments, Related) are
/// Polymarket-only enrichments; the screen only populates them when the
/// active exchange is Polymarket, and clears them on exchange switch.
class PolymarketDetailPanel : public QWidget {
    Q_OBJECT
  public:
    explicit PolymarketDetailPanel(QWidget* parent = nullptr);

    /// The per-market feeds, each with its own loading / failure state.
    enum class Load { Book, Chart, Trades, Holders, Comments, Related };
    /// `which` was requested for the selected market: its tab shows a skeleton
    /// in its shape until the data (or set_load_error) arrives.
    void set_loading(Load which);
    /// The request for `which` failed: its tab shows the reason with RETRY
    /// (reload_requested). Data already on screen stays.
    void set_load_error(Load which, const QString& message);
    /// The active exchange does not provide `which` (e.g. price history in a
    /// preview): a plain "not available here" state, no RETRY.
    void set_load_unavailable(Load which, const QString& message);
    /// Nothing is selected: a skeleton while the market list loads (the screen
    /// selects its first market as soon as it arrives), else a designed state
    /// that says why there is nothing to show.
    void show_no_market(bool loading, const QString& reason = QString());
    /// The stored trading credential of the active exchange exists but could
    /// not be read (empty = readable or none): the TRADE tab says so with RETRY
    /// instead of "connect an account".
    void set_account_read_error(const QString& reason);

    /// Install a per-exchange presentation profile. Restyles the tab bar
    /// accent, swaps the stats grid layout (hides OPEN INT on Kalshi), and
    /// re-renders the currently-selected market using the new formatters.
    /// Also calls set_polymarket_extras_enabled() based on the profile.
    void set_presentation(const ExchangePresentation& p);

    // Core (unified) setters — consumed for both Polymarket and Kalshi.
    void set_market(const fincept::services::prediction::PredictionMarket& market);
    void set_order_book(const fincept::services::prediction::PredictionOrderBook& book);
    void set_price_history(const fincept::services::prediction::PriceHistory& history);
    void set_trades(const QVector<fincept::services::prediction::PredictionTrade>& trades);

    // Trading ticket setters — wired from adapter balance_ready / positions_ready.
    void set_balance(const fincept::services::prediction::AccountBalance& balance);
    void set_positions(const QVector<fincept::services::prediction::PredictionPosition>& positions);
    void on_order_result(const fincept::services::prediction::OrderResult& result);

    /// Called when credentials state changes so ticket can show/hide the
    /// "connect account" placeholder vs the actual ticket form.
    void set_trading_enabled(bool enabled);
    /// Restrict the ticket's ORDER TYPE box to the codes the active exchange can place
    /// (from ExchangeCapabilities). The box was a fixed GTC/FOK/FAK list for every venue.
    void set_order_types(const QStringList& types);
    /// Exchange-wide price tick / minimum size (ExchangeCapabilities). Used to
    /// validate the ticket whenever the selected market's order book has not
    /// supplied its own constraints, so an off-tick price is rejected up front
    /// instead of being silently rounded by the adapter after confirmation.
    void set_exchange_constraints(double tick_size, double min_order_size);

    // Polymarket-only enrichment setters — guarded by active_id at the caller.
    void set_price_summary(const fincept::services::polymarket::PriceSummary& summary);
    void set_top_holders(const QVector<fincept::services::polymarket::TopHolder>& holders);
    void set_comments(const QVector<fincept::services::polymarket::Comment>& comments);
    void set_related_markets(const QVector<fincept::services::prediction::PredictionMarket>& markets);
    void set_open_interest(double oi);

    /// Attach a long-form tooltip (e.g. series fee info) to the market
    /// question label. Pass an empty string to clear. Used by Kalshi to
    /// surface per-series fee_type + fee_multiplier on hover.
    void set_series_tooltip(const QString& tooltip);

    /// Hide/disable the Polymarket-only tabs (Holders/Comments/Related) and
    /// clear their contents. Called when the active exchange changes to a
    /// provider that does not support those enrichments.
    void set_polymarket_extras_enabled(bool enabled);

    void clear();

  protected:
    void changeEvent(QEvent* event) override;

  signals:
    void tab_changed(int index);
    void interval_changed(const QString& interval);
    void outcome_changed(int index);
    void related_market_clicked(const fincept::services::prediction::PredictionMarket& market);
    void place_order(const fincept::services::prediction::OrderRequest& req);
    /// RETRY on a failed tab: re-request the selected market's data.
    void reload_requested();
    /// CONNECT ACCOUNT on the TRADE tab.
    void connect_account_requested();
    /// RETRY on the TRADE tab's "could not read the stored credential" page.
    void retry_credentials_requested();

  private:
    void build_ui();
    QWidget* create_overview_page();
    QWidget* create_trade_page();
    QWidget* create_holders_page();
    QWidget* create_comments_page();
    QWidget* create_related_page();

    void set_active_tab(int tab);
    void apply_accent_to_tabs();
    void apply_presentation_to_stats(); // show/hide OPEN INT cell, re-label
    void render_status_badge(const fincept::services::prediction::PredictionMarket& market);
    void refresh_ticket_side_style();
    void on_submit_clicked();
    void retranslateUi();
    /// The TRADE tab's page without a usable account: what the active exchange
    /// needs + CONNECT ACCOUNT, or the unreadable-credential page + RETRY.
    void refresh_account_page();

    /// Recompute the "≈ COST" readout under the ticket from the current
    /// price × size. Shows "—" when either field is blank/invalid.
    void refresh_ticket_cost();
    /// Fill the ticket price from an order-book level click and surface the
    /// TRADE tab so the effect is visible. No-op when trading is disabled.
    void on_book_price_clicked(double price);
    /// Repopulate the ticket outcome combo for `market`, preserving the
    /// user's current choice by outcome name when the market is unchanged.
    void sync_ticket_outcomes(const fincept::services::prediction::PredictionMarket& market, bool same_market);

    QList<QPushButton*> tab_btns_;
    QStackedWidget* stack_ = nullptr;
    // The tabs of the selected market, or (nothing selected) a skeleton / designed state:
    // an AutoLoadPane in feed mode — the selection is pushed by the screen (set_market →
    // feed_data, the list loading → feed_waiting, nothing to show → feed_idle).
    fincept::ui::AutoLoadPane* market_pane_ = nullptr;
    QWidget* market_view_ = nullptr;
    // Feeds still loading (bit = 1 << Load). A reply that never comes (no data, no
    // error) ends in "no answer" + RETRY instead of a skeleton forever.
    unsigned pending_loads_ = 0;
    QTimer* load_watchdog_ = nullptr;
    void settle_load(Load which) { pending_loads_ &= ~(1u << static_cast<unsigned>(which)); }

    // Fixed-text captions cached for retranslateUi.
    QLabel* outcomes_header_ = nullptr;
    QList<QLabel*> stat_caption_lbls_;  // VOLUME / LIQUIDITY / OPEN INT / END DATE / MIDPOINT / SPREAD / LAST TRADE
    fincept::ui::EmptyState* no_acct_state_ = nullptr; // TRADE without a usable account
    QString account_read_error_;
    QLabel* bal_caption_lbl_ = nullptr; // "AVAILABLE"
    QLabel* pos_caption_lbl_ = nullptr; // "POSITION"
    QList<QLabel*> trade_form_caption_lbls_; // OUTCOME / PRICE (0–1) / SIZE / ORDER TYPE

    // Overview
    QLabel* question_label_ = nullptr;
    QLabel* volume_label_ = nullptr;
    QLabel* liquidity_label_ = nullptr;
    QLabel* end_date_label_ = nullptr;
    QLabel* midpoint_label_ = nullptr;
    QLabel* spread_label_ = nullptr;
    QLabel* last_trade_label_ = nullptr;
    QLabel* oi_label_ = nullptr;
    QLabel* status_label_ = nullptr;
    QPushButton* news_btn_ = nullptr; // "NEWS ↗": searches the News screen for the market's subject
    QString news_query_;              // subject derived from the question ("" = button disabled)
    QWidget* outcome_container_ = nullptr;
    QLabel* description_label_ = nullptr;

    // Embedded sub-widgets
    PolymarketOrderBook* orderbook_ = nullptr;
    PolymarketPriceChart* price_chart_ = nullptr;
    PolymarketActivityFeed* activity_feed_ = nullptr;

    // Trade ticket
    QStackedWidget* ticket_stack_ = nullptr; // 0=no-account, 1=ticket
    QLabel* ticket_balance_lbl_ = nullptr;
    QLabel* ticket_position_lbl_ = nullptr;
    QComboBox* ticket_outcome_cb_ = nullptr;
    QPushButton* ticket_buy_btn_ = nullptr;
    QPushButton* ticket_sell_btn_ = nullptr;
    QLineEdit* ticket_price_edit_ = nullptr;
    QLineEdit* ticket_size_edit_ = nullptr;
    QComboBox* ticket_type_cb_ = nullptr;
    QPushButton* ticket_submit_btn_ = nullptr;
    QLabel* ticket_status_lbl_ = nullptr;
    QLabel* ticket_cost_lbl_ = nullptr; // "≈ $12.50 max cost" readout
    QString ticket_side_ = "BUY";
    /// Outcome names currently loaded into ticket_outcome_cb_. Lets a
    /// same-market refresh skip the rebuild (and so keep the selection).
    QStringList ticket_outcome_names_;
    /// True between emit place_order() and the matching on_order_result().
    /// Guards against double-submit and stops an incoming price refresh from
    /// re-enabling the submit button mid-flight.
    bool submit_in_flight_ = false;
    /// Exchange constraints captured from the last order book for the
    /// selected market. Used to validate/round the ticket price and size so
    /// the exchange doesn't silently reject the order.
    double book_tick_size_ = 0.0;
    double book_min_size_ = 0.0;
    /// Fallbacks from the active exchange's capabilities (set_exchange_constraints),
    /// used while book_* are 0 (no book yet, or a book that carries none).
    double exchange_tick_size_ = 0.0;
    double exchange_min_size_ = 0.0;

    // Holders
    QTableWidget* holders_table_ = nullptr;
    PmViewState holders_state_;

    // Comments
    QWidget* comments_container_ = nullptr;
    PmViewState comments_state_;

    // Related
    QWidget* related_container_ = nullptr;
    PmViewState related_state_;

    // OPEN INT stat cell — shown when the profile has open interest.
    QWidget* oi_box_ = nullptr;
    // LIQUIDITY stat cell — hidden on Kalshi (the API has no liquidity figure).
    QWidget* liquidity_box_ = nullptr;

    // Presentation — determines accent color, price formatting, stat
    // visibility, and status-badge wording. Default is the Polymarket
    // profile so the widget renders correctly before set_presentation()
    // is called (ctor path).
    ExchangePresentation presentation_ = ExchangePresentation::for_polymarket();

    // Cached copy of the last market rendered — needed so set_presentation()
    // can re-render the overview + status badge with new formatters.
    fincept::services::prediction::PredictionMarket last_market_;
    bool has_last_market_ = false;

    // Money on screen that is not kept elsewhere, so a number / currency format
    // change re-renders it without a fetch (ui::formatting::on_format_changed).
    fincept::services::prediction::AccountBalance last_balance_;
    bool has_last_balance_ = false;
    double last_open_interest_ = -1.0; ///< set_open_interest() figure on screen; < 0 = none

    // Tab indices.
    static constexpr int kTabTrade = 3;
    static constexpr int kTabTrades = 4;
    static constexpr int kTabHolders = 5;
    static constexpr int kTabComments = 6;
    static constexpr int kTabRelated = 7;
};

} // namespace fincept::screens::polymarket
