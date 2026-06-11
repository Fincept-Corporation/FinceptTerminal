#pragma once
// Equity Bottom Panel — tabbed: Positions, Holdings, Orders, Funds, Stats

#include "screens/equity_trading/EquityTypes.h"
#include "trading/BrokerInterface.h"
#include "trading/TradingTypes.h"

#include <QDate>
#include <QEvent>
#include <QLabel>
#include <QList>
#include <QMap>
#include <QTabWidget>
#include <QTableWidget>
#include <QVector>
#include <QWidget>

class QDateEdit;

namespace fincept::screens::equity {

class EquityBottomPanel : public QWidget {
    Q_OBJECT
  public:
    explicit EquityBottomPanel(QWidget* parent = nullptr);

    // Paper mode data
    void set_paper_positions(const QVector<trading::PtPosition>& positions);
    void set_paper_orders(const QVector<trading::PtOrder>& orders);

    // Live broker data
    void set_positions(const QVector<trading::BrokerPosition>& positions);
    void set_holdings(const QVector<trading::BrokerHolding>& holdings);

    // Live quote → in-place LTP / P&L refresh for the open-positions table.
    // Fed by the SAME DataHub quote stream that drives the top ticker bar, so the
    // position row and the header always show identical, real-time prices.
    // Works in both live (BrokerPosition) and paper (PtPosition) modes.
    void update_quote(const QString& symbol, const trading::BrokerQuote& quote);

    void set_orders(const QVector<trading::BrokerOrderInfo>& orders);
    // Funds + Stats render from view-models so the panel is source-agnostic; the
    // screen builds them from the paper engine (paper) or BrokerFunds (live).
    void set_funds_view(const EquityFundsView& view);
    void set_stats_view(const EquityStatsView& view);
    // Set the orders date-picker without emitting orders_day_changed (programmatic).
    void set_orders_date(const QDate& day);
    void set_calendar(const QVector<trading::MarketCalendarDay>& days);
    void set_clock(const trading::MarketClock& clock);
    void set_auctions(const QVector<trading::BrokerAuction>& auctions);
    void set_time_sales(const QVector<trading::BrokerTrade>& trades); // bulk load
    void prepend_trade(const trading::BrokerTrade& trade);            // live append at top
    void set_condition_codes(const QMap<QString, QString>& codes);

    void set_mode(bool is_paper);

    void set_account_id(const QString& account_id);

    // Currency symbol (₹/$/…) used to format the positions action-bar P&L totals.
    void set_currency(const QString& sym);

    // Show/hide the US-market-only tabs (Time & Sales, Auctions, Calendar). These
    // carry tape/condition/auction data that only US brokers (Alpaca etc.) provide,
    // so they're hidden for Indian and other non-US brokers.
    void set_us_market_tabs_visible(bool visible);

    // Collapse the sheet down to just its tab-bar header (and back).
    void toggle_collapsed();
    bool is_collapsed() const { return collapsed_; }

  signals:
    void cancel_order_requested(const QString& order_id);
    void modify_order_requested(const QString& order_id, double new_qty, double new_price);
    void import_holdings_requested(const QVector<trading::BrokerHolding>& holdings);
    void replicate_portfolio_requested();
    void cancel_all_orders_requested(const QString& account_id);
    void close_all_positions_requested(const QString& account_id);
    // Square off ALL holdings (delivery/CNC) — carries the current holdings so the
    // screen can close each. Positions are NOT affected.
    void square_off_all_holdings_requested(const QVector<trading::BrokerHolding>& holdings);
    // Square off a single holding (per-row SELL) — screen confirms then closes it.
    void square_off_holding_requested(const QString& symbol, const QString& exchange);
    // Paper: convert an open position's product in place (MIS -> CNC), identified
    // by its paper position id.
    void convert_position_requested(const QString& position_id, const QString& symbol, const QString& new_product);
    // Orders tab date-selector changed — request that IST day's order book.
    void orders_day_changed(const QDate& day);
    // Square off the subset of open positions by P&L sign (+1 winners, -1 losers).
    void square_off_group_requested(const QString& account_id, int sign);
    // Buy(add)/Sell(reduce) on a position or holding row — from the right-click
    // menu or the per-row SELL button. `qty` pre-fills the order ticket (the held
    // quantity for a reduce/exit; 0 = let the ticket default to 1).
    void trade_symbol_requested(const QString& symbol, const QString& product, bool is_buy, double qty);
    // Click a position/holding row → load that symbol's chart (like the watchlist).
    void chart_symbol_requested(const QString& symbol);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi();
    void apply_collapsed();
    void setup_positions_tab();
    void setup_holdings_tab();
    void setup_orders_tab();
    void setup_funds_tab();
    void setup_stats_tab();
    void setup_calendar_tab();
    void setup_time_sales_tab();
    void setup_auctions_tab();

    // Recompute the positions action-bar summary (net P&L + winners/losers chips)
    // from the current backing vectors. Called on every (re)load and quote patch.
    void update_positions_summary();

    // Live-quote patch for the Holdings table (CNC delivery): updates LTP / current
    // value / P&L for the symbol's row and the summary strip, in place — same per
    // tick refresh as the Positions table so Holdings tracks in real time.
    void update_holding_quote(const QString& symbol, double ltp, double prev_close);

