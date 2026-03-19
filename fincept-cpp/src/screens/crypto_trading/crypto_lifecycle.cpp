#include "crypto_trading_screen.h"
#include "crypto_internal.h"
#include "ui/yoga_helpers.h"
#include "ui/theme.h"
#include "trading/paper_trading.h"
#include "trading/order_matcher.h"
#include "core/logger.h"
#include "core/notification.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <ctime>
#include <algorithm>
#include <chrono>
#include <thread>

namespace fincept::crypto {

using namespace theme::colors;

// ============================================================================
std::vector<std::string> CryptoTradingScreen::default_watchlist_symbols() {
    return {
        "BTC/USDT", "ETH/USDT", "BNB/USDT", "SOL/USDT", "XRP/USDT",
        "ADA/USDT", "DOGE/USDT", "AVAX/USDT", "DOT/USDT", "MATIC/USDT",
        "LINK/USDT", "UNI/USDT", "ATOM/USDT", "LTC/USDT", "FIL/USDT",
        "APT/USDT", "ARB/USDT", "OP/USDT", "NEAR/USDT", "SUI/USDT",
    };
}

// ============================================================================
// Destructor
// ============================================================================
CryptoTradingScreen::~CryptoTradingScreen() {
    LOG_INFO(TAG, "Shutting down");
    join_pending_threads();  // wait for in-flight fetches before tearing down (CP.26)

    auto& svc = trading::ExchangeService::instance();
    if (ws_started_) {
        svc.stop_ws_stream();
        ws_started_ = false;
    }
    if (ws_price_cb_id_ >= 0) svc.remove_price_callback(ws_price_cb_id_);
    if (ws_ob_cb_id_ >= 0) svc.remove_orderbook_callback(ws_ob_cb_id_);
    if (ws_candle_cb_id_ >= 0) svc.remove_candle_callback(ws_candle_cb_id_);
    if (ws_trade_cb_id_ >= 0) svc.remove_trade_callback(ws_trade_cb_id_);
}

// ============================================================================
// Thread lifecycle — RAII tracked async threads (CP.26: no detach)
// ============================================================================
void CryptoTradingScreen::launch_async(std::thread t) {
    std::lock_guard<std::mutex> lock(threads_mutex_);
    // Prune already-finished threads to keep the vector bounded
    async_threads_.erase(
        std::remove_if(async_threads_.begin(), async_threads_.end(),
                       [](std::thread& th) { return !th.joinable(); }),
        async_threads_.end());
    async_threads_.push_back(std::move(t));
}

void CryptoTradingScreen::join_pending_threads() {
    std::lock_guard<std::mutex> lock(threads_mutex_);
    for (auto& t : async_threads_) {
        if (t.joinable()) t.join();
    }
    async_threads_.clear();
}

// ============================================================================
// Initialization — fast, no blocking network calls
// ============================================================================
void CryptoTradingScreen::init() {
    LOG_INFO(TAG, "=== INIT START ===");

    // Ensure exchange_id has a value
    if (exchange_id_.empty()) {
        exchange_id_ = "binance";
        LOG_INFO(TAG, "exchange_id_ was empty, defaulting to binance");
    }
    if (selected_symbol_.empty()) {
        selected_symbol_ = "BTC/USDT";
    }

    // Set up exchange
    auto& svc = trading::ExchangeService::instance();
    svc.set_exchange(exchange_id_);
    LOG_INFO(TAG, "Exchange set to: %s", exchange_id_.c_str());

    // Load stored API credentials for this exchange (if any)
    load_exchange_credentials();

    // Build watchlist entries
    auto symbols = default_watchlist_symbols();
    watchlist_.clear();
    for (auto& s : symbols) {
        WatchlistEntry e;
        e.symbol = s;
        watchlist_.push_back(e);
    }
    LOG_INFO(TAG, "Watchlist initialized with %d symbols", (int)watchlist_.size());

    // Create or load paper trading portfolio (DB only, fast)
    load_portfolio();

    // Register portfolio with exchange service so position prices get updated
    if (!portfolio_id_.empty()) {
        svc.watch_symbol(selected_symbol_, portfolio_id_);
        // Also watch all symbols that have open positions
        for (const auto& pos : positions_) {
            if (pos.symbol != selected_symbol_) {
                svc.watch_symbol(pos.symbol, portfolio_id_);
            }
        }
    }

    // Start the price feed for watched symbols (drives OrderMatcher + position updates)
    if (!svc.is_feed_running()) {
        svc.start_price_feed(PRICE_FEED_POLL_SEC);
    }

    // Fetch available exchanges from ccxt (async, non-blocking)
    if (!exchanges_loaded_ && !exchanges_fetching_) {
        exchanges_fetching_ = true;
        launch_async(std::thread([this]() {
            try {
                auto ids = trading::ExchangeService::instance().list_exchange_ids();
                LOG_INFO(TAG, "Loaded %d exchange IDs from ccxt", (int)ids.size());
                std::lock_guard<std::mutex> lock(data_mutex_);
                available_exchanges_ = std::move(ids);
                exchanges_loaded_ = true;
            } catch (const std::exception& e) {
                LOG_ERROR(TAG, "Failed to load exchange IDs: %s", e.what());
            }
            exchanges_fetching_ = false;
        }));
    }

    initialized_ = true;
    LOG_INFO(TAG, "=== INIT COMPLETE === (portfolio_id=%s, balance=%.2f)",
             portfolio_id_.c_str(), portfolio_.balance);

    // Kick off initial REST fetches (fast initial data while WS connects)
    LOG_INFO(TAG, "Starting async data fetches...");
    ticker_loading_ = true;
    ob_loading_ = true;
    async_refresh_ticker();
    async_refresh_orderbook();
    async_refresh_watchlist_prices();

    // Fetch initial OHLCV candles via REST
    candles_loading_ = true;
    candles_fetching_ = true;
    const std::string sym = selected_symbol_;
    launch_async(std::thread([this, sym]() {
        try {
            auto ohlcv = trading::ExchangeService::instance().fetch_ohlcv(sym, "1m", OHLCV_FETCH_COUNT);
            LOG_INFO(TAG, "<<< Initial OHLCV received: %d candles", (int)ohlcv.size());
            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                candles_ = std::move(ohlcv);
            }
            candles_loading_ = false;
            candles_fetching_ = false;
            success_count_++;
        } catch (const std::exception& e) {
            LOG_ERROR(TAG, "<<< OHLCV fetch failed: %s", e.what());
            candles_loading_ = false;
            candles_fetching_ = false;
            error_count_++;
        }
    }));

