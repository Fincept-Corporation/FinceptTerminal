#pragma once
// Equity Trading Screen — multi-account coordinator.
// Supports multiple broker accounts simultaneously via DataStreamManager.
// Each account has its own data stream (WS/polling), portfolio, and credentials.

#include "core/instrument/Instrument.h"
#include "core/symbol/IGroupLinked.h"
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "screens/equity_trading/EquityLayoutPlan.h"
#include "screens/equity_trading/EquityPublicFeed.h"
#include "screens/equity_trading/EquitySymbolBox.h"
#include "screens/equity_trading/EquityTypes.h"
#include "services/markets/MarketDataTypes.h"
#include "trading/BrokerAccount.h"
#include "trading/PaperTrading.h"
#include "trading/TradingTypes.h"

#include <optional>

#include <QDate>
#include <QEvent>
#include <QHash>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QSet>
#include <QShowEvent>
#include <QTimer>
#include <QWidget>

#include <atomic>

class QCompleter;
class QStringListModel;
class QSplitter;

namespace fincept::feeds {
class FeedPanel;
}

namespace fincept::screens {
class PortfolioMonitorScreen;
}

namespace fincept::ui {
class ScreenStatusBar;
}

namespace fincept::screens::equity {
class EquityTickerBar;
class EquityWatchlist;
class EquityChart;
class EquityChartPanel;
class EquityOrderEntry;
class EquityOrderBook;
class EquityBottomPanel;
class EquityQuoteSnapshot;
class EquityCommandBar;
class EquityWorkspace;
class EquityBarButton;
class EquityFitLabel;
} // namespace fincept::screens::equity

namespace fincept::screens {

class EquityTradingScreen : public QWidget, public IGroupLinked, public IStatefulScreen, public ISectionLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit EquityTradingScreen(QWidget* parent = nullptr);
    ~EquityTradingScreen();

    // IGroupLinked — see WatchlistScreen for the propagation contract. An inert
    // twin (see EquitySingleInstance.h) forwards all four to the panel's real screen.
    void set_group(SymbolGroup g) override;
    SymbolGroup group() const override;
    void on_group_symbol_changed(const SymbolRef& ref) override;
    SymbolRef current_symbol() const override;

    // IStatefulScreen
    QVariantMap save_state() const override;
    void restore_state(const QVariantMap& state) override;
    QString state_key() const override { return QStringLiteral("equity_trading"); }

    // ISectionLinked — deep link into a zone / blotter tab: "watchlist", "chart",
    // "positions", "holdings", "orders", "fills", "funds", "stats", "ticket"
    // (focus "buy" / "sell" picks the ticket's side). Unknown section -> false.
    // Never places or cancels anything.
    bool open_section(const QString& section, const QString& focus) override;

    // External entry point — opens the SAME quick order ticket used in-screen for
    // an arbitrary symbol/side, driven from another screen (e.g. Equity Research).
    // Ensures a broker account is focused (the credential-aware pick this tab uses
    // on show) and seeds a reference price the caller already has, so paper market
    // orders fill even when this screen's own quote stream hasn't ticked for the
    // symbol. The trading tab's own symbol selection is saved and restored, so this
    // never disturbs an in-progress trade here. Reuses open_order_ticket_for() +
    // on_order_submitted() — no duplicate form, no duplicate placement logic.
    // match_exchanges are the broker exchanges that can trade the symbol's market
    // (e.g. {"NSE"} or the US venues); the order routes to a usable account whose
    // broker serves one of them, so multi-broker setups pick the right account.
    // Prefill (payload of equity.open_order_ticket): `quantity` > 0 = the ticket's
    // quantity (0 = its default), `order_type` "market" / "limit" ("" = its default),
    // `limit_price` > 0 = the limit (a typed limit, never the reference `ref_price`).
    // Prefill ONLY: the values are validated as if typed (lot, tick, minimum — shown
    // inline, never silently clamped); the user presses BUY / SELL and the screen's own
    // confirmation and the approval gate decide. Nothing is sent from here.
    void open_external_order_ticket(const QString& symbol, const QString& exchange, const QStringList& match_exchanges,
                                    bool is_buy, double ref_price, double quantity = 0.0,
                                    const QString& order_type = QString(), double limit_price = 0.0);

