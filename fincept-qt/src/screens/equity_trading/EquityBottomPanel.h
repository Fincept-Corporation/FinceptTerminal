#pragma once
// Equity Bottom Panel — tabbed: Positions, Holdings, Orders, Funds, Stats

#include "screens/equity_trading/EquityTypes.h"
#include "trading/BrokerInterface.h"
#include "trading/TradingTypes.h"

#include <QDate>
#include <QEvent>
#include <QHash>
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
    ~EquityBottomPanel() override;

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

    // The account's ISO currency code ("INR", "USD"): every money figure of the
    // panel (P&L, invested / current, totals, fills' fee and realised P&L) is in it
    // and formatted by ui/formatting in the user's style. Empty is ignored.
    void set_currency(const QString& currency_code);

    // Show/hide the US-market-only tabs (Time & Sales, Auctions, Calendar). These
    // carry tape/condition/auction data that only US brokers (Alpaca etc.) provide,
    // so they're hidden for Indian and other non-US brokers.
    void set_us_market_tabs_visible(bool visible);

    // Pages owned by the screen (FEEDS, ALL ACCOUNTS), as tabs after the blotter's
    // own. add_extra_tab() re-parents `page` into the tab stack (a page re-parented
    // away later — e.g. the monitor floated into its own window — loses its tab).
    void add_extra_tab(QWidget* page, const QString& title);
    void set_extra_tab_visible(QWidget* page, bool visible);
    // Make `page`'s tab current (shown, and the sheet expanded).
    void show_extra_tab(QWidget* page);
    // Deep link: make a blotter tab current by its stable id ("positions",
    // "holdings", "orders", "fills", "funds", "stats"). False when unknown or not
    // shown for this account (e.g. FILLS outside paper mode).
    bool show_section(const QString& id);

    // Collapse the sheet down to just its tab-bar header (and back).
    void toggle_collapsed();
    bool is_collapsed() const { return collapsed_; }

    // Explanatory empty states. While a table is empty it shows the matching text
    // centred over it instead of a blank grid: `account_text` for Positions /
    // Holdings / Orders, `tape_text` for the broker-only Time & Sales / Auctions /
    // Calendar tabs. Empty strings restore the plain (blank) tables.
    void set_empty_notice(const QString& account_text, const QString& tape_text);

    // ── Multi-currency paper rows (built-in paper account) ──────────────────
    // Per symbol: the price currency (as quoted, e.g. "USD", "GBp"), price decimals
    // and the factor to the account currency. Prices (avg / LTP) are shown in the
    // instrument's currency; money (P&L, invested, current, totals) in the account
    // currency (set_currency) after the factor. Symbols without an entry keep the
    // single-currency behaviour. A non-finite `fx` means "no rate yet": that row's
    // money shows "—" and the totals leave it out (and say so) — never 1:1.
    struct SymbolMeta {
        QString currency;
        int digits = -1;
        double fx = 1.0;
    };
    void set_symbol_meta(const QHash<QString, SymbolMeta>& meta);
    // Simulated fills of the paper account (FILLS tab, shown in paper mode).
    // `note` is shown on every row ("SIMULATED · delayed price").
    void set_paper_trades(const QVector<trading::PtTrade>& trades, const QString& note);

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
    // `quantity` is the held quantity shown on the row (what the confirmation names).
    void square_off_holding_requested(const QString& symbol, const QString& exchange, double quantity);
    // Paper: convert an open position's product in place (MIS -> CNC), identified
    // by its paper position id.
    void convert_position_requested(const QString& position_id, const QString& symbol, const QString& new_product);
    // Orders tab date-selector changed — request that IST day's order book.
    void orders_day_changed(const QDate& day);
    // Square off the subset of open positions by P&L sign (+1 winners, -1 losers).
    // `targets` are the rows picked at click time from the same tick-updated P&L the
    // header chips show (and listed in the confirmation) — exactly what gets closed.
    void square_off_group_requested(const QString& account_id, int sign,
                                    const QVector<trading::BrokerPosition>& targets);
    // Buy(add)/Sell(reduce) on a position or holding row — from the right-click
    // menu or the per-row SELL button. `exchange` and `product` are the row's own
    // (broker wire spelling for product); `qty` pre-fills the order ticket (the held
    // quantity for a reduce/exit, unrounded; 0 = let the ticket default to 1).
    void trade_symbol_requested(const QString& symbol, const QString& exchange, const QString& product, bool is_buy,
                                double qty);
    // Click a position/holding row → load that symbol's chart (like the watchlist).
    void chart_symbol_requested(const QString& symbol);

  protected:
    void changeEvent(QEvent* event) override;
    // Keeps the empty-state labels sized to their table viewports.
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    void retranslateUi();
    void apply_collapsed();

    // Empty-state overlays (set_empty_notice). The label is a child of the TABLE,
    // laid over its viewport — not a child of the viewport, whose contents scroll.
    struct EmptyState {
        QTableWidget* table = nullptr;
        QLabel* label = nullptr;
        bool tape = false; // tape_text (true) or account_text (false)
    };
    void install_empty_state(QTableWidget* table, bool tape);
    void refresh_empty_state(const EmptyState& s);
    QVector<EmptyState> empty_states_;
    QString account_notice_;
    QString tape_notice_;
    void setup_positions_tab();
    // Adds the Holdings page as an empty shell; build_holdings_tab() fills it the
    // first time the tab is shown (it was the largest cost of opening the screen).
    void setup_holdings_tab();
    void build_holdings_tab();
    // Paint last_holdings_ into the holdings table + summary strip (built only).
    void populate_holdings_view();
    void setup_orders_tab();
    void setup_funds_tab();
    void setup_stats_tab();
    void setup_calendar_tab();
    void setup_time_sales_tab();
    void setup_auctions_tab();

    // Recompute the positions action-bar summary (net P&L + winners/losers chips)
    // from the current backing vectors. Called on every (re)load and quote patch.
    void update_positions_summary();

    void setup_fills_tab();
    // Price text in the symbol's own currency (SymbolMeta), else the panel currency.
    QString px_text(const QString& symbol, double v) const;
    // Factor price currency -> account currency: 1 without an entry, NaN when the
    // entry has no rate yet.
    double meta_fx(const QString& symbol) const;
    // Tooltip of a money cell left blank for want of a rate.
    QString fx_missing_tip(const QString& symbol) const;
    // Settings > Currency & Number Format changed: repaint every tab from the
    // cached payloads below (no fetch, no reset).
    void rerender_from_cache();
    // The holdings table's money headers carry the account currency ("Invested (₹)").
    void apply_money_headers();
    // The quantity / price edit dialog shared by broker and paper orders.
    void open_modify_dialog(const QString& order_id, double qty, double price, const QString& price_caption);
    QHash<QString, SymbolMeta> meta_;
    QTableWidget* fills_table_ = nullptr;
    int fills_tab_idx_ = -1;

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
    QWidget* make_positions_action_cell(const QString& symbol, const QString& exchange, const QString& product,
                                        double qty, const QString& side, bool show_convert, const QString& paper_pid);

    // Open positions currently in profit (sign > 0) or loss (sign < 0), from the
    // tick-updated rows the header chips are computed from. Flat (qty 0) rows and
    // paper delivery rows are excluded. Paper rows carry no exchange.
    QVector<trading::BrokerPosition> square_off_group_targets(int sign) const;
    // Confirm + emit square_off_group_requested for the winners / losers group.
    void request_square_off_group(int sign);

    QTabWidget* tabs_ = nullptr;

    // Collapsible bottom-sheet state
    class QToolButton* collapse_btn_ = nullptr; // ▾/▴ toggle in the tab-bar corner
    QWidget* tab_content_ = nullptr;            // QTabWidget's page stack; hidden when collapsed
    bool collapsed_ = false;
    QList<int> saved_split_sizes_; // parent splitter sizes to restore on expand

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
    QVector<trading::BrokerPosition> last_positions_;   // live mode
    QVector<trading::PtPosition> last_paper_positions_; // paper mode

    // Positions action-bar summary: net P&L + winners/losers chips, each with a
    // group square-off button. Updated by update_positions_summary().
    QLabel* positions_total_pnl_label_ = nullptr;
    QLabel* positions_win_label_ = nullptr;
    QLabel* positions_loss_label_ = nullptr;
    class QPushButton* win_squareoff_btn_ = nullptr;
    class QPushButton* loss_squareoff_btn_ = nullptr;
    QString currency_; // account ISO code (set_currency); empty = unknown, amounts without a unit

    // Last payloads, kept to re-render on a format change (rerender_from_cache).
    QVector<trading::BrokerOrderInfo> last_orders_;
    QVector<trading::PtOrder> last_paper_orders_;
    bool orders_are_paper_ = false;
    QVector<trading::PtTrade> last_paper_trades_;
    QString last_trades_note_;
    bool have_paper_trades_ = false;
    EquityFundsView last_funds_view_;
    bool have_funds_view_ = false;
    EquityStatsView last_stats_view_;
    bool have_stats_view_ = false;

    QWidget* holdings_page_ = nullptr;          // tab page; contents built lazily
    QTableWidget* holdings_table_ = nullptr;    // null until build_holdings_tab()
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
    QDateEdit* orders_date_edit_ = nullptr;    // per-day order book selector
    bool suppress_orders_date_signal_ = false; // guard during programmatic set
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
    // Mirrors set_us_market_tabs_visible(); the AUCTIONS tab additionally needs rows to show.
    bool us_tabs_visible_ = true;
};

} // namespace fincept::screens::equity
