#pragma once
// Crypto Trading Screen — professional trading terminal
// Layout: TopNav + TickerBar + [Watchlist | Chart+Bottom | OrderEntry+OrderBook] + StatusBar
//
// Integrates with ExchangeService (ccxt via Python) and paper trading engine.

#include "crypto_types.h"
#include "trading/exchange_service.h"
#include "portfolio/portfolio_types.h"
#include "storage/database.h"
#include <imgui.h>
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <ctime>

namespace fincept::crypto {

class CryptoTradingScreen {
public:
    void render();
    ~CryptoTradingScreen();

private:
    void init();
    void load_portfolio();

    // Exchange & symbol switching — proper lifecycle management
    void switch_exchange(const std::string& exchange_id);
    void switch_symbol(const std::string& symbol);
    void restart_ws_stream();
    void clear_market_data();

    // Async data fetchers — run in background threads
    void async_refresh_ticker();
    void async_refresh_orderbook();
    void async_refresh_watchlist_prices();
    void refresh_portfolio_data(); // DB only, fast, runs on main thread

    // Thread lifecycle — RAII tracked threads (CP.26: no detach)
    void launch_async(std::thread t);
    void join_pending_threads();

    // Layout panels
    void render_top_nav(float w, float h);
    void render_ticker_bar(float w, float h);
    void render_watchlist(float w, float h);
    void render_chart_area(float w, float h);
    void render_candlestick_chart(ImVec2 origin, float chart_w, float chart_h);
    void render_bottom_panel(float w, float h);
    void render_order_entry(float w, float h);
    void render_orderbook(float w, float h);
    void render_status_bar(float w, float h);

    // Order book mode renderers
    void render_ob_mode_book(float w);
    void render_ob_mode_volume(float w);
    void render_ob_mode_imbalance(float w);
    void render_ob_mode_signals(float w);

    // Bottom sub-tabs
    void render_positions_tab();
    void render_orders_tab();
    void render_history_tab();
    void render_trades_tab();
    void render_market_info_tab();
    void render_stats_tab();

    // Async fetchers for new data
    void async_fetch_market_info();
    void async_fetch_trades();

    // Actions
    void submit_order();
    void cancel_order(const std::string& order_id);

    // Live trading data fetchers
    void async_fetch_live_positions();
    void async_fetch_live_orders();
    void async_fetch_live_balance();
    void async_fetch_live_trades();

    // Credential management
    void render_credentials_popup();
    void load_exchange_credentials();
    void save_exchange_credentials();
    bool has_credentials() const;

    bool initialized_ = false;

    // --- Exchange state ---
    std::string exchange_id_ = "binance";
    std::string selected_symbol_ = "BTC/USDT";
    TradingMode trading_mode_ = TradingMode::Paper;

    // --- Dynamic exchange list (fetched from ccxt at startup) ---
    std::vector<std::string> available_exchanges_;
    std::atomic<bool> exchanges_loaded_{false};
    std::atomic<bool> exchanges_fetching_{false};
    char exchange_filter_[64] = "";

    // --- Watchlist ---
    std::vector<WatchlistEntry> watchlist_;
    int watchlist_selected_ = 0;
    char watchlist_filter_[64] = "";

    // --- Symbol search (exchange-wide) ---
    std::vector<trading::MarketInfo> search_results_;
    std::atomic<bool> search_fetching_{false};
    bool search_markets_loaded_ = false;
    std::vector<trading::MarketInfo> all_markets_; // cached market list for current exchange
    std::string search_cache_exchange_;            // exchange_id when markets were fetched
    std::string pending_symbol_switch_;            // deferred switch (set inside locked section)

    // --- Ticker (selected symbol) ---
    trading::TickerData current_ticker_;
    std::atomic<bool> ticker_loading_{false};
    std::atomic<bool> ticker_fetching_{false}; // true while async fetch in flight

    // --- Order book ---
    std::vector<OBLevel> ob_bids_;
    std::vector<OBLevel> ob_asks_;
    double ob_spread_ = 0.0;
    double ob_spread_pct_ = 0.0;
    std::atomic<bool> ob_loading_{true};
    std::atomic<bool> ob_fetching_{false};