    /// True for a second screen the shell built into the same panel by re-entering
    /// its factory: it stays empty and hidden and forwards to the first one.
    bool is_inert_twin() const { return inert_; }

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private slots:
    // Multi-account slots
    void on_account_changed(const QString& account_id);
    void on_symbol_selected(const QString& symbol);
    // An instrument picked in the command-bar symbol search.
    void on_symbol_picked(const fincept::core::instrument::Instrument& inst);
    void on_mode_toggled();
    void on_accounts_clicked(); // opens AccountManagementDialog
    void handle_token_expired(const QString& account_id);
    void on_order_submitted(const trading::UnifiedOrder& order);
    // Inline multi-broker submit: confirm, gate per account (Semi-Auto), then
    // broadcast the same order to every selected account on a worker thread.
    void on_multi_broker_submit(const trading::UnifiedOrder& order, const QStringList& account_ids);
    void on_cancel_order(const QString& order_id);
    void on_cancel_all_orders();   // CANCEL ALL ORDERS button
    void on_close_all_positions(); // SQUARE OFF ALL button (positions)
    // SQUARE OFF ALL button on the Holdings tab — market-sells every holding
    // (CNC/delivery). Acts ONLY on holdings; positions are untouched.
    void on_square_off_all_holdings(const QVector<trading::BrokerHolding>& holdings);
    // Per-row SELL on the Holdings tab — confirm then close that single holding.
    // `quantity` is the held quantity the row shows (named in the confirmation and sent).
    void on_square_off_holding(const QString& symbol, const QString& exchange, double quantity);
    void on_strategy_submitted(const trading::BasketOrderRequest& basket); // options strategy → basket
    void on_ob_price_clicked(double price);
    void on_import_holdings_requested(const QVector<trading::BrokerHolding>& holdings);
    // Replicate a source account's portfolio into a paper account (opens dialog).
    void on_replicate_portfolio_requested();
    // Paper: convert a position's product in place (MIS -> CNC) then refresh.
    void on_convert_position(const QString& position_id, const QString& symbol, const QString& new_product);
    // Orders tab date-selector changed — show that IST day's paper order book.
    void on_orders_day_changed(const QDate& day);
    // Square off the subset of open positions by P&L sign (+1 winners, -1 losers).
    // `targets` are the rows the panel picked and listed in its confirmation.
    void on_square_off_group(const QString& account_id, int sign, const QVector<trading::BrokerPosition>& targets);
    // Buy/Sell on a position or holding (right-click or per-row SELL button) →
    // open an order ticket for the row's own exchange + product. `qty` pre-fills the
    // ticket (held qty on a reduce/exit).
    void on_trade_symbol_requested(const QString& symbol, const QString& exchange, const QString& product, bool is_buy,
                                   double qty);
    // A broker account just connected: load its instrument master and start / focus its
    // stream. Driven by the ACCOUNTS dialog's credentials_saved, and — for a connect that
    // completes after that dialog was closed — by AccountManager::credentials_changed.
    void on_credentials_saved(const QString& account_id);
    // EXIT clicked on the chart's position card → confirm + square off the symbol.
    void on_chart_exit_position(const QString& symbol, const QString& exchange, const QString& product_type,
                                const QString& side, double qty);

    // Named-watchlist management (backed by WatchlistRepository, global lists)
    void on_watchlist_selected(const QString& id);
    void on_watchlist_create(const QString& name);
    void on_watchlist_rename(const QString& id, const QString& name);
    void on_watchlist_delete(const QString& id);
    void on_watchlist_symbol_added(const QString& symbol);
    void on_watchlist_instrument_added(const fincept::core::instrument::Instrument& inst);
    void on_watchlist_symbol_removed(const QString& symbol);

    // Instruments ready — reloads watchlist/market data for the active account
    // when InstrumentService finishes a load/download for the matching broker.
    void on_instruments_ready(const QString& broker_id);

