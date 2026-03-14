#pragma once
// Crypto Trading Screen — professional trading terminal
// Layout: TopNav + TickerBar + [Watchlist | Chart+Bottom | OrderEntry+OrderBook] + StatusBar
//
// Integrates with ExchangeService (ccxt via Python) and paper trading engine.

#include "crypto_types.h"
#include "trading/exchange_service.h"
#include "portfolio/portfolio_types.h"
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

    // Async data fetchers — run in background threads
    void async_refresh_ticker();
    void async_refresh_orderbook();
    void async_refresh_watchlist_prices();
    void refresh_portfolio_data(); // DB only, fast, runs on main thread

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

    // Bottom sub-tabs
    void render_positions_tab();
    void render_orders_tab();
    void render_history_tab();
    void render_stats_tab();

    // Actions
    void submit_order();
    void cancel_order(const std::string& order_id);

    bool initialized_ = false;

    // --- Exchange state ---
    std::string exchange_id_ = "binance";
    std::string selected_symbol_ = "BTC/USDT";
    TradingMode trading_mode_ = TradingMode::Paper;

    // --- Watchlist ---
    std::vector<WatchlistEntry> watchlist_;
    int watchlist_selected_ = 0;
    char watchlist_filter_[64] = "";

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

    // --- Watchlist prices ---
    std::atomic<bool> wl_fetching_{false};

    // --- Order entry ---
    OrderFormState order_form_;

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

    // --- WebSocket state ---
    bool ws_started_ = false;
    int ws_price_cb_id_ = -1;
    int ws_ob_cb_id_ = -1;
    int ws_candle_cb_id_ = -1;

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

    // Default watchlist symbols
    static std::vector<std::string> default_watchlist_symbols();
};

} // namespace fincept::crypto