    // --- Order book view mode & tick history ---
    ObViewMode ob_view_mode_ = ObViewMode::Book;
    std::vector<TickSnapshot> tick_history_;  // capped at OB_MAX_TICK_HISTORY
    int64_t last_tick_capture_ms_ = 0;        // epoch ms — throttle to 1 capture/sec

    // --- Watchlist prices ---
    std::atomic<bool> wl_fetching_{false};

    // --- Order entry ---
    OrderFormState order_form_;
    std::atomic<bool> order_submitting_{false};

    // --- Paper trading portfolio ---
    std::string portfolio_id_;
    trading::PtPortfolio portfolio_;
    std::vector<trading::PtPosition> positions_;
    std::vector<trading::PtOrder> orders_;
    std::vector<trading::PtTrade> trades_;
    trading::PtStats stats_;
    bool portfolio_loaded_ = false;

    // --- Bottom panel ---
    BottomTab bottom_tab_ = BottomTab::Positions;

    // --- Candle / OHLCV data ---
    std::vector<trading::Candle> candles_;
    std::string candle_timeframe_ = "1m";
    int selected_timeframe_ = 0; // index into timeframe array
    std::atomic<bool> candles_loading_{false};
    std::atomic<bool> candles_fetching_{false};

    // --- Trade feed (Time & Sales) ---
    std::vector<TradeEntry> recent_trades_;
    static constexpr int MAX_TRADES = 200;
    std::atomic<bool> trades_fetching_{false};

    // --- Market info (funding rate, OI) ---
    MarketInfoData market_info_;
    std::atomic<bool> market_info_fetching_{false};
    float market_info_timer_ = 0;

    // --- WebSocket state ---
    bool ws_started_ = false;
    int ws_price_cb_id_ = -1;
    int ws_ob_cb_id_ = -1;
    int ws_candle_cb_id_ = -1;
    int ws_trade_cb_id_ = -1;

    // --- Live trading data (fetched from exchange API) ---
    struct LivePosition {
        std::string symbol;
        std::string side;
        double quantity = 0;
        double entry_price = 0;
        double current_price = 0;
        double unrealized_pnl = 0;
        double leverage = 1.0;
    };
    struct LiveOrder {
        std::string id;
        std::string symbol;
        std::string side;
        std::string type;
        std::string status;
        double quantity = 0;
        double price = 0;
        double filled_qty = 0;
        std::string timestamp;
    };
    struct LiveTrade {
        std::string id;
        std::string symbol;
        std::string side;
        double price = 0;
        double quantity = 0;
        double fee = 0;
        std::string timestamp;
    };

    std::vector<LivePosition> live_positions_;
    std::vector<LiveOrder> live_orders_;
    std::vector<LiveTrade> live_trades_;
    double live_balance_ = 0.0;
    double live_equity_ = 0.0;
    double live_used_margin_ = 0.0;
    std::atomic<bool> live_positions_fetching_{false};
    std::atomic<bool> live_orders_fetching_{false};
    std::atomic<bool> live_balance_fetching_{false};
    std::atomic<bool> live_trades_fetching_{false};
    bool live_data_loaded_ = false;
    float live_data_timer_ = 0;
    static constexpr float LIVE_DATA_REFRESH_INTERVAL = 5.0f;

    // --- Credential management ---
    bool show_credentials_popup_ = false;
    char cred_api_key_[256] = "";
    char cred_api_secret_[256] = "";
    char cred_password_[128] = "";   // OKX, Kucoin etc.
    bool credentials_loaded_ = false;
    bool has_credentials_ = false;

    // --- Timers ---
    float ticker_timer_ = 0;
    float ob_timer_ = 0;
    float wl_timer_ = 0;
    float portfolio_timer_ = 0;
    time_t last_data_update_ = 0;

    // --- Error tracking ---
    std::string last_error_;
    int error_count_ = 0;
    int success_count_ = 0;

    std::mutex data_mutex_;

    // --- Async thread tracking (CP.26: no detach — join on destroy) ---
    std::vector<std::thread> async_threads_;
    std::mutex threads_mutex_;

    // Default watchlist symbols
    static std::vector<std::string> default_watchlist_symbols();
};

} // namespace fincept::crypto