    void refresh_candles();

    // DataStreamManager signal handlers (on-demand / one-shot — no hub topic). The
    // market-data ones use the symbol(+timeframe)-tagged signals and drop payloads
    // that are no longer for the selected symbol / timeframe / account / source.
    void on_stream_candles_for_symbol(const QString& account_id, const QString& symbol, const QString& timeframe,
                                      const QVector<trading::BrokerCandle>& candles);
    void on_stream_candles_failed(const QString& account_id, const QString& symbol, const QString& timeframe,
                                  const QString& reason);
    void on_stream_orderbook_for_symbol(const QString& account_id, const QString& symbol,
                                        const QVector<QPair<double, double>>& bids,
                                        const QVector<QPair<double, double>>& asks, double spread, double spread_pct,
                                        const QVector<int>& bid_orders, const QVector<int>& ask_orders);
    void on_stream_orderbook_failed(const QString& account_id, const QString& symbol, const QString& reason);
    void on_stream_time_sales_for_symbol(const QString& account_id, const QString& symbol,
                                         const QVector<trading::BrokerTrade>& trades);
    void on_stream_time_sales_failed(const QString& account_id, const QString& symbol, const QString& reason);
    void on_stream_latest_trade_for_symbol(const QString& account_id, const QString& symbol,
                                           const trading::BrokerTrade& trade);
    void on_stream_calendar_fetched(const QString& account_id, const QVector<trading::MarketCalendarDay>& days);
    void on_stream_clock_fetched(const QString& account_id, const trading::MarketClock& clock);

  private:
    // Multi-leg safety: a basket is sent leg by leg (not atomic). Confirm a LIVE
    // strategy before sending; when it partially fails, show which legs went out
    // and offer a one-click unwind of the placed legs.
    bool confirm_live_basket(const QString& account_id, const trading::BasketOrderRequest& basket);
    void offer_basket_unwind(const QString& account_id, const QString& strategy_name,
                             const trading::BasketOrderResult& result);

    // See EquitySingleInstance.h: this build is the panel's second screen.
    void become_inert_twin(EquityTradingScreen* primary);
    // Hidden for a whole event-loop turn: stop streams, hub topics, the public feed.
    void pause_streams();
    void setup_ui();
    void setup_command_bar();
    void setup_status_bar();
    void install_shortcuts();
    // Command-bar actions (BUY / SELL / TICKET / ACCOUNTS / WATCHLIST …).
    void on_bar_action(const QString& id);
    // BUY / SELL from the bar or B / S: the ticket on that side, reachable at every width.
    void open_ticket_side(bool is_buy);
    // Width class changed (EquityWorkspace): depth levels, ticket reachability.
    void apply_width_plan();
    // FEEDS / ALL ACCOUNTS: built on first use, shown as blotter tabs.
    fincept::feeds::FeedPanel* ensure_feed_panel();
    PortfolioMonitorScreen* ensure_monitor_panel();
    void setup_timers();
    void retranslateUi();
    void connect_data_stream_signals();
    void init_focused_account();
    // Ensure a configured broker is focused (saved → active → any configured).
    // Called on every show so a broker configured later is picked up automatically.
    void ensure_account_loaded();
    void switch_symbol(const QString& symbol);
    // Connected broker ids (focused first) for broker instrument-master lookups.
    QStringList search_broker_ids() const;
    void update_account_menu();

