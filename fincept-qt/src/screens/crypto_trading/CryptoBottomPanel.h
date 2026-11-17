#pragma once
// Crypto Bottom Panel — tabbed: Positions, Orders, Trades, My Trades, Fees, Market Info, Stats, Time&Sales, Depth

#include "screens/crypto_trading/CryptoTypes.h"
#include "trading/TradingTypes.h"

#include <QEvent>
#include <QJsonArray>
#include <QLabel>
#include <QSet>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QVector>
#include <QWidget>

namespace fincept::ui {
class EmptyState;
class KpiStrip;
} // namespace fincept::ui

namespace fincept::screens::crypto {

class CryptoTimeSales;
class CryptoDepthChart;

class CryptoBottomPanel : public QWidget {
    Q_OBJECT
  public:
    explicit CryptoBottomPanel(QWidget* parent = nullptr);

    // Paper mode data
    void set_positions(const QVector<trading::PtPosition>& positions);
    void set_orders(const QVector<trading::PtOrder>& orders);
    /// `ccy` = the paper portfolio's currency, which fees and realized P&L are
    /// booked in (prices stay in each pair's quote asset); empty keeps the last one.
    void set_trades(const QVector<trading::PtTrade>& trades, const QString& ccy = QString());
    /// `ccy` = the paper portfolio's currency (money tiles); empty keeps the last one.
    /// `quote_assets` = the distinct quote assets of the portfolio's trades: the
    /// engine sums TURNOVER in each trade's own quote asset, so it is shown only
    /// when there is one (converted to `ccy` with its rate), never as a mixed
    /// sum. Empty = not known (turnover "—" unless it is zero).
    void set_stats(const trading::PtStats& stats, const QString& ccy = QString(),
                   const QStringList& quote_assets = QStringList());
    void set_market_info(const MarketInfoData& info);
    /// The market the tape and the depth chart come from (its quote asset is
    /// their price unit).
    void set_price_market(const QString& market);

    // Live mode data
    void set_live_positions(const QJsonArray& positions);
    void set_live_orders(const QJsonArray& orders);
    /// A LIVE cancel is on its way: the order's row reads "CANCELLING…" and its cancel
    /// button is disabled until the exchange answers (`on` false), across refreshes.
    void set_order_cancelling(const QString& order_id, bool on);
    /// `ccy` = the currency the balance is held in (the pair's quote or a stable fallback).
    void set_live_balance(double balance, double equity, double used_margin, const QString& ccy = QString());
    /// Live balance could not be fetched (bad API key / daemon error). Shows an
    /// explicit UNAVAILABLE state instead of a misleading $0.00 (which is
    /// indistinguishable from a genuinely empty account). `reason` goes into a
    /// tooltip.
    void set_balance_unavailable(const QString& reason);
    /// Paper ↔ live. Paper-only views (HISTORY tab, the trade-statistics cards)
    /// are hidden / labelled PAPER while live, so paper figures never read as live.
    void set_mode(bool is_paper);
    /// Blank every account-specific view (positions, orders, history, my trades,
    /// stats, live balance) — called on a paper↔live switch so the previous mode's
    /// rows don't linger until (or instead of) the new mode's first refresh.
    void clear_account_views();

    // New live tabs
    void update_my_trades(const QJsonObject& data);
    void update_fees(const QJsonObject& data);

    // New widget forwarding
    void add_trade_entry(const TradeEntry& trade);
    /// Recent prints fetched over REST on a symbol / exchange switch, so the
    /// time & sales tape is not empty until the first streamed trade.
    void seed_trade_history(const QVector<TradeEntry>& trades);
    /// Symbol / exchange switch: drop the time & sales tape, the depth chart and the
    /// MARKET-tab readouts so the previous pair's data isn't mixed with (or shown
    /// as) the new pair's until its first updates arrive.
    void reset_market_data();
    /// Live positions / orders fetch failed (no API key, bad credentials, daemon
    /// error, venue without positions): show the reason in the empty state instead
    /// of a table that reads as "no positions" (or still shows paper rows).
    void set_live_positions_unavailable(const QString& reason);
    void set_live_orders_unavailable(const QString& reason);
    void set_depth_data(const QVector<QPair<double, double>>& bids, const QVector<QPair<double, double>>& asks,
                        double spread, double spread_pct);

    /// Patch the positions table with fresh WS ticker prices. Works in both
    /// paper and live mode — recalculates current_price, unrealized PnL, and
    /// PnL% from the latest mark price without hitting SQLite or the daemon.
    /// Call at every WS flush tick.
    void update_position_prices(const QHash<QString, double>& last_prices);

    void set_account_id(const QString& account_id);
    /// LIVE: the market the bulk buttons act on (named in their text). Empty:
    /// none known yet — the buttons then name the whole account.
    void set_scope_market(const QString& market);

