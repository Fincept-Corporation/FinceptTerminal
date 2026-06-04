#pragma once
// Equity Trading Screen — multi-account coordinator.
// Supports multiple broker accounts simultaneously via DataStreamManager.
// Each account has its own data stream (WS/polling), portfolio, and credentials.

#include "core/symbol/IGroupLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "screens/equity_trading/EquityTypes.h"
#include "trading/BrokerAccount.h"
#include "trading/TradingTypes.h"

#include <QEvent>
#include <QHash>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QShowEvent>
#include <QTimer>
#include <QWidget>

#include <atomic>

class QCompleter;
class QStringListModel;

namespace fincept::screens::equity {
class EquityTickerBar;
class EquityWatchlist;
class EquityChart;
class EquityChartPanel;
class EquityOrderEntry;
class EquityOrderBook;
class EquityBottomPanel;
} // namespace fincept::screens::equity

namespace fincept::screens {

class EquityTradingScreen : public QWidget, public IGroupLinked, public IStatefulScreen {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit EquityTradingScreen(QWidget* parent = nullptr);
    ~EquityTradingScreen();

    // IGroupLinked — see WatchlistScreen for the propagation contract.
    void set_group(SymbolGroup g) override { link_group_ = g; }
    SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const SymbolRef& ref) override;
    SymbolRef current_symbol() const override;

    // IStatefulScreen
    QVariantMap save_state() const override;
    void restore_state(const QVariantMap& state) override;
    QString state_key() const override { return QStringLiteral("equity_trading"); }

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private slots:
    // Multi-account slots
    void on_account_changed(const QString& account_id);
    void on_symbol_selected(const QString& symbol);
    // Live instrument-search suggestions for the command-bar symbol box.
    void on_symbol_search_changed(const QString& text);
    void on_mode_toggled();
    void on_accounts_clicked(); // opens AccountManagementDialog
    void handle_token_expired(const QString& account_id);
    void on_order_submitted(const trading::UnifiedOrder& order);
    void on_cancel_order(const QString& order_id);
    void on_cancel_all_orders();                                          // CANCEL ALL ORDERS button
    void on_close_all_positions();                                        // SQUARE OFF ALL button
    void on_strategy_submitted(const trading::BasketOrderRequest& basket); // options strategy → basket
    void on_ob_price_clicked(double price);
    void on_import_holdings_requested(const QVector<trading::BrokerHolding>& holdings);
    // EXIT clicked on the chart's position card → confirm + square off the symbol.
    void on_chart_exit_position(const QString& symbol, const QString& exchange,
                                const QString& product_type, const QString& side, double qty);

    // Named-watchlist management (backed by WatchlistRepository, global lists)
    void on_watchlist_selected(const QString& id);
    void on_watchlist_create(const QString& name);
    void on_watchlist_rename(const QString& id, const QString& name);
    void on_watchlist_delete(const QString& id);
    void on_watchlist_symbol_added(const QString& symbol);
    void on_watchlist_symbol_removed(const QString& symbol);

    // Instruments ready — reloads watchlist/market data for the active account
    // when InstrumentService finishes a load/download for the matching broker.
    void on_instruments_ready(const QString& broker_id);

    void refresh_candles();
    void update_clock();

    // DataStreamManager signal handlers (on-demand / one-shot — kept as legacy signals)
    void on_stream_candles_fetched(const QString& account_id, const QVector<trading::BrokerCandle>& candles);
    void on_stream_orderbook_fetched(const QString& account_id,
                                     const QVector<QPair<double, double>>& bids,
                                     const QVector<QPair<double, double>>& asks,
                                     double spread, double spread_pct,
                                     const QVector<int>& bid_orders,
                                     const QVector<int>& ask_orders);
    void on_stream_time_sales_fetched(const QString& account_id, const QVector<trading::BrokerTrade>& trades);
    void on_stream_latest_trade_fetched(const QString& account_id, const trading::BrokerTrade& trade);
    void on_stream_calendar_fetched(const QString& account_id, const QVector<trading::MarketCalendarDay>& days);
    void on_stream_clock_fetched(const QString& account_id, const trading::MarketClock& clock);

  private:
    void setup_ui();
    void setup_timers();
    void retranslateUi();
    void connect_data_stream_signals();
    void init_focused_account();
    void switch_symbol(const QString& symbol);
    // Parse a (possibly "SYMBOL · EXCH · Broker") command-bar entry and activate it.
    void apply_symbol_input(const QString& raw);
    // Connected broker ids (focused first) for unified instrument search.
    QStringList search_broker_ids() const;
    void update_account_menu();
    void update_connection_status();
    void async_modify_order(const QString& order_id, double qty, double price);