    // Start WebSocket streaming for real-time updates
    // restart_ws_stream() handles callback registration + stream start
    restart_ws_stream();

    // Fetch initial trades and market info via REST
    async_fetch_trades();
    async_fetch_market_info();
}

void CryptoTradingScreen::load_portfolio() {
    LOG_INFO(TAG, "Loading paper trading portfolio for exchange: %s", exchange_id_.c_str());

    try {
        // Reset state so stale data from previous exchange doesn't leak
        portfolio_id_.clear();
        portfolio_ = {};
        positions_.clear();
        orders_.clear();
        trades_.clear();
        stats_ = {};
        portfolio_loaded_ = false;

        // Find portfolio scoped to this exchange
        auto found = trading::pt_find_portfolio("Crypto Paper Trading", exchange_id_);
        if (found) {
            portfolio_ = *found;
            portfolio_id_ = found->id;
            LOG_INFO(TAG, "Using existing portfolio: %s (exchange=%s, balance=%.2f)",
                     portfolio_id_.c_str(), exchange_id_.c_str(), portfolio_.balance);
        }

        // Create if not found for this exchange
        if (portfolio_id_.empty()) {
            portfolio_ = trading::pt_create_portfolio(
                "Crypto Paper Trading", DEFAULT_PAPER_BALANCE, "USDT",
                1.0, "cross", 0.001, exchange_id_);
            portfolio_id_ = portfolio_.id;
            LOG_INFO(TAG, "Created new portfolio: %s (exchange=%s, balance=%.2f)",
                     portfolio_id_.c_str(), exchange_id_.c_str(), portfolio_.balance);
        }

        // Load positions, orders, trades, stats from DB
        positions_ = trading::pt_get_positions(portfolio_id_);
        orders_ = trading::pt_get_orders(portfolio_id_);
        trades_ = trading::pt_get_trades(portfolio_id_, 50);
        stats_ = trading::pt_get_stats(portfolio_id_);
        portfolio_loaded_ = true;

        LOG_INFO(TAG, "Portfolio data loaded: %d positions, %d orders, %d trades",
                 (int)positions_.size(), (int)orders_.size(), (int)trades_.size());
    } catch (const std::exception& e) {
        LOG_ERROR(TAG, "Failed to load portfolio: %s", e.what());
        last_error_ = std::string("Portfolio load failed: ") + e.what();
        error_count_++;
    }
}