    // ── Built-in paper account (no broker; see trading/BrokerAccount.h) ──────
    bool focused_is_builtin() const;
    // Screen symbols are canonical (Yahoo-form) instruments — the built-in paper
    // account; with a broker account focused they are that broker's symbols.
    bool canonical_mode() const { return focused_is_builtin(); }
    // Create (first use) / look up the built-in portfolio and make sure the delayed-
    // price service tracks it. Returns its portfolio id (empty on a storage failure).
    QString ensure_builtin_account();
    void focus_builtin_account();          // on_account_changed() for the built-in account
    QString base_currency() const;          // the user's region currency
    static double default_paper_balance(const QString& currency);
    void open_paper_settings();             // reset / set balance of the built-in account
    // Paper account whose fills come from delayed public prices: the built-in one, or
    // a local-paper broker account while its broker feed is down.
    bool simulated_on_delayed_prices() const;
    // Place a paper order on delayed prices (pt_submit_delayed_order) with the
    // simulated-order confirmation; reports the outcome on the ticket.
    void place_simulated_order(const trading::UnifiedOrder& order, bool confirm = true);
    // Close (part of) a paper position on delayed prices: fills now while the venue
    // is open, else queues a reduce-only MARKET order for the open. Returns false
    // with `message` set when refused.
    bool simulated_close(const QString& symbol, const QString& product, double quantity, QString* message);
    // The newest delayed quote of a screen symbol as the simulator sees it.
    trading::PtDelayedQuote delayed_quote_for(const QString& symbol) const;
    // Track / untrack the focused local-paper broker account on delayed prices while
    // its broker feed is down (PaperMarkService).
    void sync_public_paper_tracking();

    // ── Instruments (canonical symbols) ───────────────────────────────────────
    fincept::core::instrument::Instrument instrument_of(const QString& symbol) const;
    QString display_of(const QString& symbol) const; // "RELIANCE · NSE"
    // Public-data target (Yahoo ticker) of a screen symbol, or why it has none.
    equity::EquityPublicFeed::Target public_target(const QString& symbol) const;
    // Watchlist row labels / currencies / broker reachability + bottom-panel meta.
    void refresh_row_info();
    // Why the focused broker cannot trade `symbol` (empty = it can / no broker focused).
    QString broker_block_reason(const QString& symbol) const;
    void apply_selected_instrument_ui(); // ticket / ribbon / labels for the selected symbol
    void set_instrument_line(const QString& full, const QString& compact); // command-bar instrument line
    void apply_quote_detail(const QString& symbol); // snapshot / ticket price source / status

    // ── Layout / chrome ───────────────────────────────────────────────────────
    void update_status_bar();
    void update_session_lines(); // ticket + snapshot + command-bar session state for the selected symbol
    void update_right_column();  // depth ladder vs market snapshot
    void update_first_run_note();
    // Workspace layout (splitter sizes per width class, rail, compact tab) <-> screen state.
    void restore_splitters(const QVariantMap& s);
    QVariantMap splitter_state() const;
    // Watchlist row actions
    void on_watchlist_research(const QString& symbol);
    void on_watchlist_news(const QString& symbol);
    void on_watchlist_alert(const QString& symbol);
    void update_connection_status();
    void async_modify_order(const QString& order_id, double qty, double price);

    // Chart right-click trading: pop a confirm ticket (qty + limit price) at the
    // clicked price, then route through on_order_submitted(). Watchlist-add reuses
    // the existing on_watchlist_symbol_added() slot for the charted symbol.
    void on_chart_buy_requested(double price);
    void on_chart_sell_requested(double price);
    void on_chart_add_to_watchlist();
    void open_chart_order_ticket(bool is_buy, double price);
    // Generalized order ticket for an arbitrary symbol/product (right-click trade
    // from the positions/holdings tables). Routes through on_order_submitted().
    // `order_type` ("market" / "limit" / "" = Market) and `limit_price` (> 0) pre-fill
    // the ticket (external prefill); every value is checked as if typed, inline.
    void open_order_ticket_for(const QString& symbol, const QString& exchange, const QString& product, bool is_buy,
                               double qty = 0.0, const QString& order_type = QString(), double limit_price = 0.0);
    // Reference / paper-fill price for `symbol`: current_price_ for the selected symbol,
    // else the focused broker stream's last quote for it. 0 when unknown (never another
    // symbol's price, never a delayed public quote).
    double ref_price_for(const QString& symbol) const;
    // After an order path that may have touched several accounts (ALL broadcast, BASKETS):
    // pull fresh blotters for those accounts and repaint the focused one.
    void refresh_after_bulk_orders(const QStringList& account_ids);
    // Pick a usable account (paper, or live + Connected) whose broker can trade one
    // of `match_exchanges` (broker.exchanges ∩ match). Prefers the already-focused
    // account, then a live match, then a paper match. Empty when nothing matches.
    QString pick_account_for_exchanges(const QStringList& match) const;
    // Re-reads the focused account's paper portfolio into the panels. No-op for
    // live accounts and native-paper accounts (their data flows from
    // AccountDataStream via the hub).
    void refresh_paper_panels();
    // Re-derives focused_native_paper_ and points the blotter at the matching data source
    // (broker rows for LIVE / native paper, paper-engine rows for the local simulator).
    // Call after anything that can change the answer: focus, mode toggle, credentials.
    void sync_native_paper();
    void flush_paper_prices(); // persist buffered paper position prices to SQLite