    // Chart right-click trading: pop a confirm ticket (qty + limit price) at the
    // clicked price, then route through on_order_submitted(). Watchlist-add reuses
    // the existing on_watchlist_symbol_added() slot for the charted symbol.
    void on_chart_buy_requested(double price);
    void on_chart_sell_requested(double price);
    void on_chart_add_to_watchlist();
    void open_chart_order_ticket(bool is_buy, double price);
    // Re-reads the focused account's paper portfolio into the panels. No-op for
    // live accounts (their data flows from AccountDataStream via the hub).
    void refresh_paper_panels();
    void flush_paper_prices(); // persist buffered paper position prices to SQLite

    // DataHub subscription helpers (D4 migration)
    void hub_subscribe_streaming();
    void hub_unsubscribe_all();
    void hub_subscribe_quotes();
    QString broker_id_for_focused() const;

    // Named-watchlist controller (WatchlistRepository). load_watchlists() refreshes
    // the combo + seeds a default from the broker's default_watchlist on first run;
    // apply_active_watchlist() pushes the active list's symbols into the view and
    // (when resubscribe=true) re-points the live stream + hub at the new symbol set.
    void load_watchlists();
    void apply_active_watchlist(bool resubscribe);
    // Effective live-subscription set = active watchlist ∪ open-position symbols,
    // so symbols you hold (even if not in the list) get live WebSocket prices.
    QStringList effective_symbols() const;
    void update_position_symbols(const QStringList& syms); // re-subscribe when it changes
    // Push the open position for the displayed symbol onto the chart's overlay
    // card + entry line (from the live cache or the paper engine), or clear it
    // when the symbol is flat.
    void update_chart_position();

    // Ensure the broker instrument master for `account_id` is loaded into
    // InstrumentService (from SQLite cache or a fresh download). Market data
    // (quotes/charts/depth) needs the numeric securityId map, which only exists
    // once instruments are loaded. Mirrors the ChainSubTab load pattern.
    void ensure_instruments_loaded(const QString& account_id);

    // ── Command bar widgets ──
    QPushButton* account_btn_ = nullptr;  // shows focused account name
    QMenu* account_menu_ = nullptr;       // lists all active accounts
    QLineEdit* symbol_input_ = nullptr;
    QCompleter* symbol_completer_ = nullptr;             // dynamic instrument-search popup
    QStringListModel* symbol_completer_model_ = nullptr; // suggestions, refreshed per keystroke
    QPushButton* mode_btn_ = nullptr;
    QPushButton* accounts_btn_ = nullptr; // opens AccountManagementDialog
    QLabel* exchange_label_ = nullptr;
    QLabel* clock_label_ = nullptr;
    QLabel* conn_label_ = nullptr;        // aggregate connection status

    // ── Sub-widgets ──
    equity::EquityTickerBar* ticker_bar_ = nullptr;
    equity::EquityWatchlist* watchlist_ = nullptr;
    equity::EquityChartPanel* chart_ = nullptr;
    equity::EquityOrderEntry* order_entry_ = nullptr;
    equity::EquityOrderBook* orderbook_ = nullptr;
    equity::EquityBottomPanel* bottom_panel_ = nullptr;

    // ── Timers (only UI-local timers remain; data timers are in AccountDataStream) ──
    QTimer* clock_timer_ = nullptr;
    QTimer* market_clock_timer_ = nullptr;

    // ── Multi-account state ──
    QString focused_account_id_;          // the account targeted by order entry + chart
    QString selected_symbol_ = "RELIANCE";
    QString selected_exchange_ = "NSE";

    // Paper trading (derived from focused account)
    int fill_cb_id_ = -1; // OrderMatcher fill callback
    // Cached focused-account trading context. Refreshed on every focus / symbol /
    // mode change (hub_subscribe_streaming) and on every paper refresh, so the
    // per-tick quote handler avoids a mutex-locked BrokerAccount copy per tick.
    bool focused_is_paper_ = false;
    QString focused_paper_portfolio_id_;
    // Paper engine price persistence is coalesced off the per-tick GUI hot path:
    // the latest LTP per symbol is buffered here and flushed to SQLite on a timer
    // (and synchronously before refresh_paper_panels reads), instead of one DB
    // UPDATE per tick. SL/TP + limit matching still run every tick (in-memory).
    QHash<QString, double> pending_paper_prices_;
    bool paper_flush_armed_ = false;

    // Async guards
    std::atomic<bool> token_expired_shown_{false};

    QStringList watchlist_symbols_;
    QString active_watchlist_id_; // current WatchlistRepository list id
    QStringList position_symbols_; // symbols with open positions (transient, for live pricing)
    QVector<trading::BrokerPosition> live_positions_; // cached live positions for focused account
    double current_price_ = 0.0;  // last known LTP for selected symbol

    bool initialized_ = false;
    bool hub_active_ = false;

    // Symbol group link — SymbolGroup::None when unlinked.
    SymbolGroup link_group_ = SymbolGroup::None;
};

} // namespace fincept::screens