// ============================================================================
// Exchange & symbol switching — proper lifecycle management
// ============================================================================

void CryptoTradingScreen::clear_market_data() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    current_ticker_ = {};
    ob_bids_.clear();
    ob_asks_.clear();
    ob_spread_ = 0.0;
    ob_spread_pct_ = 0.0;
    candles_.clear();
    recent_trades_.clear();
    market_info_ = {};
    for (auto& w : watchlist_) {
        w.price = 0.0;
        w.change = 0.0;
        w.change_pct = 0.0;
        w.volume = 0.0;
        w.has_data = false;
    }
    last_error_.clear();
    error_count_ = 0;
    success_count_ = 0;
}

void CryptoTradingScreen::restart_ws_stream() {
    auto& svc = trading::ExchangeService::instance();

    // Tear down existing WS
    if (ws_started_) {
        svc.stop_ws_stream();
        ws_started_ = false;
    }

    // Remove old callbacks
    if (ws_price_cb_id_ >= 0) { svc.remove_price_callback(ws_price_cb_id_); ws_price_cb_id_ = -1; }
    if (ws_ob_cb_id_ >= 0) { svc.remove_orderbook_callback(ws_ob_cb_id_); ws_ob_cb_id_ = -1; }
    if (ws_candle_cb_id_ >= 0) { svc.remove_candle_callback(ws_candle_cb_id_); ws_candle_cb_id_ = -1; }
    if (ws_trade_cb_id_ >= 0) { svc.remove_trade_callback(ws_trade_cb_id_); ws_trade_cb_id_ = -1; }

    // Re-register callbacks (same as in init())
    ws_price_cb_id_ = svc.on_price_update([this](const std::string& symbol, const trading::TickerData& ticker) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        if (symbol == selected_symbol_) {
            current_ticker_ = ticker;
            ticker_loading_ = false;
        }
        for (auto& w : watchlist_) {
            if (w.symbol == symbol) {
                w.price = ticker.last;
                w.change = ticker.change;
                w.change_pct = ticker.percentage;
                w.volume = ticker.base_volume;
                w.has_data = true;
                break;
            }
        }
        last_data_update_ = time(nullptr);
    });

    ws_ob_cb_id_ = svc.on_orderbook_update([this](const std::string& symbol, const trading::OrderBookData& ob) {
        if (symbol != selected_symbol_) return;
        std::lock_guard<std::mutex> lock(data_mutex_);

        // Parse bids/asks into cumulative levels
        ob_bids_.clear();
        ob_asks_.clear();
        {
            double cum = 0;
            for (const auto& [price, amount] : ob.bids) {
                cum += amount;
                ob_bids_.push_back({price, amount, cum});
            }
        }
        {
            double cum = 0;
            for (const auto& [price, amount] : ob.asks) {
                cum += amount;
                ob_asks_.push_back({price, amount, cum});
            }
        }
        ob_spread_ = ob.spread;
        ob_spread_pct_ = ob.spread_pct;
        ob_loading_ = false;
        last_data_update_ = time(nullptr);

        // --- Tick history capture (throttled to OB_TICK_CAPTURE_MS) ---
        // Feeds Imbalance and Signals view modes with computed analytics.
        const int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        if (now_ms - last_tick_capture_ms_ >= static_cast<int64_t>(OB_TICK_CAPTURE_MS)) {
            last_tick_capture_ms_ = now_ms;

            TickSnapshot snap;
            snap.timestamp = now_ms;
            snap.best_bid  = ob.best_bid;
            snap.best_ask  = ob.best_ask;

            // Top-3 quantities from each side for weighted imbalance
            for (int i = 0; i < 3 && i < static_cast<int>(ob_bids_.size()); ++i)
                snap.bid_qty[i] = ob_bids_[i].amount;
            for (int i = 0; i < 3 && i < static_cast<int>(ob_asks_.size()); ++i)
                snap.ask_qty[i] = ob_asks_[i].amount;

            // Weighted imbalance: 50/30/20 top-3 levels (SGX formula, from Tauri impl)
            const double wBid = 50.0 * snap.bid_qty[0] + 30.0 * snap.bid_qty[1] + 20.0 * snap.bid_qty[2];
            const double wAsk = 50.0 * snap.ask_qty[0] + 30.0 * snap.ask_qty[1] + 20.0 * snap.ask_qty[2];
            snap.imbalance = (wBid + wAsk > 0.0) ? (wBid - wAsk) / (wBid + wAsk) : 0.0;

            // 60-second rise ratio: find oldest tick within 60s window
            const int64_t cutoff_60 = now_ms - 60000;
            snap.rise_ratio_60 = 0.0;
            for (int i = static_cast<int>(tick_history_.size()) - 1; i >= 0; --i) {
                if (tick_history_[i].timestamp <= cutoff_60 && tick_history_[i].best_ask > 0.0) {
                    snap.rise_ratio_60 = ((ob.best_ask - tick_history_[i].best_ask)
                                          / tick_history_[i].best_ask) * 100.0;
                    break;
                }
            }

            tick_history_.push_back(snap);
            if (static_cast<int>(tick_history_.size()) > OB_MAX_TICK_HISTORY)
                tick_history_.erase(tick_history_.begin());
        }
    });

    ws_candle_cb_id_ = svc.on_candle_update([this](const std::string& symbol, const trading::Candle& candle) {
        if (symbol != selected_symbol_) return;
        std::lock_guard<std::mutex> lock(data_mutex_);
        if (!candles_.empty() && candles_.back().timestamp == candle.timestamp) {
            candles_.back() = candle;
        } else {
            candles_.push_back(candle);
            if (candles_.size() > MAX_CANDLE_BUFFER) candles_.erase(candles_.begin());
        }
    });

    ws_trade_cb_id_ = svc.on_trade_update([this](const std::string& symbol, const trading::TradeData& trade) {
        if (symbol != selected_symbol_) return;
        std::lock_guard<std::mutex> lock(data_mutex_);
        recent_trades_.insert(recent_trades_.begin(), TradeEntry{
            trade.id, trade.side, trade.price, trade.amount, trade.timestamp
        });
        if ((int)recent_trades_.size() > MAX_TRADES) recent_trades_.resize(MAX_TRADES);
    });

    // Start new WS stream — cap to top N symbols to limit concurrent WS subscriptions.
    // Primary symbol is always first; remaining slots filled from watchlist in order.
    // Callbacks capture `this` — selected_symbol_ is read at invocation time,
    // so symbol switches (via set_ws_primary_symbol) take effect automatically
    // without restarting these callbacks.
    std::vector<std::string> all_symbols;
    all_symbols.reserve(WS_MAX_WATCHLIST_SYMBOLS + 1);
    all_symbols.push_back(selected_symbol_);
    for (const auto& w : watchlist_) {
        if ((int)all_symbols.size() >= WS_MAX_WATCHLIST_SYMBOLS) break;
        if (w.symbol != selected_symbol_) all_symbols.push_back(w.symbol);
    }

    try {
        svc.start_ws_stream(selected_symbol_, all_symbols);
        ws_started_ = true;
        LOG_INFO(TAG, "WS stream restarted for %s + %d symbols on %s",
                 selected_symbol_.c_str(), (int)all_symbols.size(), exchange_id_.c_str());
    } catch (const std::exception& e) {
        LOG_ERROR(TAG, "Failed to restart WS: %s", e.what());
        last_error_ = std::string("WS restart failed: ") + e.what();
    }
}