    // Blank the shared positions/holdings/orders tables (and their row-aligned
    // caches). Called on every account or paper↔live transition so one account's
    // (or one mode's) data never lingers under another — e.g. Fyers paper orders
    // showing under a Zerodha live account. The incoming context repaints via
    // refresh_paper_panels() (paper) or the live broker hub topics (live).
    void clear_blotter_tables();

    static QTableWidgetItem* ensure_item(QTableWidget* table, int row, int col);

    // Build the Positions "Action" cell: a SELL button (opens an order ticket
    // pre-filled with the held qty) plus, for paper intraday rows, a "→ CNC"
    // convert button. `paper_pid` is empty for live rows (no convert).
    QWidget* make_positions_action_cell(const QString& symbol, const QString& product, double qty,
                                        bool show_convert, const QString& paper_pid);

    QTabWidget* tabs_ = nullptr;

    // Collapsible bottom-sheet state
    class QToolButton* collapse_btn_ = nullptr; // ▾/▴ toggle in the tab-bar corner
    QWidget* tab_content_ = nullptr;            // QTabWidget's page stack; hidden when collapsed
    bool collapsed_ = false;
    QList<int> saved_split_sizes_;              // parent splitter sizes to restore on expand

    // Tab indices (cached for retranslateUi tab text)
    int positions_tab_idx_ = -1;
    int holdings_tab_idx_ = -1;
    int orders_tab_idx_ = -1;
    int funds_tab_idx_ = -1;
    int stats_tab_idx_ = -1;
    int time_sales_tab_idx_ = -1;
    int auctions_tab_idx_ = -1;
    int calendar_tab_idx_ = -1;

    QTableWidget* positions_table_ = nullptr;
    // Backing data for the positions table, kept row-aligned with it so live
    // quotes can patch LTP / P&L / P&L% in place (no full rebuild, no flicker).
    QVector<trading::BrokerPosition> last_positions_;     // live mode
    QVector<trading::PtPosition> last_paper_positions_;   // paper mode

    // Positions action-bar summary: net P&L + winners/losers chips, each with a
    // group square-off button. Updated by update_positions_summary().
    QLabel* positions_total_pnl_label_ = nullptr;
    QLabel* positions_win_label_ = nullptr;
    QLabel* positions_loss_label_ = nullptr;
    class QPushButton* win_squareoff_btn_ = nullptr;
    class QPushButton* loss_squareoff_btn_ = nullptr;
    QString currency_sym_ = QStringLiteral("₹"); // for action-bar P&L formatting
    QTableWidget* holdings_table_ = nullptr;
    QLabel* holdings_invested_label_ = nullptr;
    QLabel* holdings_current_label_ = nullptr;
    QLabel* holdings_pnl_label_ = nullptr;
    QLabel* holdings_pnl_pct_label_ = nullptr;
    QLabel* holdings_day_pnl_label_ = nullptr; // "Today's P&L" total
    QLabel* holdings_count_label_ = nullptr;
    // Holdings summary-strip caption labels (cached for retranslateUi)
    QLabel* holdings_count_caption_ = nullptr;
    QLabel* holdings_invested_caption_ = nullptr;
    QLabel* holdings_current_caption_ = nullptr;
    QLabel* holdings_pnl_caption_ = nullptr;
    QLabel* holdings_pnl_pct_caption_ = nullptr;
    QLabel* holdings_day_pnl_caption_ = nullptr;
    class QPushButton* holdings_import_btn_ = nullptr;
    class QPushButton* holdings_replicate_btn_ = nullptr;
    class QPushButton* holdings_square_off_btn_ = nullptr;
    QVector<trading::BrokerHolding> last_holdings_;
    QTableWidget* orders_table_ = nullptr;
    QDateEdit* orders_date_edit_ = nullptr;        // per-day order book selector
    bool suppress_orders_date_signal_ = false;     // guard during programmatic set
    QTableWidget* calendar_table_ = nullptr;
    QTableWidget* time_sales_table_ = nullptr;
    QString time_sales_symbol_;              // track last symbol to detect change
    QMap<QString, QString> condition_codes_; // code → description cache
    QTableWidget* auctions_table_ = nullptr;

    // Clock banner labels
    QLabel* clock_status_label_ = nullptr;
    QLabel* clock_next_label_ = nullptr;

    // Funds tab — card grid. Index order is documented by FundCard in the .cpp.
    static constexpr int kFundCards = 9;
    QLabel* fund_card_val_[kFundCards] = {};
    QLabel* fund_card_cap_[kFundCards] = {};

    // Stats tab — card grid. Index order is documented by StatCard in the .cpp.
    static constexpr int kStatCards = 12;
    QLabel* stat_card_val_[kStatCards] = {};
    QLabel* stat_card_cap_[kStatCards] = {};

    // Bulk action buttons
    class QPushButton* cancel_all_btn_ = nullptr;
    class QPushButton* close_all_btn_ = nullptr;

    QString account_id_;
    bool is_paper_ = true;
};

} // namespace fincept::screens::equity