    // DataHub subscription helpers (D4 migration)
    void hub_subscribe_streaming();
    void hub_unsubscribe_all();
    void hub_subscribe_quotes();
    QString broker_id_for_focused() const;

    // ── Market-data source: focused broker stream, or the public fallback ──
    // Every price on this screen comes from ONE source at a time. With a usable
    // broker stream (EquityPublicFeed::assess_broker_feed) it is the broker; with
    // none — no account, no credentials, expired token, broker refusing market
    // data, dropped session — it is the public (delayed Yahoo) feed, so the screen
    // is never blank. sync_market_feed() is the single switch point: re-run on
    // focus / connection / credential / visibility changes; idempotent.
    // `fetch_on_switch` = re-request broker chart/depth when switching TO the
    // broker (callers that fetch themselves right after pass false).
    void sync_market_feed(bool fetch_on_switch = true);
    void schedule_market_feed_sync(); // coalesced, queued sync (AccountManager bursts)
    void wire_public_feed();          // EquityPublicFeed sinks -> the shared entry points
    // The source changed: drop the previous source's prices/candles/depth so they
    // are never shown under the new one.
    void reset_market_views();
    void refetch_broker_market_data(); // chart + depth for the selected symbol from the stream
    void push_public_targets();        // watch set + chart target -> public feed
    // Chart history for the selected symbol + timeframe from the active source,
    // with the loading / no-data / failure state shown on the chart.
    void request_chart_history();
    // Shared quote entry point for both sources (ticker bar, watchlist, chart,
    // positions, paper engine). Broker-only effects are skipped for public quotes.
    void apply_market_quote(const QString& symbol, trading::BrokerQuote quote, bool from_broker);
    // Connection label tooltip / order-ticket banner / depth + blotter empty
    // states for the current source and account.
    void update_feed_notices();
    // ── Broker with no live-quote path (e.g. Tradejini in this build) ──
    // A broker whose adapter advertises no quote stream (IBroker::ws_adapter_name()
    // empty) has its quote call as the only price source. When that call refuses —
    // the refusal reaches the screen with the depth request, which falls back to
    // get_quotes and forwards the broker's own text — and no broker price has
    // arrived, the watchlist and price ribbon say "no live quotes" instead of sitting
    // on blank "--" rows. Any broker price, an account switch or a source switch
    // clears it.
    void note_broker_quote_refusal(const QString& reason);
    void apply_broker_quote_notice(); // paint broker_quote_refusal_ onto watchlist + ribbon
    QString chart_key() const;        // "<source>|<symbol>|<timeframe>" of the wanted series
    // True when a broker-stream chart payload for (account, symbol, timeframe) is
    // what the chart wants right now: focused account, broker source, selected
    // symbol, active timeframe.
    bool is_broker_chart_target(const QString& account_id, const QString& symbol, const QString& timeframe) const;
    // True when a broker depth / tape payload for (account, symbol) belongs to the
    // selected symbol on the focused account while the broker is the source.
    bool is_broker_symbol_target(const QString& account_id, const QString& symbol) const;
    QString chart_source_label() const;
    QString exchange_for_symbol(const QString& symbol) const;
    QString public_feed_reason() const; // why the screen is on public data (sentence)