void CryptoTradingScreen::switch_exchange(const std::string& exchange_id) {
    if (exchange_id == exchange_id_) return;

    LOG_INFO(TAG, "Switching exchange: %s -> %s", exchange_id_.c_str(), exchange_id.c_str());

    // Stop price feed before switching (it uses the old exchange)
    auto& svc = trading::ExchangeService::instance();
    svc.stop_price_feed();

    // Update exchange
    exchange_id_ = exchange_id;
    svc.set_exchange(exchange_id);

    // Invalidate search cache — new exchange has different markets
    search_markets_loaded_ = false;
    search_cache_exchange_.clear();
    all_markets_.clear();
    search_results_.clear();

    // Load credentials for the new exchange
    load_exchange_credentials();
    live_data_loaded_ = false;

    // Clear all stale market data from previous exchange
    clear_market_data();

    // Swap portfolio — each exchange gets its own isolated paper portfolio
    // Unwatch symbols from old portfolio first
    if (!portfolio_id_.empty()) {
        svc.unwatch_symbol(selected_symbol_, portfolio_id_);
        for (const auto& pos : positions_) {
            svc.unwatch_symbol(pos.symbol, portfolio_id_);
        }
        trading::OrderMatcher::instance().clear_portfolio_orders(portfolio_id_);
    }
    load_portfolio();

    // Reset loading states
    ticker_loading_ = true;
    ob_loading_ = true;
    candles_loading_ = true;

    // Re-watch the selected symbol with the NEW portfolio
    if (!portfolio_id_.empty()) {
        svc.watch_symbol(selected_symbol_, portfolio_id_);
        for (const auto& pos : positions_) {
            if (pos.symbol != selected_symbol_) {
                svc.watch_symbol(pos.symbol, portfolio_id_);
            }
        }
    }
    svc.start_price_feed(PRICE_FEED_POLL_SEC);

    // Restart WebSocket stream for new exchange
    restart_ws_stream();

    // Kick off REST fetches for immediate data
    async_refresh_ticker();
    async_refresh_orderbook();
    async_refresh_watchlist_prices();
    async_fetch_trades();
    async_fetch_market_info();

    // Fetch fresh candles
    candles_fetching_ = true;
    const std::string sym = selected_symbol_;
    launch_async(std::thread([this, sym]() {
        try {
            auto ohlcv = trading::ExchangeService::instance().fetch_ohlcv(sym, "1m", OHLCV_FETCH_COUNT);
            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                candles_ = std::move(ohlcv);
            }
            candles_loading_ = false;
            candles_fetching_ = false;
        } catch (const std::exception& e) {
            LOG_ERROR(TAG, "OHLCV fetch failed on exchange switch: %s", e.what());
            candles_loading_ = false;
            candles_fetching_ = false;
        }
    }));

    LOG_INFO(TAG, "Exchange switch complete: %s", exchange_id.c_str());
}

