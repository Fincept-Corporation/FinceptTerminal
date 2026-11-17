#pragma once
// Crypto Trading Screen — coordinator

#include "core/symbol/IGroupLinked.h"
#include "core/symbol/SymbolGroup.h"
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "screens/crypto_trading/CryptoTypes.h"
#include "trading/TradingTypes.h"

#include <QEvent>
#include <QHideEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QShowEvent>
#include <QStringList>
#include <QTimer>
#include <QWidget>

#include <atomic>

namespace fincept::trading {
class ExchangeSession;
}

namespace fincept::ui {
class AutoLoadPane;
}

namespace fincept::screens::crypto {
class CryptoTickerBar;
class CryptoWatchlist;
class CryptoChart;
class CryptoOrderEntry;
class CryptoOrderBook;
class CryptoBottomPanel;
} // namespace fincept::screens::crypto

namespace fincept::screens {

class CryptoTradingScreen : public QWidget, public IStatefulScreen, public IGroupLinked, public ISectionLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit CryptoTradingScreen(QWidget* parent = nullptr);
    ~CryptoTradingScreen();

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "crypto_trading"; }
    int state_version() const override { return 1; }

    // IGroupLinked — Phase 7: link crypto pairs across groups so a "BTC/USDT"
    // selection in one panel propagates to a chart panel in the same group.
    void set_group(SymbolGroup g) override { link_group_ = g; }
    SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const SymbolRef& ref) override;
    SymbolRef current_symbol() const override;

    // ISectionLinked — deep link from the header search box: a bottom tab
    // (positions | orders | history | my_trades | fees | market | stats | tape | depth)
    // or "ticket" (focus "buy" / "sell" picks the side; the quantity box gets the
    // keyboard). focus with section "" = a pair ("ETH/USDT"), as typed in the symbol box.
    // Navigation only: nothing is placed, cancelled or changed.
    bool open_section(const QString& section, const QString& focus) override;

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private slots:
    void on_exchange_changed(const QString& exchange);
    void on_symbol_selected(const QString& symbol);
    void on_mode_toggled();
    void on_api_clicked();
    void on_order_submitted(const QString& side, const QString& order_type, double qty, double price, double stop_price,
                            double sl, double tp);
    void on_cancel_order(const QString& order_id, const QString& order_symbol);
    void on_cancel_all_orders();                   // CANCEL ALL (live + paper)
    void on_close_all_positions();                 // SQUARE OFF ALL (live + paper)
    void on_close_position(const QString& symbol); // close a single position
    void on_ob_price_clicked(double price);
    void on_search_requested(const QString& filter);
    /// EventBus "crypto.open_order_ticket" (the header search box's "buy 0.5 btc"): in
    /// the active window only, switch to the pair and prefill the ticket (never submitted),
    /// then acknowledge synchronously with "crypto.open_order_ticket.ack" {token}.
    void on_external_order_ticket(const QVariantMap& request);

    void refresh_ticker();
    void refresh_orderbook();
    void refresh_portfolio();
    void refresh_watchlist();
    void refresh_market_info();
    void refresh_candles();
    void refresh_live_data();
    void update_clock();

  private:
    void setup_ui();
    void setup_timers();
    void retranslateUi();
    void init_exchange();
    void load_portfolio();
    void switch_symbol(const QString& symbol);
    /// Tells the ticker, book, chart, tape and depth chart which market the
    /// prices come from — the resolved market (a pair may be shown from one
    /// quoted in another asset), else the selected pair — so each prints the
    /// right quote asset. Call whenever selected_symbol_ or market_ changes.
    void apply_price_market();

    // Perp detection + futures-control visibility. Leverage / margin-mode /
    // reduce-only controls only make sense on derivatives, so they are shown
    // only when the active market is a perp (Hyperliquid, or a settled pair).
    bool is_perp_market() const;
    void update_futures_visibility();

    // `attempt` drives the retry backoff for an empty OHLCV response (#338) —
    // callers always start at 0.
    void async_fetch_candles(const QString& symbol, const QString& timeframe, int attempt = 0);
    void async_fetch_live_positions();
    void async_fetch_live_orders();
    void async_fetch_live_balance();
    void async_fetch_my_trades();
    void async_fetch_trading_fees();
    void async_fetch_mark_price();
    // Leverage / margin mode: the ticket's APPLY, confirmed with the scope
    // (one market, or the whole account on e.g. Bybit), then sent; the
    // account's current values are read back.
    void on_apply_leverage(int leverage, const QString& margin_mode);
    void async_fetch_leverage_info();

    // ── Exchange routing ──────────────────────────────────────────────────
    // The session of the exchange THIS screen shows. Every call names it (or,
    // on a worker, the exchange id captured on the UI thread); nothing reads
    // ExchangeService's process-wide "current exchange", which another window
    // or a later switch can change under a pending order.
    trading::ExchangeSession* session() const;

    /// Where a LIVE action goes and as whom, captured when the user acts.
    struct LiveContext {
        QString exchange;
        QString account_id;    // credential fingerprint the daemon re-checks
        QString account_label; // "API key ab12…ef90"
        trading::ExchangeCredentials creds;
        QString selected_symbol;
    };
    LiveContext live_context() const;
    /// False (with why) when the screen moved on since `ctx` was captured:
    /// another exchange, PAPER, another pair, other API keys.
    bool live_context_current(const LiveContext& ctx, QString* why) const;

    // ── LIVE orders: prepare (nothing sent) -> confirm exactly that -> place ──
    void submit_live_order(const LiveContext& ctx, const QJsonObject& args);
    void confirm_and_place(const LiveContext& ctx, const QJsonObject& args, const QJsonObject& prepared);
    void report_order_outcome(const LiveContext& ctx, const QJsonObject& args, const QJsonObject& prepared,
                              const QJsonObject& reply);
    void look_up_unconfirmed_order(const LiveContext& ctx, const QString& symbol, const QString& token, int attempt);
    QString order_confirm_text(const LiveContext& ctx, const QJsonObject& prepared) const;
    // Close / square off: fetch the positions fresh, prepare each reduce-only
    // close, list them in one confirmation, then place exactly those.
    void close_live_positions(const QString& only_symbol);
    void place_prepared_batch(const LiveContext& ctx, const QVector<QJsonObject>& args,
                              const QVector<QJsonObject>& prepared, const QString& title);
    void warm_live_session(); // build the authenticated client + markets when LIVE is armed
    void on_market_choice(const QString& symbol);
    void connect_session_signals(); // ws_failed / ws_state_changed of this screen's session
    // Symbol / exchange switch: seed the time & sales tape with the latest REST
    // prints, and load the market's order minimums into the ticket.
    void async_seed_trade_tape();
    void async_fetch_market_limits();
    // A place / cancel ended without a confirmation (time-out or a dropped
    // connection): the order may still exist. Refresh the live views now and
    // again shortly after, so it shows up as soon as the exchange has it. Never
    // re-sends anything.
    void reconcile_after_unconfirmed();

    // ── DataHub subscription lifecycle (the only data path since Phase 6) ─
    // Only exchanges registered as DataHub producers via
    // ExchangeSessionManager::topic_patterns() can appear in the dropdown.
    void hub_subscribe_topics();
    void hub_unsubscribe_topics();

    // ── Never-blank market panes (chart plot + order book) ──
    // A visible pane asks for data: fetch the chart history when none is drawn;
    // on a `retry` (RETRY, Python became ready) also restart a stream that gave up.
    // Public market data only — nothing is placed, changed or connected.
    void ensure_market_feed(bool retry);
    /// Symbol / exchange / timeframe switch: chart and book back to their
    /// skeletons, captioned with what is loading.
    void reset_market_panes();
    /// The live stream is down (`message` = why) or back (""): chart + book states.
    void report_live_feed(const QString& message);
    /// The exchange's API-key state (none / stored but unreadable / present) to the bottom panel.
    void push_key_state();
    /// The fee schedule of the pair on screen, when FEES / MARKET wants it and it
    /// is not loaded for this exchange, pair and mode (`force` = RETRY / entering LIVE).
    void maybe_fetch_fees(bool force);

    // ── Command bar widgets ──
    QPushButton* exchange_btn_ = nullptr;
    QMenu* exchange_menu_ = nullptr;
    QLineEdit* symbol_input_ = nullptr;
    QPushButton* mode_btn_ = nullptr;
    QPushButton* api_btn_ = nullptr;
    QLabel* ws_status_ = nullptr;
    QLabel* ws_error_label_ = nullptr; // why live prices are down (ws_failed), next to the pill
    QObject* session_conn_owner_ = nullptr;
    QLabel* ws_transport_ = nullptr; // tiny hint: "NATIVE" for Kraken, "DAEMON" for ccxt
    QLabel* clock_label_ = nullptr;

    /// Context object that owns all direct connections to the native Kraken
    /// WS client. Destroyed (and recreated) on symbol/exchange swap so every
    /// connection is auto-disconnected in one move.
    QObject* ws_subscription_owner_ = nullptr;

    // ── Sub-widgets ──
    crypto::CryptoTickerBar* ticker_bar_ = nullptr;
    crypto::CryptoWatchlist* watchlist_ = nullptr;
    crypto::CryptoChart* chart_ = nullptr;
    crypto::CryptoOrderEntry* order_entry_ = nullptr;
    crypto::CryptoOrderBook* orderbook_ = nullptr;
    crypto::CryptoBottomPanel* bottom_panel_ = nullptr;
    // The order book inside a feed-mode AutoLoadPane: skeleton until the first
    // snapshot, error page (RETRY) when the stream is down, Python setup page.
    ui::AutoLoadPane* ob_pane_ = nullptr;
    bool ob_feed_reported_ = false; // the pane was told a book is on screen (since the last reset / error)
    bool ob_feed_retry_ = false;    // the pane let go of the feed while visible (RETRY / requirement lost)
    QString fees_key_;              // "<exchange>|<pair>|<mode>" whose fee schedule was requested
    QString market_info_key_;       // "<exchange>|<pair>" whose funding / OI was last requested

    // ── Timers ──
    QTimer* ticker_timer_ = nullptr;
    QTimer* ob_timer_ = nullptr;
    QTimer* portfolio_timer_ = nullptr;
    QTimer* watchlist_timer_ = nullptr;
    QTimer* market_info_timer_ = nullptr;
    QTimer* live_data_timer_ = nullptr;
    QTimer* clock_timer_ = nullptr;

    // ── State ──
    QString exchange_id_ = "kraken";
    QString selected_symbol_ = "BTC/USDT";
    crypto::TradingMode trading_mode_ = crypto::TradingMode::Paper;

    // Paper trading
    QString portfolio_id_;
    trading::PtPortfolio portfolio_;

    // "<symbol>|<timeframe>" the chart currently holds history for. Lets an
    // empty OHLCV response keep a good chart instead of wiping it, while still
    // clearing content that belongs to a symbol the user has left (#338).
    QString chart_symbol_;
    static constexpr int CANDLE_FETCH_MAX_ATTEMPTS = 3;
    static constexpr int CANDLE_FETCH_RETRY_MS = 2000;

    // The market selected_symbol_ resolves to on exchange_id_ (the daemon's one
    // resolver) and what may be sent there. UI thread only.
    crypto::ResolvedMarket market_;
    // A LIVE cancel-all / close / square-off is running: a second press must
    // not send a second set of orders (UI thread only).
    bool account_action_busy_ = false;
    // Idempotency for a repeated ticket after an unknown outcome: the same
    // ticket pressed again within 10 minutes reuses the client order id, so an
    // exchange that enforces unique ids rejects a duplicate.
    QString last_unknown_token_;
    QString last_unknown_key_;
    qint64 last_unknown_at_ = 0;

    // Async fetch guards
    std::atomic<bool> candles_fetching_{false};
    std::atomic<int> live_inflight_{0}; // counts async_fetch_live_* tasks still running
    std::atomic<bool> paper_bookkeeping_in_flight_{false};

    // Startup gate — ensures daemon-dependent fetches fire exactly once,
    // either via daemon_ready signal or via the 8s safety fallback timeout.
    bool startup_fetches_done_ = false;

    // WS/REST mode edge detection — when WS transitions online we stop REST
    // polling; when it drops we restart it. tri-state: -1 = unknown.
    int last_ws_state_ = -1;
    int last_ws_status_label_state_ = -1; // separate from last_ws_state_ since update_clock is 1Hz
    void apply_feed_mode(bool ws_connected);

    QStringList watchlist_symbols_ = {
        "BTC/USDT",  "ETH/USDT",  "SOL/USDT",  "BNB/USDT",  "XRP/USDT",   "DOGE/USDT", "ADA/USDT",
        "AVAX/USDT", "TON/USDT",  "LINK/USDT", "DOT/USDT",  "MATIC/USDT", "UNI/USDT",  "ATOM/USDT",
        "LTC/USDT",  "BCH/USDT",  "APT/USDT",  "ARB/USDT",  "OP/USDT",    "SUI/USDT",  "TRX/USDT",
        "INJ/USDT",  "NEAR/USDT", "WIF/USDT",  "PEPE/USDT",
    };

    bool initialized_ = false;

    // Phase 7: symbol group link — SymbolGroup::None when unlinked.
    // Crypto Trading publishes selected_symbol_ (a "BASE/QUOTE" pair) into
    // the linked group with asset_class="crypto"; consumes inbound symbols
    // tagged crypto by routing them through switch_symbol().
    SymbolGroup link_group_ = SymbolGroup::None;

    // Cached market info — funding rate and open interest arrive on separate
    // workers so we merge into this cache and emit the union to the bottom
    // panel from each worker's return path (UI-thread — no lock needed).
    crypto::MarketInfoData market_info_cache_;

    // ── WS update coalescing (10fps UI flush) ──
    QTimer* ws_flush_timer_ = nullptr;
    QHash<QString, trading::TickerData> pending_tickers_; // accumulated since last flush
    trading::TickerData pending_primary_ticker_;          // latest for selected symbol
    bool has_pending_primary_ = false;

    // Orderbook coalescing — latest-wins. Raw bursts (~20 msgs/sec on busy
    // symbols) collapse to one UI refresh per flush tick (10/sec).
    trading::OrderBookData pending_orderbook_;
    bool has_pending_orderbook_ = false;

    // Hard caps on the two unbounded append-only WS buffers. The drain
    // (flush_ws_updates, 10fps) normally empties them every tick, but any
    // stall — hidden screen, blocked event loop — used to let them grow at
    // exchange tick rate forever. Oldest entries are dropped on overflow.
    static constexpr int kMaxPendingTrades = 2000;
    static constexpr int kMaxPendingCandles = 2000;

    // Candle coalescing — append closed candles in order, keep the latest
    // in-progress candle separately so we render partial bars without
    // flooding the chart on every tick.
    QVector<trading::Candle> pending_candles_;

    // Trade coalescing — batch recent trades so bottom_panel_ does one list
    // append per flush tick instead of one per WS message.
    QVector<crypto::TradeEntry> pending_trades_;

    void flush_ws_updates();
};

} // namespace fincept::screens