    // Mark held F&O option positions to market. Fyers' live HSM tick feed spells
    // an option ("NIFTY09JUN26C23200") differently from its REST position symbol
    // ("NIFTY2660923200CE"), so option ticks never match the per-symbol quote
    // topics. This router reconciles the two by (underlying, strike, side) and
    // patches the position/holding row + paper P&L. Bound to the focused stream
    // (opt_quote_conn_) only while an option position is actually held.
    void route_option_quote(const QString& account_id, const QString& symbol, const trading::BrokerQuote& quote);

    // Named-watchlist controller (WatchlistRepository). load_watchlists() refreshes
    // the combo + seeds a default from the broker's default_watchlist on first run;
    // apply_active_watchlist() pushes the active list's symbols into the view and
    // (when resubscribe=true) re-points the live stream + hub at the new symbol set.
    void load_watchlists();
    void apply_active_watchlist(bool resubscribe);
    // Effective live-subscription set = active watchlist ∪ open-position symbols
    // ∪ holding symbols, so symbols you hold (even if not in the list) get live
    // WebSocket prices instead of only the 5-minute REST portfolio snapshot.
    QStringList effective_symbols() const;
    void update_position_symbols(const QStringList& syms); // re-subscribe when it changes
    void update_holding_symbols(const QStringList& syms);  // re-subscribe when it changes
    // Push the open position for the displayed symbol onto the chart's overlay
    // card + entry line (from the live cache or the paper engine), or clear it
    // when the symbol is flat.
    void update_chart_position();

    // Ensure the broker instrument master for `account_id` is loaded into
    // InstrumentService (from SQLite cache or a fresh download). Market data
    // (quotes/charts/depth) needs the numeric securityId map, which only exists
    // once instruments are loaded. Mirrors the ChainSubTab load pattern.
    void ensure_instruments_loaded(const QString& account_id);

    // ── Command bar (EquityCommandBar) ──
    equity::EquityCommandBar* cmd_bar_ = nullptr;
    equity::EquityBarButton* account_btn_ = nullptr; // shows focused account name
    QMenu* account_menu_ = nullptr;                  // lists all active accounts
    // Symbol search (shared picker through the equity::make_symbol_box factory).
    equity::SymbolBox* symbol_box_ = nullptr;
    equity::EquityBarButton* mode_btn_ = nullptr; // PAPER / LIVE chip (per-account toggle)
    QPushButton* feeds_btn_ = nullptr;            // FEEDS toggle (feed monitor blotter tab)
    QPushButton* unified_btn_ = nullptr;          // ALL ACCOUNTS toggle (portfolio monitor blotter tab)
    QPushButton* accounts_btn_ = nullptr;         // opens AccountManagementDialog
    QPushButton* watchlist_btn_ = nullptr;        // WATCHLIST toggle (fold to the rail)
    // Instrument line (venue · name · currency) and aggregate connection status:
    // labels that never widen the command bar (full text, else compact, else elided).
    equity::EquityFitLabel* exchange_label_ = nullptr;
    equity::EquityFitLabel* conn_label_ = nullptr;
    QString conn_tone_; // tone property last applied to conn_label_
    // Connection label text + tone ("", "info", "ok", "warn", "danger"); re-polished
    // only when the tone changes.
    void set_conn_status(const QString& full, const QString& compact, const QString& tone);

    // ── Sub-widgets ──
    equity::EquityTickerBar* ticker_bar_ = nullptr;
    equity::EquityWatchlist* watchlist_ = nullptr;
    equity::EquityChartPanel* chart_ = nullptr;
    equity::EquityOrderEntry* order_entry_ = nullptr;
    equity::EquityOrderBook* orderbook_ = nullptr;
    equity::EquityBottomPanel* bottom_panel_ = nullptr;
    equity::EquityQuoteSnapshot* snapshot_ = nullptr; // right column when there is no depth
    QWidget* first_run_note_ = nullptr;               // dismissible "paper account ready" note
    QLabel* first_run_text_ = nullptr;
    equity::EquityWorkspace* workspace_ = nullptr;    // the responsive three-zone body
    // Data source + delay, session, account + currency, funds / buying power, clock.
    fincept::ui::ScreenStatusBar* status_bar_ = nullptr;
    QTimer* status_timer_ = nullptr;
    fincept::feeds::FeedPanel* feed_panel_ = nullptr; // FEEDS blotter tab (built on first use)
    PortfolioMonitorScreen* monitor_panel_ = nullptr; // ALL ACCOUNTS blotter tab (built on first use)
    QWidget* monitor_float_ = nullptr;                // top-level window while the monitor is detached

