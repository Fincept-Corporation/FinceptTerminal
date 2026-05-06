#pragma once
// Crypto Trading Screen — coordinator

#include "core/symbol/IGroupLinked.h"
#include "core/symbol/SymbolGroup.h"
#include "screens/IStatefulScreen.h"
#include "screens/crypto_trading/CryptoTypes.h"
#include "trading/TradingTypes.h"

#include <QHideEvent>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QShowEvent>
#include <QStringList>
#include <QTimer>
#include <QWidget>

#include <atomic>

namespace fincept::screens::crypto {
class CryptoTickerBar;
class CryptoWatchlist;
class CryptoChart;
class CryptoOrderEntry;
class CryptoOrderBook;
class CryptoBottomPanel;
} // namespace fincept::screens::crypto

namespace fincept::screens {

class CryptoTradingScreen : public QWidget, public IStatefulScreen, public IGroupLinked {
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

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_exchange_changed(const QString& exchange);
    void on_symbol_selected(const QString& symbol);
    void on_mode_toggled();
    void on_api_clicked();
    void on_order_submitted(const QString& side, const QString& order_type, double qty, double price, double stop_price,
                            double sl, double tp);
    void on_cancel_order(const QString& order_id);
    void on_ob_price_clicked(double price);
    void on_search_requested(const QString& filter);

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
    void init_exchange();
    void load_portfolio();
    void switch_symbol(const QString& symbol);

    void async_fetch_candles(const QString& symbol, const QString& timeframe);
    void async_fetch_live_positions();
    void async_fetch_live_orders();
    void async_fetch_live_balance();
    void async_fetch_my_trades();
    void async_fetch_trading_fees();
    void async_fetch_mark_price();
    void async_set_leverage(int leverage);
    void async_set_margin_mode(const QString& mode);

    // ── DataHub subscription lifecycle (the only data path since Phase 6) ─
    // Only exchanges registered as DataHub producers via
    // ExchangeSessionManager::topic_patterns() can appear in the dropdown.
    void hub_subscribe_topics();
    void hub_unsubscribe_topics();

    // ── Command bar widgets ──
    QPushButton* exchange_btn_ = nullptr;
    QMenu* exchange_menu_ = nullptr;
    QLineEdit* symbol_input_ = nullptr;
    QPushButton* mode_btn_ = nullptr;
    QPushButton* api_btn_ = nullptr;
    QLabel* ws_status_ = nullptr;
    QLabel* ws_transport_ = nullptr;  // tiny hint: "NATIVE" for Kraken, "DAEMON" for ccxt
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


    // Async fetch guards
    std::atomic<bool> candles_fetching_{false};
    std::atomic<int> live_inflight_{0};  // counts async_fetch_live_* tasks still running
    std::atomic<bool> paper_bookkeeping_in_flight_{false};

    // Startup gate — ensures daemon-dependent fetches fire exactly once,
    // either via daemon_ready signal or via the 8s safety fallback timeout.
    bool startup_fetches_done_ = false;

    // WS/REST mode edge detection — when WS transitions online we stop REST
    // polling; when it drops we restart it. tri-state: -1 = unknown.
    int last_ws_state_ = -1;
    int last_ws_status_label_state_ = -1;  // separate from last_ws_state_ since update_clock is 1Hz
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
