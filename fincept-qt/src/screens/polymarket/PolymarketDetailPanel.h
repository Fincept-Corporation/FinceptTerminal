#pragma once

#include "screens/polymarket/ExchangePresentation.h"
#include "services/polymarket/PolymarketTypes.h"
#include "services/prediction/PredictionTypes.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QWidget>

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

  signals:
    void tab_changed(int index);
    void interval_changed(const QString& interval);
    void outcome_changed(int index);
    void related_market_clicked(const fincept::services::prediction::PredictionMarket& market);
    void place_order(const fincept::services::prediction::OrderRequest& req);

  private:
    void build_ui();
    QWidget* create_overview_page();
    QWidget* create_trade_page();
    QWidget* create_holders_page();
    QWidget* create_comments_page();
    QWidget* create_related_page();

    void set_active_tab(int tab);
    void apply_accent_to_tabs();
    void apply_presentation_to_stats();  // show/hide OPEN INT cell, re-label
    void render_status_badge(const fincept::services::prediction::PredictionMarket& market);
    void refresh_ticket_side_style();
    void on_submit_clicked();

    QList<QPushButton*> tab_btns_;
    QStackedWidget* stack_ = nullptr;

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
    QWidget* outcome_container_ = nullptr;
    QLabel* description_label_ = nullptr;

    // Embedded sub-widgets
    PolymarketOrderBook* orderbook_ = nullptr;
    PolymarketPriceChart* price_chart_ = nullptr;
    PolymarketActivityFeed* activity_feed_ = nullptr;

    // Trade ticket
    QStackedWidget* ticket_stack_       = nullptr;  // 0=no-account, 1=ticket
    QLabel*         ticket_balance_lbl_ = nullptr;
    QLabel*         ticket_position_lbl_= nullptr;
    QComboBox*      ticket_outcome_cb_  = nullptr;
    QPushButton*    ticket_buy_btn_     = nullptr;
    QPushButton*    ticket_sell_btn_    = nullptr;
    QLineEdit*      ticket_price_edit_  = nullptr;
    QLineEdit*      ticket_size_edit_   = nullptr;
    QComboBox*      ticket_type_cb_     = nullptr;
    QPushButton*    ticket_submit_btn_  = nullptr;
    QLabel*         ticket_status_lbl_  = nullptr;
    QString         ticket_side_        = "BUY";

    // Holders
    QTableWidget* holders_table_ = nullptr;

    // Comments
    QWidget* comments_container_ = nullptr;

    // Related
    QWidget* related_container_ = nullptr;

    // OPEN INT stat cell — hidden on Kalshi (no OI endpoint).
    QWidget* oi_box_ = nullptr;

    // Presentation — determines accent color, price formatting, stat
    // visibility, and status-badge wording. Default is the Polymarket
    // profile so the widget renders correctly before set_presentation()
    // is called (ctor path).
    ExchangePresentation presentation_ = ExchangePresentation::for_polymarket();

    // Cached copy of the last market rendered — needed so set_presentation()
    // can re-render the overview + status badge with new formatters.
    fincept::services::prediction::PredictionMarket last_market_;
    bool has_last_market_ = false;

    // Tab indices.
    static constexpr int kTabTrade    = 3;
    static constexpr int kTabTrades   = 4;
    static constexpr int kTabHolders  = 5;
    static constexpr int kTabComments = 6;
    static constexpr int kTabRelated  = 7;
};

} // namespace fincept::screens::polymarket