void CryptoTradingScreen::switch_symbol(const std::string& symbol) {
    if (symbol == selected_symbol_) return;

    LOG_INFO(TAG, "Switching symbol: %s -> %s", selected_symbol_.c_str(), symbol.c_str());

    selected_symbol_ = symbol;

    // Clear symbol-specific data (keep watchlist prices intact)
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        current_ticker_ = {};
        ob_bids_.clear();
        ob_asks_.clear();
        ob_spread_ = 0.0;
        ob_spread_pct_ = 0.0;
        candles_.clear();
        recent_trades_.clear();
        market_info_ = {};
    }

    ticker_loading_ = true;
    ob_loading_ = true;
    candles_loading_ = true;

    // Watch new symbol for paper trading price updates
    if (!portfolio_id_.empty()) {
        trading::ExchangeService::instance().watch_symbol(symbol, portfolio_id_);
    }

    // Tell ExchangeService the new primary — WS subprocess stays alive,
    // callbacks filter on selected_symbol_ (member read at invocation time)
    // so they automatically route to the new symbol without a subprocess restart.
    trading::ExchangeService::instance().set_ws_primary_symbol(symbol);

    // Serve from price cache immediately — eliminates blank ticker on switch
    {
        const auto cached = trading::ExchangeService::instance().get_cached_price(symbol);
        if (cached.last > 0.0) {
            std::lock_guard<std::mutex> lock(data_mutex_);
            current_ticker_ = cached;
            ticker_loading_ = false;
            LOG_INFO(TAG, "Showing cached price for %s: %.4f", symbol.c_str(), cached.last);
        }
    }

    // REST fetches to fill data not yet in cache (orderbook, candles, trades, market info)
    async_refresh_ticker();
    async_refresh_orderbook();
    async_fetch_trades();
    async_fetch_market_info();

    // Fetch OHLCV candles for new symbol
    candles_fetching_ = true;
    const std::string sym = symbol;
    launch_async(std::thread([this, sym]() {
        try {
            auto ohlcv = trading::ExchangeService::instance().fetch_ohlcv(
                sym, candle_timeframe_, OHLCV_FETCH_COUNT);
            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                candles_ = std::move(ohlcv);
            }
            candles_loading_ = false;
            candles_fetching_ = false;
        } catch (const std::exception& e) {
            LOG_ERROR(TAG, "OHLCV fetch failed on symbol switch: %s", e.what());
            candles_loading_ = false;
            candles_fetching_ = false;
        }
    }));
}