    void float_monitor();  // reparent monitor_panel_ into its own window
    void redock_monitor(); // bring it back as its blotter tab

  protected:
    // Watches monitor_float_ for Close to re-dock the monitor (feeds pattern,
    // without a dedicated moc'd window class).
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    // ── Timers (only UI-local timers remain; data timers are in AccountDataStream) ──
    QTimer* market_clock_timer_ = nullptr;

    // ── Single instance per panel (EquitySingleInstance.h) ──
    bool inert_ = false;
    QPointer<EquityTradingScreen> primary_; // the panel's real screen (set on an inert twin only)

    // ── Multi-account state ──
    QString focused_account_id_; // the account targeted by order entry + chart
    QString selected_symbol_ = "RELIANCE";
    // chart_key() ("<source>|<symbol>|<timeframe>") of the history the chart
    // currently holds; empty when it holds none. Lets an empty broker response
    // keep a good chart instead of wiping it down to live-only bars, while still
    // clearing content the user has moved on from (#338). The source tag means a
    // public series is never kept under the broker source, or vice versa.
    QString chart_symbol_;
    QString selected_exchange_ = "NSE";

    // Paper trading (derived from focused account)
    int fill_cb_id_ = -1; // OrderMatcher fill callback
    // Cached focused-account trading context. Refreshed on every focus / symbol /
    // mode change (hub_subscribe_streaming) and on every paper refresh, so the
    // per-tick quote handler avoids a mutex-locked BrokerAccount copy per tick.
    bool focused_is_paper_ = false; // LOCAL paper simulator only — false whenever focused_native_paper_ is true
    // True when the focused account's PAPER mode runs on the broker's own paper venue
    // (UnifiedTrading::uses_native_paper): orders, positions, funds and the blotter then
    // come from the broker exactly as in LIVE; only the PAPER tag stays. Cached because the
    // check reads secure storage; refreshed by sync_native_paper().
    bool focused_native_paper_ = false;
    // True when the focused broker is a US-market broker (region == "US"). Gates the
    // US-only data fetches (time&sales / calendar / clock) and bottom-panel tabs.
    bool focused_is_us_market_ = false;
    QString focused_paper_portfolio_id_;
    // Paper engine price persistence is coalesced off the per-tick GUI hot path:
    // the latest LTP per symbol is buffered here and flushed to SQLite on a timer
    // (and synchronously before refresh_paper_panels reads), instead of one DB
    // UPDATE per tick. SL/TP + limit matching still run every tick (in-memory).
    QHash<QString, double> pending_paper_prices_;
    bool paper_flush_armed_ = false;
    // Focused stream → route_option_quote, bound only while an F&O option position
    // is held (Fyers' HSM tick symbol can't match the per-symbol quote topics).
    QMetaObject::Connection opt_quote_conn_;

    // Async guards
    std::atomic<bool> token_expired_shown_{false};
    // Accounts whose connect was already handled via the dialog's credentials_saved, so
    // the queued AccountManager::credentials_changed follow-up does not run it twice.
    QSet<QString> creds_saved_handled_;

    QStringList watchlist_symbols_;
    QString active_watchlist_id_; // current WatchlistRepository list id
    // Paper order book day shown in the Orders tab (default today; date selector
    // moves it to view a past IST session). Open positions/holdings stay current.
    QDate orders_view_day_ = QDate::currentDate();
    QStringList position_symbols_;                    // symbols with open positions (transient, for live pricing)
    QStringList holding_symbols_;                     // symbols in holdings (transient, for live pricing)
    QVector<trading::BrokerPosition> live_positions_; // cached live positions for focused account
    double current_price_ = 0.0;                      // last known LTP for selected symbol

