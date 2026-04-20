#pragma once

#include "screens/polymarket/ExchangePresentation.h"
#include "services/polymarket/PolymarketTypes.h"
#include "services/prediction/PredictionTypes.h"

#include <QLabel>
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

    // Polymarket-only enrichment setters — guarded by active_id at the caller.
    void set_price_summary(const fincept::services::polymarket::PriceSummary& summary);
    void set_top_holders(const QVector<fincept::services::polymarket::TopHolder>& holders);
    void set_comments(const QVector<fincept::services::polymarket::Comment>& comments);
    void set_related_markets(const QVector<fincept::services::prediction::PredictionMarket>& markets);
    void set_open_interest(double oi);

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

  private:
    void build_ui();
    QWidget* create_overview_page();
    QWidget* create_holders_page();
    QWidget* create_comments_page();
    QWidget* create_related_page();

    void set_active_tab(int tab);
    void apply_accent_to_tabs();
    void apply_presentation_to_stats();  // show/hide OPEN INT cell, re-label
    void render_status_badge(const fincept::services::prediction::PredictionMarket& market);

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

    // Polymarket-only tab button indices (for enable/disable).
    static constexpr int kTabHolders = 4;
    static constexpr int kTabComments = 5;
    static constexpr int kTabRelated = 6;
};

} // namespace fincept::screens::polymarket