// ============================================================================
// Async data fetchers — run in detached threads
// ============================================================================
void CryptoTradingScreen::async_refresh_ticker() {
    if (ticker_fetching_) {
        LOG_DEBUG(TAG, "Ticker fetch already in flight, skipping");
        return;
    }

    ticker_fetching_ = true;
    ticker_loading_ = true;
    const std::string symbol = selected_symbol_;
    LOG_INFO(TAG, ">>> Fetching ticker for %s...", symbol.c_str());

    launch_async(std::thread([this, symbol]() {
        try {
            auto ticker = trading::ExchangeService::instance().fetch_ticker(symbol);

            LOG_INFO(TAG, "<<< Ticker received: %s last=%.2f bid=%.2f ask=%.2f change=%.2f%%",
                     ticker.symbol.c_str(), ticker.last, ticker.bid, ticker.ask, ticker.percentage);

            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                current_ticker_ = ticker;

                // Also update the watchlist entry
                for (auto& w : watchlist_) {
                    if (w.symbol == symbol) {
                        w.price = ticker.last;
                        w.change = ticker.change;
                        w.change_pct = ticker.percentage;
                        w.volume = ticker.base_volume;
                        w.has_data = true;
                        break;
                    }
                }
            }

            ticker_loading_ = false;
            ticker_fetching_ = false;
            success_count_++;
            last_data_update_ = time(nullptr);
        } catch (const std::exception& e) {
            LOG_ERROR(TAG, "<<< Ticker fetch FAILED for %s: %s", symbol.c_str(), e.what());
            ticker_loading_ = false;
            ticker_fetching_ = false;
            last_error_ = std::string("Ticker: ") + e.what();
            error_count_++;
        }
    }));
}

void CryptoTradingScreen::async_refresh_orderbook() {
    if (ob_fetching_) {
        LOG_DEBUG(TAG, "Orderbook fetch already in flight, skipping");
        return;
    }

    ob_fetching_ = true;
    ob_loading_ = true;
    const std::string symbol = selected_symbol_;
    LOG_INFO(TAG, ">>> Fetching orderbook for %s...", symbol.c_str());

    launch_async(std::thread([this, symbol]() {
        try {
            auto ob = trading::ExchangeService::instance().fetch_orderbook(symbol, 15);

            LOG_INFO(TAG, "<<< Orderbook received: %s bids=%d asks=%d spread=%.4f (%.4f%%)",
                     ob.symbol.c_str(), (int)ob.bids.size(), (int)ob.asks.size(),
                     ob.spread, ob.spread_pct);

            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                ob_bids_.clear();
                ob_asks_.clear();

                double cum = 0;
                for (auto& [price, amount] : ob.bids) {
                    cum += amount;
                    ob_bids_.push_back({price, amount, cum});
                }

                cum = 0;
                for (auto& [price, amount] : ob.asks) {
                    cum += amount;
                    ob_asks_.push_back({price, amount, cum});
                }

                ob_spread_ = ob.spread;
                ob_spread_pct_ = ob.spread_pct;
            }

            ob_loading_ = false;
            ob_fetching_ = false;
            success_count_++;
            last_data_update_ = time(nullptr);
        } catch (const std::exception& e) {
            LOG_ERROR(TAG, "<<< Orderbook fetch FAILED for %s: %s", symbol.c_str(), e.what());
            ob_loading_ = false;
            ob_fetching_ = false;
            last_error_ = std::string("Orderbook: ") + e.what();
            error_count_++;
        }
    }));
}