    bool initialized_ = false;
    bool hub_active_ = false;
    // True once pause_streams() ran for a hide; the next show resumes everything.
    // A hide + show within one event-loop turn (Qt re-creating the native window when
    // the chart's first WebEngine view appears) never pauses, so nothing is rebuilt.
    bool streams_paused_ = false;

    // ── Market-data source state (see sync_market_feed) ──
    equity::EquityPublicFeed* public_feed_ = nullptr;
    bool public_mode_ = false; // true: market data on screen is the public fallback
    equity::EquityPublicFeed::BrokerAssessment feed_state_;
    // A token session that just dropped usually reconnects within seconds: keep the
    // broker as the source for this grace period before falling back.
    QTimer* feed_grace_timer_ = nullptr;
    bool feed_grace_expired_ = false;
    bool feed_sync_queued_ = false;
    bool accounts_btn_accented_ = false;
    // symbol -> exchange stored with it in the active named watchlist (public
    // feed symbol mapping); symbols not listed use selected_exchange_.
    QHash<QString, QString> watchlist_exchanges_;
    // Broker chart request in flight (key) + token for its last-resort no-response
    // watchdog. Data and failures arrive as tagged stream signals; the watchdog
    // only covers a broker call that never answers at all.
    QString chart_pending_key_;
    quint64 chart_request_seq_ = 0;
    // Symbol whose broker depth the order book currently shows (empty = none). A
    // depth failure clears the panel only when it is not already showing this
    // symbol's book (a live WebSocket push may have filled it meanwhile).
    QString orderbook_symbol_;
    // See note_broker_quote_refusal(): the focused broker's own reason for giving no
    // quote while it is shown (empty otherwise), and whether a broker price has
    // arrived since the account / source was last set (then the notice never applies).
    QString broker_quote_refusal_;
    bool broker_quote_seen_ = false;

    // ── Instruments / delayed quotes ──
    // Instrument behind each screen symbol (canonical record; for broker symbols the
    // canonical listing they map to). Rows the public data path cannot quote keep a
    // record with an empty `symbol`.
    QHash<QString, fincept::core::instrument::Instrument> instruments_;
    QHash<QString, QString> stored_symbol_of_;          // screen symbol -> symbol as stored in the watchlist
    QHash<QString, services::QuoteData> quote_detail_;  // screen symbol -> newest public quote
    QHash<QString, qint64> quote_rx_ms_;                // screen symbol -> when it was received
    QString account_currency_;                          // focused paper portfolio's currency
    QString builtin_selected_symbol_;                   // last symbol viewed on the built-in account
    QString public_tracked_portfolio_;                  // broker paper portfolio tracked on delayed prices
    bool skip_paper_confirm_ = false;                   // "don't ask again for paper orders"
    qint64 last_paper_refresh_ms_ = 0;                  // throttles prices_marked repaints
    QVariantMap pending_splitter_state_;                // restored after the first real layout
    QHash<QString, QString> row_block_reason_;          // broker mode: why the broker cannot trade a row
    equity::EquityFundsView last_funds_view_;           // newest funds figures (status bar)
    bool have_funds_view_ = false;
    bool row_info_queued_ = false;
    // Watchlist / position symbols the broker stream may subscribe (rows the focused
    // broker cannot trade are left out).
    QStringList stream_symbols() const;
    void schedule_row_info();
    // Orders tab rows for a paper portfolio and the picked day (built-in account: the
    // user's local day + every resting order; broker paper: the IST session book).
    QVector<trading::PtOrder> paper_orders_for_day(const QString& portfolio_id, const QDate& day) const;
    // Saved rows through InstrumentResolver::resolve_stored (local index; follows
    // renames, never migrates): fills names / currencies and reports "X is now Y" in
    // the watchlist notice — the user decides, nothing is rewritten.
    void refine_stored_rows(const QVector<QPair<QString, QPair<QString, QString>>>& rows);
    quint64 stored_resolve_gen_ = 0;
    QStringList rename_notes_;

    // Symbol group link — SymbolGroup::None when unlinked.
    SymbolGroup link_group_ = SymbolGroup::None;
};

} // namespace fincept::screens