    // ── Designed states (no tab opens blank) ──
    /// The API-key state of the exchange on screen. Decides what an empty or
    /// failed LIVE view (and MY TRADES in PAPER) says: stored keys that could not
    /// be read (`read_error`, ExchangeSession::credentials_read_error()) → "Stored
    /// API keys unreadable" + RETRY (re-reads them); no key → ADD API KEY; else the
    /// fetch error + RETRY.
    void set_live_key_state(const QString& exchange, bool has_keys, const QString& read_error);
    /// The paper account could not be loaded: POSITIONS / ORDERS / HISTORY say why + RETRY.
    void set_paper_unavailable(const QString& reason);
    /// The pair's fee schedule is being fetched (FEES table and the MAKER / TAKER tile).
    void set_fees_loading();
    /// The venue reports no funding / mark / index figures for the pair (a spot
    /// market, or not offered): the MARKET tiles say `why` instead of loading.
    void set_funding_unavailable(const QString& why);
    /// Same for open interest.
    void set_open_interest_unavailable(const QString& why);
    /// FEES or MARKET is the tab on screen (both show the fee schedule).
    bool fee_view_visible() const;
    /// MARKET is the tab on screen.
    bool market_view_visible() const;
    /// Select a tab by its stable id (a deep link from the header search box):
    /// positions | orders | history | my_trades | fees | market | stats | tape | depth.
    /// The tab's own switch path runs (FEES / MARKET ask for their figures as on a
    /// click). False for an unknown id or a tab hidden in this mode (HISTORY is PAPER only).
    bool show_section(const QString& key);
    /// Why the live stream is down ("" = it is up): the empty T&S tape and depth
    /// chart say so instead of waiting forever.
    void set_live_feed_error(const QString& message);

  signals:
    /// "NEW ORDER" on an empty positions / orders / history tab: focus the ticket.
    void order_ticket_requested();
    /// RETRY on a failed LIVE positions / orders / fills view.
    void live_refresh_requested();
    /// ADD API KEY: open the exchange's API key dialog.
    void api_key_requested();
    /// RETRY on "Stored API keys unreadable": read the stored keys again.
    void credentials_retry_requested();
    /// FEES / MARKET shown and the pair's schedule is not loaded (`force` = RETRY).
    void fees_wanted(bool force);
    /// MARKET shown: funding / open interest wanted for the pair on screen.
    void market_info_wanted();
    /// RETRY on "Paper account not loaded".
    void paper_retry_requested();
    /// `symbol` is the order's own market as reported by the exchange / paper engine
    /// (may differ from the screen's selected display pair on remapped venues).
    void cancel_order_requested(const QString& order_id, const QString& symbol);
    void close_position_requested(const QString& symbol);
    void cancel_all_orders_requested(const QString& account_id);
    void close_all_positions_requested(const QString& account_id);

  protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

  private:
    void retranslateUi();
    void apply_mode_labels(); // paper/live-dependent tab visibility + stat card titles
    void setup_positions_tab();
    void setup_orders_tab();
    void setup_trades_tab();
    void setup_my_trades_tab();
    void setup_fees_tab();
    void setup_market_info_tab();
    void setup_stats_tab();

    static QTableWidgetItem* ensure_item(QTableWidget* table, int row, int col);
    /// Page 0 (the table) with rows, else page 1 (the designed empty / unavailable state).
    void update_empty_state(QTableWidget* table, QStackedWidget* stack, int row_count);
    /// Page 2: a table skeleton while the first rows of this mode are on their way.
    static void show_table_loading(QStackedWidget* stack);

    // Designed state of each table tab (page 1), from what is known now.
    void apply_positions_empty();
    void apply_orders_empty();
    void apply_trades_empty();
    void apply_my_trades_empty();
    void apply_fees_empty();
    void apply_all_empty_states();
    /// A LIVE view of `what` ("positions", …) is unavailable for `reason`: the
    /// spec + action, by the key state (unreadable keys / no key / fetch error).
    void apply_live_unavailable(ui::EmptyState* es, const QString& what, const QString& reason);
    /// An EmptyState's buttons → the action stored on it (dynamic properties).
    void on_empty_action(ui::EmptyState* es, bool primary);
    void on_tab_changed(int index);

    // ── Format change: re-render from the last data held (never re-fetch) ──
    void rerender_from_cache();
    void render_stats();
    void render_market_info();
    void render_live_balance();
    /// P&L column header: names the unit when every row is in one currency.
    void update_positions_pnl_header();