void CryptoTradingScreen::async_refresh_watchlist_prices() {
    if (wl_fetching_) {
        LOG_DEBUG(TAG, "Watchlist price fetch already in flight, skipping");
        return;
    }

    wl_fetching_ = true;

    // Collect symbols
    std::vector<std::string> symbols;
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        for (auto& w : watchlist_) {
            symbols.push_back(w.symbol);
        }
    }

    LOG_INFO(TAG, ">>> Fetching watchlist prices for %d symbols...", (int)symbols.size());

    launch_async(std::thread([this, symbols]() {
        try {
            auto tickers = trading::ExchangeService::instance().fetch_tickers(symbols);

            LOG_INFO(TAG, "<<< Watchlist prices received: %d tickers", (int)tickers.size());

            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                for (auto& ticker : tickers) {
                    for (auto& w : watchlist_) {
                        if (w.symbol == ticker.symbol) {
                            w.price = ticker.last;
                            w.change = ticker.change;
                            w.change_pct = ticker.percentage;
                            w.volume = ticker.base_volume;
                            w.has_data = true;

                            LOG_DEBUG(TAG, "  WL %s: $%.2f (%.2f%%)",
                                      w.symbol.c_str(), w.price, w.change_pct);
                            break;
                        }
                    }

                    // Update current ticker if it matches selected symbol
                    if (ticker.symbol == selected_symbol_) {
                        current_ticker_ = ticker;
                    }
                }
            }

            wl_fetching_ = false;
            success_count_++;
            last_data_update_ = time(nullptr);
        } catch (const std::exception& e) {
            LOG_ERROR(TAG, "<<< Watchlist price fetch FAILED: %s", e.what());
            wl_fetching_ = false;
            last_error_ = std::string("Watchlist: ") + e.what();
            error_count_++;
        }
    }));
}

void CryptoTradingScreen::refresh_portfolio_data() {
    if (portfolio_id_.empty()) return;

    try {
        // Update position mark-to-market prices using current ticker data
        double last_price = current_ticker_.last;
        if (last_price > 0) {
            trading::pt_update_position_price(portfolio_id_, selected_symbol_, last_price);
        }

        // Also update prices for positions in other symbols from the price cache
        auto& svc = trading::ExchangeService::instance();
        for (const auto& pos : positions_) {
            if (pos.symbol == selected_symbol_) continue;
            auto cached = svc.get_cached_price(pos.symbol);
            if (cached.last > 0) {
                trading::pt_update_position_price(portfolio_id_, pos.symbol, cached.last);
            }
        }

        // Check SL/TP triggers for all open positions
        {
            auto& matcher = trading::OrderMatcher::instance();
            if (last_price > 0)
                matcher.check_sl_tp_triggers(portfolio_id_, selected_symbol_, last_price);
            for (const auto& pos : positions_) {
                if (pos.symbol == selected_symbol_) continue;
                const auto cached = svc.get_cached_price(pos.symbol);
                if (cached.last > 0)
                    matcher.check_sl_tp_triggers(portfolio_id_, pos.symbol, cached.last);
            }
        }

        std::lock_guard<std::mutex> lock(data_mutex_);
        portfolio_ = trading::pt_get_portfolio(portfolio_id_);
        positions_ = trading::pt_get_positions(portfolio_id_);
        orders_ = trading::pt_get_orders(portfolio_id_);
        trades_ = trading::pt_get_trades(portfolio_id_, 50);
        stats_ = trading::pt_get_stats(portfolio_id_);
    } catch (const std::exception& e) {
        LOG_ERROR(TAG, "Portfolio refresh failed: %s", e.what());
    }
}

} // namespace fincept::crypto