    QVector<trading::PtPosition> paper_positions_;
    QJsonArray live_positions_;
    bool positions_live_ = false;
    QHash<QString, double> last_marks_; // newest WS marks patched in by update_position_prices()
    QStringList positions_ccys_;        // P&L currency of each positions row (quote / settle asset)
    QVector<trading::PtOrder> paper_orders_;
    QJsonArray live_orders_;
    bool orders_live_ = false;
    QSet<QString> cancelling_ids_; // LIVE orders with a cancel in flight (set_order_cancelling)
    QVector<trading::PtTrade> paper_trades_;
    QString trades_ccy_; // paper portfolio currency of fees / realized P&L
    trading::PtStats stats_;
    bool has_stats_ = false;
    QStringList stats_quote_assets_;
    MarketInfoData market_info_;
    QJsonObject my_trades_json_;
    bool has_my_trades_ = false;
    QJsonObject fees_json_;
    bool has_fees_ = false;
    double live_balance_ = 0.0;
    double live_equity_ = 0.0;
    double live_used_margin_ = 0.0;
    QString live_balance_ccy_;
    bool has_live_balance_ = false;

    QTabWidget* tabs_ = nullptr;

    QTableWidget* positions_table_ = nullptr;
    QTableWidget* orders_table_ = nullptr;
    QTableWidget* trades_table_ = nullptr;
    QTableWidget* my_trades_table_ = nullptr;
    QTableWidget* fees_table_ = nullptr;

    // Each table is wrapped in a QStackedWidget so we can swap between the
    // table (page 0) and an empty-state placeholder (page 1) when the data
    // set is empty. Avoids the "blank table with header only" UX.
    QStackedWidget* positions_stack_ = nullptr;
    QStackedWidget* orders_stack_ = nullptr;
    QStackedWidget* trades_stack_ = nullptr;
    QStackedWidget* my_trades_stack_ = nullptr;
    QStackedWidget* fees_stack_ = nullptr;

    // Page 1 of each stack: the designed empty / unavailable state.
    ui::EmptyState* positions_empty_ = nullptr;
    ui::EmptyState* orders_empty_ = nullptr;
    ui::EmptyState* trades_empty_ = nullptr;
    ui::EmptyState* my_trades_empty_ = nullptr;
    ui::EmptyState* fees_empty_ = nullptr;

    // What the designed states say.
    QString positions_reason_;  ///< LIVE positions fetch error ("" = none)
    QString orders_reason_;     ///< LIVE open-orders fetch error
    QString my_trades_reason_;  ///< LIVE fills fetch error
    QString paper_error_;       ///< paper account could not be loaded
    QString fees_error_;        ///< fee schedule fetch error
    bool fees_unsupported_ = false; ///< the venue publishes no schedule for the pair (not an error)
    bool fees_loading_ = false;
    bool fees_known_ = false;   ///< maker / taker of the pair on screen are known
    double fees_maker_ = 0.0;
    double fees_taker_ = 0.0;
    bool fees_default_schedule_ = false;
    QString live_exchange_;     ///< exchange named in the key states ("KRAKEN")
    bool live_has_keys_ = false;
    QString live_read_error_;   ///< stored keys exist but could not be read

    // Tab indices for the host containers (cached for retranslateUi tab text)
    int positions_tab_idx_ = -1;
    int orders_tab_idx_ = -1;
    int trades_tab_idx_ = -1;
    int my_trades_tab_idx_ = -1;
    int fees_tab_idx_ = -1;
    int time_sales_tab_idx_ = -1;
    int depth_tab_idx_ = -1;
    int market_tab_idx_ = -1;
    int stats_tab_idx_ = -1;

    // Market Info: six tiles (funding, mark, index, open interest, maker / taker,
    // next funding) — Loading until their figures arrive, "—" with the reason
    // when the venue has none for the pair (spot markets have no funding).
    ui::KpiStrip* market_strip_ = nullptr;
    QString funding_na_; ///< why funding / mark / index / next funding are not available ("" = unknown yet)
    QString oi_na_;      ///< why open interest is not available

    // Stats: paper trade statistics as a KPI strip, and the live account strip
    // (LIVE BALANCE / LIVE EQUITY / USED MARGIN, shown in LIVE mode only).
    ui::KpiStrip* stats_strip_ = nullptr;
    ui::KpiStrip* live_strip_ = nullptr;
    ui::EmptyState* stats_empty_ = nullptr; ///< "No closed paper trades yet" (instead of a row of zeros)
    QString stats_ccy_; // the paper portfolio's currency (set_stats); empty until known

    // Bulk action buttons
    class QPushButton* cancel_all_btn_ = nullptr;
    class QPushButton* close_all_btn_ = nullptr;

    // New widgets
    CryptoTimeSales* time_sales_ = nullptr;
    CryptoDepthChart* depth_chart_ = nullptr;

    QString account_id_;
    QString scope_market_;
    bool is_paper_ = true;
    void update_bulk_labels();
};

} // namespace fincept::screens::crypto
