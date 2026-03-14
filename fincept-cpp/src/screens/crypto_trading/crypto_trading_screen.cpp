#include "crypto_trading_screen.h"
#include "ui/yoga_helpers.h"
#include "ui/theme.h"
#include "trading/paper_trading.h"
#include "trading/order_matcher.h"
#include "core/logger.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <ctime>
#include <algorithm>
#include <thread>

namespace fincept::crypto {

using namespace theme::colors;
static const char* TAG = "CryptoTrading";

// ============================================================================
// Default watchlist — top crypto pairs
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
        svc.start_price_feed(3);
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
    std::string sym = selected_symbol_;
    std::thread([this, sym]() {
        try {
            auto ohlcv = trading::ExchangeService::instance().fetch_ohlcv(sym, "1m", 200);
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
    }).detach();

    // Start WebSocket streaming for real-time updates
    try {
        LOG_INFO(TAG, "Registering WS callbacks...");
        auto& svc2 = trading::ExchangeService::instance();

        // Register price callback
        ws_price_cb_id_ = svc2.on_price_update([this](const std::string& symbol, const trading::TickerData& ticker) {
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

        // Register orderbook callback
        ws_ob_cb_id_ = svc2.on_orderbook_update([this](const std::string& symbol, const trading::OrderBookData& ob) {
            if (symbol != selected_symbol_) return;
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
            ob_loading_ = false;
            last_data_update_ = time(nullptr);
        });

        // Register candle callback
        ws_candle_cb_id_ = svc2.on_candle_update([this](const std::string& symbol, const trading::Candle& candle) {
            if (symbol != selected_symbol_) return;
            std::lock_guard<std::mutex> lock(data_mutex_);

            if (!candles_.empty() && candles_.back().timestamp == candle.timestamp) {
                candles_.back() = candle;
            } else {
                candles_.push_back(candle);
                if (candles_.size() > 500) {
                    candles_.erase(candles_.begin());
                }
            }
        });

        // Register trade callback for Time & Sales
        ws_trade_cb_id_ = svc2.on_trade_update([this](const std::string& symbol, const trading::TradeData& trade) {
            if (symbol != selected_symbol_) return;
            std::lock_guard<std::mutex> lock(data_mutex_);
            recent_trades_.insert(recent_trades_.begin(), TradeEntry{
                trade.id, trade.side, trade.price, trade.amount, trade.timestamp
            });
            if ((int)recent_trades_.size() > MAX_TRADES) {
                recent_trades_.resize(MAX_TRADES);
            }
        });

        LOG_INFO(TAG, "WS callbacks registered, starting stream...");

        // Collect all watchlist symbols for WS
        std::vector<std::string> all_symbols;
        for (auto& w : watchlist_) all_symbols.push_back(w.symbol);

        svc2.start_ws_stream(selected_symbol_, all_symbols);
        ws_started_ = true;
        LOG_INFO(TAG, "WebSocket stream started for %s + %d symbols",
                 selected_symbol_.c_str(), (int)all_symbols.size());
    } catch (const std::exception& e) {
        LOG_ERROR(TAG, "Failed to start WS stream: %s", e.what());
        last_error_ = std::string("WS start failed: ") + e.what();
        error_count_++;
    }

    // Fetch initial trades and market info via REST
    async_fetch_trades();
    async_fetch_market_info();
}

void CryptoTradingScreen::load_portfolio() {
    LOG_INFO(TAG, "Loading paper trading portfolio...");

    try {
        // Try to find existing crypto paper trading portfolio
        auto portfolios = trading::pt_list_portfolios();
        LOG_INFO(TAG, "Found %d existing portfolios", (int)portfolios.size());

        for (auto& p : portfolios) {
            LOG_DEBUG(TAG, "  Portfolio: id=%s name='%s' balance=%.2f",
                      p.id.c_str(), p.name.c_str(), p.balance);
            if (p.name == "Crypto Paper Trading") {
                portfolio_ = p;
                portfolio_id_ = p.id;
                LOG_INFO(TAG, "Using existing portfolio: %s (balance=%.2f)",
                         portfolio_id_.c_str(), portfolio_.balance);
                break;
            }
        }

        // Create if not found
        if (portfolio_id_.empty()) {
            portfolio_ = trading::pt_create_portfolio(
                "Crypto Paper Trading", 100000.0, "USDT", 1.0, "cross", 0.001);
            portfolio_id_ = portfolio_.id;
            LOG_INFO(TAG, "Created new portfolio: %s (balance=%.2f)",
                     portfolio_id_.c_str(), portfolio_.balance);
        }

        // Cleanup: delete empty portfolios (not ours) to prevent DB bloat
        if (portfolios.size() > 10) {
            int cleaned = 0;
            for (const auto& p : portfolios) {
                if (p.id == portfolio_id_) continue;
                auto p_trades = trading::pt_get_trades(p.id, 1);
                auto p_positions = trading::pt_get_positions(p.id);
                if (p_trades.empty() && p_positions.empty()) {
                    trading::pt_delete_portfolio(p.id);
                    cleaned++;
                }
            }
            if (cleaned > 0) {
                LOG_INFO(TAG, "Cleaned up %d empty portfolios", cleaned);
            }
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
// Async data fetchers — run in detached threads
// ============================================================================
void CryptoTradingScreen::async_refresh_ticker() {
    if (ticker_fetching_) {
        LOG_DEBUG(TAG, "Ticker fetch already in flight, skipping");
        return;
    }

    ticker_fetching_ = true;
    ticker_loading_ = true;
    std::string symbol = selected_symbol_;
    LOG_INFO(TAG, ">>> Fetching ticker for %s...", symbol.c_str());

    std::thread([this, symbol]() {
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
    }).detach();
}

void CryptoTradingScreen::async_refresh_orderbook() {
    if (ob_fetching_) {
        LOG_DEBUG(TAG, "Orderbook fetch already in flight, skipping");
        return;
    }

    ob_fetching_ = true;
    ob_loading_ = true;
    std::string symbol = selected_symbol_;
    LOG_INFO(TAG, ">>> Fetching orderbook for %s...", symbol.c_str());

    std::thread([this, symbol]() {
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
    }).detach();
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

    std::thread([this, symbols]() {
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
    }).detach();
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

// ============================================================================
// Main render — Yoga-based responsive layout
// ============================================================================
void CryptoTradingScreen::render() {
    if (!initialized_) init();

    ui::ScreenFrame frame("##crypto_trading", ImVec2(0, 0), BG_DARKEST);
    if (!frame.begin()) { frame.end(); return; }

    float W = frame.width();
    float H = frame.height();

    // Vertical stack: top_nav(28) + ticker_bar(36) + main_area(flex) + status(22)
    auto vstack = ui::vstack_layout(W, H, {28, 36, -1, 22});

    render_top_nav(W, vstack.heights[0]);
    render_ticker_bar(W, vstack.heights[1]);

    // Main area — three-panel horizontal layout
    float main_h = vstack.heights[2];
    {
        auto panels = ui::three_panel_layout(W, main_h, 15, 22, 180, 280, 300, 4);

        ImGui::BeginChild("##crypto_left", ImVec2(panels.left_w, panels.left_h), false);
        render_watchlist(panels.left_w, panels.left_h);
        ImGui::EndChild();

        ImGui::SameLine(0, panels.gap);

        ImGui::BeginChild("##crypto_center", ImVec2(panels.center_w, panels.center_h), false);
        {
            float chart_h = panels.center_h * 0.55f;
            float bottom_h = panels.center_h - chart_h - 4;

            render_chart_area(panels.center_w, chart_h);
            ImGui::Dummy(ImVec2(0, 4));
            render_bottom_panel(panels.center_w, bottom_h);
        }
        ImGui::EndChild();

        if (panels.right_w > 0) {
            ImGui::SameLine(0, panels.gap);

            ImGui::BeginChild("##crypto_right", ImVec2(panels.right_w, panels.right_h), false);
            {
                float entry_h = panels.right_h * 0.45f;
                float book_h = panels.right_h - entry_h - 4;

                render_order_entry(panels.right_w, entry_h);
                ImGui::Dummy(ImVec2(0, 4));
                render_orderbook(panels.right_w, book_h);
            }
            ImGui::EndChild();
        }
    }

    render_status_bar(W, vstack.heights[3]);

    frame.end();

    // Periodic refreshes — WS handles ticker/orderbook/candles in real-time,
    // polling is fallback only (long intervals)
    float dt = ImGui::GetIO().DeltaTime;
    portfolio_timer_ += dt;

    bool ws_ok = trading::ExchangeService::instance().is_ws_connected();
    if (!ws_ok) {
        // WS not connected — fall back to faster REST polling
        ticker_timer_ += dt;
        ob_timer_ += dt;
        wl_timer_ += dt;

        if (ticker_timer_ >= 3.0f) {
            ticker_timer_ = 0;
            async_refresh_ticker();
        }
        if (ob_timer_ >= 3.0f) {
            ob_timer_ = 0;
            async_refresh_orderbook();
        }
        if (wl_timer_ >= 15.0f) {
            wl_timer_ = 0;
            async_refresh_watchlist_prices();
        }
    }

    if (portfolio_timer_ >= 1.5f) {
        portfolio_timer_ = 0;
        refresh_portfolio_data();
    }

    // Market info refresh (every 30s)
    market_info_timer_ += dt;
    if (market_info_timer_ >= 30.0f) {
        market_info_timer_ = 0;
        async_fetch_market_info();
    }

    // Clear success/error messages after timeout
    if (order_form_.msg_timer > 0) {
        order_form_.msg_timer -= dt;
        if (order_form_.msg_timer <= 0) {
            order_form_.error.clear();
            order_form_.success.clear();
        }
    }
}

// ============================================================================
// Top Navigation Bar
// ============================================================================
void CryptoTradingScreen::render_top_nav(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##crypto_topnav", ImVec2(w, h), true);

    ImGui::TextColored(ACCENT, "CRYPTO");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(TEXT_PRIMARY, "TRADING");
    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    // Exchange selector
    ImGui::TextColored(TEXT_DIM, "Exchange:");
    ImGui::SameLine(0, 4);
    ImGui::PushItemWidth(100);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    static const char* exchanges[] = {"binance", "kraken", "bybit", "okx", "coinbase"};
    static int ex_idx = 0;
    if (ImGui::Combo("##exchange", &ex_idx, exchanges, 5)) {
        exchange_id_ = exchanges[ex_idx];
        trading::ExchangeService::instance().set_exchange(exchange_id_);
        LOG_INFO(TAG, "Exchange changed to: %s", exchange_id_.c_str());
        // Force refresh with new exchange
        ticker_timer_ = 99;
        ob_timer_ = 99;
        wl_timer_ = 99;
    }
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    // Trading mode toggle
    bool is_paper = (trading_mode_ == TradingMode::Paper);
    ImGui::PushStyleColor(ImGuiCol_Button, is_paper ? ImVec4(0.0f, 0.5f, 0.2f, 1.0f) : BG_WIDGET);
    ImGui::PushStyleColor(ImGuiCol_Text, is_paper ? TEXT_PRIMARY : TEXT_DIM);
    if (ImGui::SmallButton("PAPER")) trading_mode_ = TradingMode::Paper;
    ImGui::PopStyleColor(2);

    ImGui::SameLine(0, 4);

    bool is_live = (trading_mode_ == TradingMode::Live);
    ImGui::PushStyleColor(ImGuiCol_Button, is_live ? ImVec4(0.7f, 0.1f, 0.1f, 1.0f) : BG_WIDGET);
    ImGui::PushStyleColor(ImGuiCol_Text, is_live ? TEXT_PRIMARY : TEXT_DIM);
    if (ImGui::SmallButton("LIVE")) trading_mode_ = TradingMode::Live;
    ImGui::PopStyleColor(2);

    // Right side: balance
    float balance_w = 200;
    float avail = ImGui::GetContentRegionAvail().x;
    if (avail > balance_w + 20) {
        ImGui::SameLine(ImGui::GetCursorPosX() + avail - balance_w);
        ImGui::TextColored(TEXT_DIM, "Balance:");
        ImGui::SameLine(0, 4);
        char bal_buf[32];
        std::snprintf(bal_buf, sizeof(bal_buf), "%.2f USDT", portfolio_.balance);
        ImGui::TextColored(MARKET_GREEN, "%s", bal_buf);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Ticker Bar — selected symbol price info
// ============================================================================
void CryptoTradingScreen::render_ticker_bar(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##crypto_ticker", ImVec2(w, h), true);

    // Symbol
    ImGui::SetWindowFontScale(1.2f);
    ImGui::TextColored(TEXT_PRIMARY, "%s", selected_symbol_.c_str());
    ImGui::SetWindowFontScale(1.0f);

    ImGui::SameLine(0, 16);

    double price = current_ticker_.last;

    if (price <= 0 && ticker_loading_) {
        theme::LoadingSpinner("Loading...");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    if (price <= 0) {
        ImGui::TextColored(TEXT_DIM, "Waiting for data...");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    // Price
    char buf[64];
    bool positive = current_ticker_.change >= 0;
    ImVec4 chg_col = positive ? MARKET_GREEN : MARKET_RED;

    if (price > 1.0)
        std::snprintf(buf, sizeof(buf), "%.2f", price);
    else
        std::snprintf(buf, sizeof(buf), "%.6f", price);

    ImGui::SetWindowFontScale(1.1f);
    ImGui::TextColored(chg_col, "%s", buf);
    ImGui::SetWindowFontScale(1.0f);

    ImGui::SameLine(0, 16);

    // Change
    std::snprintf(buf, sizeof(buf), "%s%.2f (%s%.2f%%)",
        positive ? "+" : "", current_ticker_.change,
        positive ? "+" : "", current_ticker_.percentage);
    ImGui::TextColored(chg_col, "%s", buf);

    ImGui::SameLine(0, 24);
    ImGui::TextColored(BORDER_BRIGHT, "|");

    // High / Low
    ImGui::SameLine(0, 12);
    ImGui::TextColored(TEXT_DIM, "H:");
    ImGui::SameLine(0, 4);
    std::snprintf(buf, sizeof(buf), "%.2f", current_ticker_.high);
    ImGui::TextColored(TEXT_SECONDARY, "%s", buf);

    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "L:");
    ImGui::SameLine(0, 4);
    std::snprintf(buf, sizeof(buf), "%.2f", current_ticker_.low);
    ImGui::TextColored(TEXT_SECONDARY, "%s", buf);

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");

    // Volume
    ImGui::SameLine(0, 12);
    ImGui::TextColored(TEXT_DIM, "Vol:");
    ImGui::SameLine(0, 4);
    double vol = current_ticker_.base_volume;
    if (vol >= 1e9)
        std::snprintf(buf, sizeof(buf), "%.2fB", vol / 1e9);
    else if (vol >= 1e6)
        std::snprintf(buf, sizeof(buf), "%.2fM", vol / 1e6);
    else if (vol >= 1e3)
        std::snprintf(buf, sizeof(buf), "%.2fK", vol / 1e3);
    else
        std::snprintf(buf, sizeof(buf), "%.2f", vol);
    ImGui::TextColored(TEXT_SECONDARY, "%s", buf);

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");

    // Bid / Ask
    ImGui::SameLine(0, 12);
    ImGui::TextColored(TEXT_DIM, "Bid:");
    ImGui::SameLine(0, 4);
    std::snprintf(buf, sizeof(buf), "%.2f", current_ticker_.bid);
    ImGui::TextColored(MARKET_GREEN, "%s", buf);

    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "Ask:");
    ImGui::SameLine(0, 4);
    std::snprintf(buf, sizeof(buf), "%.2f", current_ticker_.ask);
    ImGui::TextColored(MARKET_RED, "%s", buf);

    // Loading indicator
    if (ticker_fetching_) {
        ImGui::SameLine(0, 8);
        ImGui::TextColored(WARNING, "[*]");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Watchlist — left panel
// ============================================================================
void CryptoTradingScreen::render_watchlist(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##crypto_watchlist", ImVec2(w, h), true);

    // Header
    ImGui::TextColored(ACCENT, "WATCHLIST");
    ImGui::SameLine();

    int loaded = 0;
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        for (auto& w : watchlist_) if (w.has_data) loaded++;
    }
    ImGui::TextColored(TEXT_DIM, "(%d/%d)", loaded, (int)watchlist_.size());

    if (wl_fetching_) {
        ImGui::SameLine();
        ImGui::TextColored(WARNING, "[*]");
    }

    // Filter
    ImGui::PushItemWidth(-1);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::InputTextWithHint("##wl_filter", "Filter...", watchlist_filter_, sizeof(watchlist_filter_));
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    ImGui::Separator();

    // Scrollable list
    ImGui::BeginChild("##wl_scroll", ImVec2(0, 0), false);

    std::string filter_str(watchlist_filter_);
    for (auto& c : filter_str) c = (char)std::toupper((unsigned char)c);

    char buf[64];
    std::lock_guard<std::mutex> lock(data_mutex_);

    for (int i = 0; i < (int)watchlist_.size(); i++) {
        auto& entry = watchlist_[i];

        // Filter check
        if (!filter_str.empty()) {
            std::string sym_upper = entry.symbol;
            for (auto& c : sym_upper) c = (char)std::toupper((unsigned char)c);
            if (sym_upper.find(filter_str) == std::string::npos) continue;
        }

        bool is_selected = (entry.symbol == selected_symbol_);
        ImGui::PushID(i);

        ImGui::PushStyleColor(ImGuiCol_Header, BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, BG_HOVER);

        if (ImGui::Selectable("##wl_sel", is_selected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 22))) {
            watchlist_selected_ = i;
            // Watch new symbol for position price updates
            if (!portfolio_id_.empty() && entry.symbol != selected_symbol_) {
                trading::ExchangeService::instance().watch_symbol(entry.symbol, portfolio_id_);
            }
            selected_symbol_ = entry.symbol;
            LOG_INFO(TAG, "Symbol selected: %s", selected_symbol_.c_str());
            // Force immediate refresh
            ticker_timer_ = 99;
            ob_timer_ = 99;
        }

        ImGui::PopStyleColor(2);

        // Draw content over the selectable
        ImGui::SameLine(4);

        // Symbol name (truncate base only)
        std::string base = entry.symbol;
        auto slash = base.find('/');
        if (slash != std::string::npos) base = base.substr(0, slash);
        ImGui::TextColored(TEXT_PRIMARY, "%s", base.c_str());

        if (entry.has_data) {
            // Price on right
            if (entry.price > 1.0)
                std::snprintf(buf, sizeof(buf), "%.2f", entry.price);
            else if (entry.price > 0.01)
                std::snprintf(buf, sizeof(buf), "%.4f", entry.price);
            else
                std::snprintf(buf, sizeof(buf), "%.6f", entry.price);

            ImVec4 col = entry.change_pct >= 0 ? MARKET_GREEN : MARKET_RED;

            // Change % next to symbol
            ImGui::SameLine(0, 4);
            char pct_buf[16];
            std::snprintf(pct_buf, sizeof(pct_buf), "%.1f%%", entry.change_pct);
            ImGui::TextColored(col, "%s", pct_buf);

            // Price on far right
            float price_w = ImGui::CalcTextSize(buf).x;
            float avail = ImGui::GetContentRegionAvail().x;
            if (avail > price_w + 4) {
                ImGui::SameLine(w - price_w - 16);
                ImGui::TextColored(col, "%s", buf);
            }
        } else {
            float avail = ImGui::GetContentRegionAvail().x;
            if (avail > 20) {
                ImGui::SameLine(w - 30);
                ImGui::TextColored(TEXT_DIM, "...");
            }
        }

        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Chart Area (placeholder)
// ============================================================================
void CryptoTradingScreen::render_chart_area(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##crypto_chart", ImVec2(w, h), true);

    // Header: CHART label + symbol + WS indicator
    ImGui::TextColored(ACCENT, "CHART");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "%s", selected_symbol_.c_str());

    ImGui::SameLine(0, 8);
    bool ws_ok = trading::ExchangeService::instance().is_ws_connected();
    ImGui::TextColored(ws_ok ? MARKET_GREEN : WARNING, ws_ok ? "[WS LIVE]" : "[REST]");

    ImGui::SameLine(0, 16);

    // Timeframe buttons
    static const char* timeframes[] = {"1m", "5m", "15m", "1h", "4h", "1d"};
    for (int i = 0; i < 6; i++) {
        if (i > 0) ImGui::SameLine(0, 2);
        bool active = (i == selected_timeframe_);
        ImGui::PushStyleColor(ImGuiCol_Button, active ? ACCENT_DIM : BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_Text, active ? TEXT_PRIMARY : TEXT_DIM);
        if (ImGui::SmallButton(timeframes[i])) {
            if (selected_timeframe_ != i) {
                selected_timeframe_ = i;
                candle_timeframe_ = timeframes[i];
                // Re-fetch candles for new timeframe
                candles_loading_ = true;
                candles_fetching_ = true;
                std::string sym = selected_symbol_;
                std::string tf = candle_timeframe_;
                std::thread([this, sym, tf]() {
                    try {
                        auto ohlcv = trading::ExchangeService::instance().fetch_ohlcv(sym, tf, 200);
                        LOG_INFO(TAG, "<<< OHLCV %s received: %d candles", tf.c_str(), (int)ohlcv.size());
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
                }).detach();
            }
        }
        ImGui::PopStyleColor(2);
    }

    // Candle count
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        ImGui::SameLine(0, 12);
        ImGui::TextColored(TEXT_DIM, "[%d candles]", (int)candles_.size());
    }

    ImGui::Separator();

    float chart_w = ImGui::GetContentRegionAvail().x;
    float chart_h = ImGui::GetContentRegionAvail().y;

    std::lock_guard<std::mutex> lock(data_mutex_);
    if (chart_h > 60 && !candles_.empty()) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        render_candlestick_chart(p, chart_w, chart_h);
        ImGui::Dummy(ImVec2(chart_w, chart_h));
    } else if (candles_loading_) {
        ImGui::Dummy(ImVec2(0, 20));
        theme::LoadingSpinner("Loading OHLCV data...");
    } else if (chart_h > 60 && current_ticker_.last > 0) {
        // No candle data yet but have ticker — show price placeholder
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        char price_text[64];
        std::snprintf(price_text, sizeof(price_text), "%.2f", current_ticker_.last);
        ImVec2 text_size = ImGui::CalcTextSize(price_text);
        ImVec2 text_pos(p.x + (chart_w - text_size.x) / 2, p.y + (chart_h - text_size.y) / 2);
        dl->AddText(text_pos, ImGui::ColorConvertFloat4ToU32(TEXT_PRIMARY), price_text);
        ImGui::Dummy(ImVec2(chart_w, chart_h));
    } else {
        ImGui::Dummy(ImVec2(0, 20));
        theme::LoadingSpinner("Loading chart data...");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Candlestick Chart — renders OHLCV candles via ImDrawList
// ============================================================================
void CryptoTradingScreen::render_candlestick_chart(ImVec2 origin, float chart_w, float chart_h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Colors
    ImU32 grid_col = ImGui::ColorConvertFloat4ToU32(ImVec4(0.15f, 0.15f, 0.17f, 1.0f));
    ImU32 green = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.8f, 0.4f, 1.0f));
    ImU32 red = ImGui::ColorConvertFloat4ToU32(ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
    ImU32 green_dim = ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.6f, 0.3f, 0.6f));
    ImU32 red_dim = ImGui::ColorConvertFloat4ToU32(ImVec4(0.7f, 0.15f, 0.15f, 0.6f));
    ImU32 text_col = ImGui::ColorConvertFloat4ToU32(TEXT_DIM);
    ImU32 crosshair_col = ImGui::ColorConvertFloat4ToU32(ImVec4(0.4f, 0.4f, 0.5f, 0.5f));

    // Reserve right margin for price axis
    float price_axis_w = 70.0f;
    float plot_w = chart_w - price_axis_w;
    if (plot_w < 50) plot_w = chart_w;

    // Determine how many candles fit
    float candle_width = 6.0f;
    float candle_gap = 2.0f;
    float candle_step = candle_width + candle_gap;
    int max_visible = (int)(plot_w / candle_step);
    if (max_visible < 5) max_visible = 5;

    // Get visible range (show most recent candles)
    int total = (int)candles_.size();
    int start_idx = total > max_visible ? total - max_visible : 0;
    int visible_count = total - start_idx;
    if (visible_count <= 0) return;

    // Find price range
    double price_high = -1e18;
    double price_low = 1e18;
    for (int i = start_idx; i < total; i++) {
        const auto& c = candles_[i];
        if (c.high > price_high) price_high = c.high;
        if (c.low < price_low) price_low = c.low;
    }

    // Add 2% padding
    double range = price_high - price_low;
    if (range < 0.01) range = 1.0;
    double padding = range * 0.02;
    price_high += padding;
    price_low -= padding;
    range = price_high - price_low;

    // Helper: price to Y coordinate
    auto price_to_y = [&](double price) -> float {
        return origin.y + chart_h * (1.0f - (float)((price - price_low) / range));
    };

    // Draw grid lines (horizontal)
    int grid_lines = 5;
    for (int i = 0; i <= grid_lines; i++) {
        float y = origin.y + chart_h * i / (float)grid_lines;
        dl->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + plot_w, y), grid_col);

        // Price labels on right axis
        if (plot_w < chart_w) {
            double price = price_high - (range * i / grid_lines);
            char label[32];
            if (price >= 1000)
                std::snprintf(label, sizeof(label), "%.1f", price);
            else if (price >= 1)
                std::snprintf(label, sizeof(label), "%.2f", price);
            else
                std::snprintf(label, sizeof(label), "%.4f", price);
            dl->AddText(ImVec2(origin.x + plot_w + 4, y - 6), text_col, label);
        }
    }

    // Draw vertical grid lines
    int v_grid = 6;
    for (int i = 1; i < v_grid; i++) {
        float x = origin.x + plot_w * i / (float)v_grid;
        dl->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + chart_h), grid_col);
    }

    // Draw candles
    float x_start = origin.x + plot_w - visible_count * candle_step;

    for (int i = 0; i < visible_count; i++) {
        const auto& c = candles_[start_idx + i];
        float cx = x_start + i * candle_step + candle_width * 0.5f;

        bool bullish = c.close >= c.open;
        ImU32 body_col = bullish ? green : red;
        ImU32 wick_col = bullish ? green_dim : red_dim;

        // Wick (high-low line)
        float wick_x = cx;
        float wick_top = price_to_y(c.high);
        float wick_bot = price_to_y(c.low);
        dl->AddLine(ImVec2(wick_x, wick_top), ImVec2(wick_x, wick_bot), wick_col, 1.0f);

        // Body (open-close rect)
        float body_top = price_to_y(bullish ? c.close : c.open);
        float body_bot = price_to_y(bullish ? c.open : c.close);
        float body_min_h = 1.0f;
        if (body_bot - body_top < body_min_h) {
            body_bot = body_top + body_min_h;
        }

        float body_left = cx - candle_width * 0.5f;
        float body_right = cx + candle_width * 0.5f;
        dl->AddRectFilled(ImVec2(body_left, body_top), ImVec2(body_right, body_bot), body_col);
    }

    // Volume bars at bottom (subtle, 15% of chart height)
    double vol_max = 0;
    for (int i = start_idx; i < total; i++) {
        if (candles_[i].volume > vol_max) vol_max = candles_[i].volume;
    }

    if (vol_max > 0) {
        float vol_zone = chart_h * 0.15f;
        float vol_base = origin.y + chart_h;

        for (int i = 0; i < visible_count; i++) {
            const auto& c = candles_[start_idx + i];
            float cx = x_start + i * candle_step + candle_width * 0.5f;
            float vol_h = (float)(c.volume / vol_max) * vol_zone;
            bool bullish = c.close >= c.open;

            ImU32 vol_col = bullish
                ? ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.5f, 0.25f, 0.3f))
                : ImGui::ColorConvertFloat4ToU32(ImVec4(0.5f, 0.1f, 0.1f, 0.3f));

            float body_left = cx - candle_width * 0.5f;
            float body_right = cx + candle_width * 0.5f;
            dl->AddRectFilled(ImVec2(body_left, vol_base - vol_h),
                              ImVec2(body_right, vol_base), vol_col);
        }
    }

    // Current price line
    if (current_ticker_.last > 0 && current_ticker_.last >= price_low && current_ticker_.last <= price_high) {
        float y = price_to_y(current_ticker_.last);
        ImU32 price_line_col = ImGui::ColorConvertFloat4ToU32(
            current_ticker_.change >= 0 ? MARKET_GREEN : MARKET_RED);

        // Dashed line effect
        float dash_len = 6.0f;
        for (float x = origin.x; x < origin.x + plot_w; x += dash_len * 2) {
            float end = x + dash_len;
            if (end > origin.x + plot_w) end = origin.x + plot_w;
            dl->AddLine(ImVec2(x, y), ImVec2(end, y), price_line_col, 1.0f);
        }

        // Price tag on right
        if (plot_w < chart_w) {
            char tag[32];
            if (current_ticker_.last >= 1000)
                std::snprintf(tag, sizeof(tag), "%.1f", current_ticker_.last);
            else
                std::snprintf(tag, sizeof(tag), "%.2f", current_ticker_.last);

            ImVec2 tag_size = ImGui::CalcTextSize(tag);
            float tag_x = origin.x + plot_w + 2;
            float tag_y = y - tag_size.y * 0.5f;
            dl->AddRectFilled(ImVec2(tag_x, tag_y - 1),
                              ImVec2(tag_x + tag_size.x + 6, tag_y + tag_size.y + 1),
                              price_line_col);
            dl->AddText(ImVec2(tag_x + 3, tag_y),
                        ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 1)), tag);
        }
    }

    // Crosshair on hover
    ImVec2 mouse = ImGui::GetMousePos();
    if (mouse.x >= origin.x && mouse.x <= origin.x + plot_w &&
        mouse.y >= origin.y && mouse.y <= origin.y + chart_h) {

        dl->AddLine(ImVec2(origin.x, mouse.y), ImVec2(origin.x + plot_w, mouse.y), crosshair_col);
        dl->AddLine(ImVec2(mouse.x, origin.y), ImVec2(mouse.x, origin.y + chart_h), crosshair_col);

        // Price at cursor Y
        double hover_price = price_high - range * ((mouse.y - origin.y) / chart_h);
        char hover_label[32];
        std::snprintf(hover_label, sizeof(hover_label), "%.2f", hover_price);
        dl->AddText(ImVec2(origin.x + plot_w + 4, mouse.y - 6),
                    ImGui::ColorConvertFloat4ToU32(TEXT_PRIMARY), hover_label);
    }
}

// ============================================================================
// Bottom Panel — Positions / Orders / History / Stats
// ============================================================================
void CryptoTradingScreen::render_bottom_panel(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##crypto_bottom", ImVec2(w, h), true);

    struct TabDef { const char* label; BottomTab tab; };
    static const TabDef tabs[] = {
        {"Positions", BottomTab::Positions},
        {"Orders", BottomTab::Orders},
        {"History", BottomTab::History},
        {"Trades", BottomTab::Trades},
        {"Market Info", BottomTab::MarketInfo},
        {"Stats", BottomTab::Stats},
    };

    for (int i = 0; i < 6; i++) {
        if (i > 0) ImGui::SameLine(0, 2);
        bool active = (bottom_tab_ == tabs[i].tab);
        ImGui::PushStyleColor(ImGuiCol_Button, active ? ACCENT_BG : ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, active ? ACCENT : TEXT_DIM);
        if (ImGui::SmallButton(tabs[i].label)) {
            bottom_tab_ = tabs[i].tab;
        }
        ImGui::PopStyleColor(2);
    }

    // Count badges
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        ImGui::SameLine(0, 8);
        if (!positions_.empty()) {
            char badge[16];
            std::snprintf(badge, sizeof(badge), "[%d pos]", (int)positions_.size());
            ImGui::TextColored(ACCENT, "%s", badge);
            ImGui::SameLine(0, 4);
        }
        int pending = 0;
        for (auto& o : orders_) if (o.status == "pending") pending++;
        if (pending > 0) {
            char badge[16];
            std::snprintf(badge, sizeof(badge), "[%d ord]", pending);
            ImGui::TextColored(WARNING, "%s", badge);
        }
    }

    ImGui::Separator();

    ImGui::BeginChild("##bottom_content", ImVec2(0, 0), false);
    switch (bottom_tab_) {
        case BottomTab::Positions:  render_positions_tab(); break;
        case BottomTab::Orders:     render_orders_tab(); break;
        case BottomTab::History:    render_history_tab(); break;
        case BottomTab::Trades:     render_trades_tab(); break;
        case BottomTab::MarketInfo: render_market_info_tab(); break;
        case BottomTab::Stats:      render_stats_tab(); break;
    }
    ImGui::EndChild();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void CryptoTradingScreen::render_positions_tab() {
    std::lock_guard<std::mutex> lock(data_mutex_);

    if (!portfolio_loaded_) {
        theme::LoadingSpinner("Loading portfolio...");
        return;
    }

    if (positions_.empty()) {
        ImGui::TextColored(TEXT_DIM, "No open positions");
        return;
    }

    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("##positions", 7, flags)) {
        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("Side", ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupColumn("Entry", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("PnL", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Lev", ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, BG_DARKEST);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        char buf[32];
        for (auto& pos : positions_) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(TEXT_PRIMARY, "%s", pos.symbol.c_str());

            ImGui::TableSetColumnIndex(1);
            ImVec4 side_col = (pos.side == "long") ? MARKET_GREEN : MARKET_RED;
            ImGui::TextColored(side_col, "%s", pos.side == "long" ? "LONG" : "SHORT");

            ImGui::TableSetColumnIndex(2);
            std::snprintf(buf, sizeof(buf), "%.4f", pos.quantity);
            ImGui::TextColored(TEXT_SECONDARY, "%s", buf);

            ImGui::TableSetColumnIndex(3);
            std::snprintf(buf, sizeof(buf), "%.2f", pos.entry_price);
            ImGui::TextColored(TEXT_SECONDARY, "%s", buf);

            ImGui::TableSetColumnIndex(4);
            std::snprintf(buf, sizeof(buf), "%.2f", pos.current_price);
            ImGui::TextColored(TEXT_PRIMARY, "%s", buf);

            ImGui::TableSetColumnIndex(5);
            ImVec4 pnl_col = pos.unrealized_pnl >= 0 ? MARKET_GREEN : MARKET_RED;
            std::snprintf(buf, sizeof(buf), "%s%.2f",
                pos.unrealized_pnl >= 0 ? "+" : "", pos.unrealized_pnl);
            ImGui::TextColored(pnl_col, "%s", buf);

            ImGui::TableSetColumnIndex(6);
            std::snprintf(buf, sizeof(buf), "%.0fx", pos.leverage);
            ImGui::TextColored(TEXT_DIM, "%s", buf);
        }

        ImGui::EndTable();
    }
}

void CryptoTradingScreen::render_orders_tab() {
    std::lock_guard<std::mutex> lock(data_mutex_);

    if (!portfolio_loaded_) {
        theme::LoadingSpinner("Loading orders...");
        return;
    }

    if (orders_.empty()) {
        ImGui::TextColored(TEXT_DIM, "No orders");
        return;
    }

    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("##orders", 7, flags)) {
        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Side", ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.7f);
        ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 0.7f);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, BG_DARKEST);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        char buf[32];
        for (auto& ord : orders_) {
            ImGui::TableNextRow();
            ImGui::PushID(ord.id.c_str());

            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(TEXT_PRIMARY, "%s", ord.symbol.c_str());

            ImGui::TableSetColumnIndex(1);
            ImVec4 side_col = (ord.side == "buy") ? MARKET_GREEN : MARKET_RED;
            ImGui::TextColored(side_col, "%s", ord.side == "buy" ? "BUY" : "SELL");

            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(TEXT_SECONDARY, "%s", ord.order_type.c_str());

            ImGui::TableSetColumnIndex(3);
            std::snprintf(buf, sizeof(buf), "%.4f", ord.quantity);
            ImGui::TextColored(TEXT_SECONDARY, "%s", buf);

            ImGui::TableSetColumnIndex(4);
            if (ord.price.has_value()) {
                std::snprintf(buf, sizeof(buf), "%.2f", *ord.price);
                ImGui::TextColored(TEXT_PRIMARY, "%s", buf);
            } else {
                ImGui::TextColored(TEXT_DIM, "MKT");
            }

            ImGui::TableSetColumnIndex(5);
            ImVec4 status_col = TEXT_DIM;
            if (ord.status == "pending") status_col = WARNING;
            else if (ord.status == "filled") status_col = MARKET_GREEN;
            else if (ord.status == "cancelled") status_col = MARKET_RED;
            ImGui::TextColored(status_col, "%s", ord.status.c_str());

            ImGui::TableSetColumnIndex(6);
            if (ord.status == "pending") {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_Text, MARKET_RED);
                if (ImGui::SmallButton("Cancel")) {
                    cancel_order(ord.id);
                }
                ImGui::PopStyleColor(2);
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

void CryptoTradingScreen::render_history_tab() {
    std::lock_guard<std::mutex> lock(data_mutex_);

    if (!portfolio_loaded_) {
        theme::LoadingSpinner("Loading history...");
        return;
    }

    if (trades_.empty()) {
        ImGui::TextColored(TEXT_DIM, "No trade history");
        return;
    }

    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("##history", 6, flags)) {
        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Side", ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthStretch, 0.7f);
        ImGui::TableSetupColumn("Fee", ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableSetupColumn("PnL", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, BG_DARKEST);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        char buf[32];
        for (auto& t : trades_) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(TEXT_PRIMARY, "%s", t.symbol.c_str());

            ImGui::TableSetColumnIndex(1);
            ImVec4 side_col = (t.side == "buy") ? MARKET_GREEN : MARKET_RED;
            ImGui::TextColored(side_col, "%s", t.side == "buy" ? "BUY" : "SELL");

            ImGui::TableSetColumnIndex(2);
            std::snprintf(buf, sizeof(buf), "%.2f", t.price);
            ImGui::TextColored(TEXT_SECONDARY, "%s", buf);

            ImGui::TableSetColumnIndex(3);
            std::snprintf(buf, sizeof(buf), "%.4f", t.quantity);
            ImGui::TextColored(TEXT_SECONDARY, "%s", buf);

            ImGui::TableSetColumnIndex(4);
            std::snprintf(buf, sizeof(buf), "%.4f", t.fee);
            ImGui::TextColored(TEXT_DIM, "%s", buf);

            ImGui::TableSetColumnIndex(5);
            ImVec4 pnl_col = t.pnl >= 0 ? MARKET_GREEN : MARKET_RED;
            std::snprintf(buf, sizeof(buf), "%s%.2f", t.pnl >= 0 ? "+" : "", t.pnl);
            ImGui::TextColored(pnl_col, "%s", buf);
        }

        ImGui::EndTable();
    }
}

void CryptoTradingScreen::render_stats_tab() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    char buf[32];

    if (!portfolio_loaded_) {
        theme::LoadingSpinner("Loading stats...");
        return;
    }

    ImGui::Columns(2, "##stats_cols", false);

    // Compute unrealized PnL from open positions
    double unrealized_pnl = 0.0;
    for (const auto& pos : positions_) {
        unrealized_pnl += pos.unrealized_pnl;
    }

    ImGui::TextColored(TEXT_DIM, "Realized PnL");
    ImVec4 pnl_col = stats_.total_pnl >= 0 ? MARKET_GREEN : MARKET_RED;
    std::snprintf(buf, sizeof(buf), "%s%.2f", stats_.total_pnl >= 0 ? "+" : "", stats_.total_pnl);
    ImGui::TextColored(pnl_col, "%s", buf);
    ImGui::Spacing();

    ImGui::TextColored(TEXT_DIM, "Unrealized PnL");
    ImVec4 upnl_col = unrealized_pnl >= 0 ? MARKET_GREEN : MARKET_RED;
    std::snprintf(buf, sizeof(buf), "%s%.2f", unrealized_pnl >= 0 ? "+" : "", unrealized_pnl);
    ImGui::TextColored(upnl_col, "%s", buf);
    ImGui::Spacing();

    ImGui::TextColored(TEXT_DIM, "Win Rate");
    std::snprintf(buf, sizeof(buf), "%.1f%%", stats_.win_rate);
    ImGui::TextColored(TEXT_PRIMARY, "%s", buf);
    ImGui::Spacing();

    ImGui::TextColored(TEXT_DIM, "Total Trades");
    std::snprintf(buf, sizeof(buf), "%lld", (long long)stats_.total_trades);
    ImGui::TextColored(TEXT_PRIMARY, "%s", buf);
    ImGui::Spacing();

    ImGui::TextColored(TEXT_DIM, "Balance");
    std::snprintf(buf, sizeof(buf), "%.2f", portfolio_.balance);
    ImGui::TextColored(TEXT_PRIMARY, "%s", buf);

    ImGui::NextColumn();

    ImGui::TextColored(TEXT_DIM, "Winning");
    std::snprintf(buf, sizeof(buf), "%lld", (long long)stats_.winning_trades);
    ImGui::TextColored(MARKET_GREEN, "%s", buf);
    ImGui::Spacing();

    ImGui::TextColored(TEXT_DIM, "Losing");
    std::snprintf(buf, sizeof(buf), "%lld", (long long)stats_.losing_trades);
    ImGui::TextColored(MARKET_RED, "%s", buf);
    ImGui::Spacing();

    ImGui::TextColored(TEXT_DIM, "Largest Win");
    std::snprintf(buf, sizeof(buf), "+%.2f", stats_.largest_win);
    ImGui::TextColored(MARKET_GREEN, "%s", buf);
    ImGui::Spacing();

    ImGui::TextColored(TEXT_DIM, "Largest Loss");
    std::snprintf(buf, sizeof(buf), "%.2f", stats_.largest_loss);
    ImGui::TextColored(MARKET_RED, "%s", buf);

    ImGui::Columns(1);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
    if (ImGui::Button("Reset Portfolio", ImVec2(-1, 0))) {
        portfolio_ = trading::pt_reset_portfolio(portfolio_id_);
        refresh_portfolio_data();
        LOG_INFO(TAG, "Portfolio reset");
    }
    ImGui::PopStyleColor();
}

// ============================================================================
// Trades tab — Time & Sales feed
// ============================================================================
void CryptoTradingScreen::render_trades_tab() {
    std::lock_guard<std::mutex> lock(data_mutex_);

    if (recent_trades_.empty()) {
        if (trades_fetching_) {
            ImGui::TextColored(TEXT_DIM, "Loading trades...");
        } else {
            ImGui::TextColored(TEXT_DIM, "No trades yet");
        }
        return;
    }

    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("##trades_feed", 4, flags)) {
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Side", ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Amount", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, BG_DARKEST);
        ImGui::TableHeadersRow();
        ImGui::PopStyleColor();

        char buf[32];
        for (const auto& t : recent_trades_) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            time_t ts = t.timestamp / 1000;
            struct tm tm_buf{};
#ifdef _WIN32
            localtime_s(&tm_buf, &ts);
#else
            localtime_r(&ts, &tm_buf);
#endif
            std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);
            ImGui::TextColored(TEXT_DIM, "%s", buf);

            ImGui::TableSetColumnIndex(1);
            ImVec4 col = (t.side == "buy") ? MARKET_GREEN : MARKET_RED;
            ImGui::TextColored(col, "%s", t.side == "buy" ? "BUY" : "SELL");

            ImGui::TableSetColumnIndex(2);
            std::snprintf(buf, sizeof(buf), "%.2f", t.price);
            ImGui::TextColored(col, "%s", buf);

            ImGui::TableSetColumnIndex(3);
            std::snprintf(buf, sizeof(buf), "%.4f", t.amount);
            ImGui::TextColored(TEXT_SECONDARY, "%s", buf);
        }

        ImGui::EndTable();
    }
}

void CryptoTradingScreen::async_fetch_trades() {
    if (trades_fetching_) return;
    trades_fetching_ = true;

    std::string sym = selected_symbol_;
    std::thread([this, sym]() {
        try {
            auto trades = trading::ExchangeService::instance().fetch_trades(sym, 100);
            std::lock_guard<std::mutex> lock(data_mutex_);
            recent_trades_.clear();
            for (const auto& t : trades) {
                recent_trades_.push_back({t.id, t.side, t.price, t.amount, t.timestamp});
            }
        } catch (const std::exception& e) {
            LOG_ERROR("CryptoTrading", "Failed to fetch trades: %s", e.what());
        }
        trades_fetching_ = false;
    }).detach();
}

// ============================================================================
// Market Info tab — funding rate, open interest, fees
// ============================================================================
void CryptoTradingScreen::render_market_info_tab() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    char buf[64];

    if (!market_info_.has_data) {
        if (market_info_fetching_) {
            ImGui::TextColored(TEXT_DIM, "Loading market info...");
        } else {
            ImGui::TextColored(TEXT_DIM, "No market info available (spot mode)");
        }
        return;
    }

    ImGui::Columns(2, "##mktinfo_cols", false);

    ImGui::TextColored(TEXT_DIM, "Funding Rate");
    std::snprintf(buf, sizeof(buf), "%.4f%%", market_info_.funding_rate * 100);
    ImVec4 fr_col = market_info_.funding_rate >= 0 ? MARKET_GREEN : MARKET_RED;
    ImGui::TextColored(fr_col, "%s", buf);
    ImGui::Spacing();

    ImGui::TextColored(TEXT_DIM, "Mark Price");
    std::snprintf(buf, sizeof(buf), "%.2f", market_info_.mark_price);
    ImGui::TextColored(TEXT_PRIMARY, "%s", buf);
    ImGui::Spacing();

    ImGui::TextColored(TEXT_DIM, "Index Price");
    std::snprintf(buf, sizeof(buf), "%.2f", market_info_.index_price);
    ImGui::TextColored(TEXT_PRIMARY, "%s", buf);

    ImGui::NextColumn();

    ImGui::TextColored(TEXT_DIM, "Open Interest");
    std::snprintf(buf, sizeof(buf), "%.2f", market_info_.open_interest);
    ImGui::TextColored(TEXT_PRIMARY, "%s", buf);
    ImGui::Spacing();

    ImGui::TextColored(TEXT_DIM, "OI Value (USD)");
    double oi_val = market_info_.open_interest_value;
    if (oi_val > 1e9) std::snprintf(buf, sizeof(buf), "%.2fB", oi_val / 1e9);
    else if (oi_val > 1e6) std::snprintf(buf, sizeof(buf), "%.2fM", oi_val / 1e6);
    else std::snprintf(buf, sizeof(buf), "%.0f", oi_val);
    ImGui::TextColored(TEXT_PRIMARY, "%s", buf);
    ImGui::Spacing();

    if (market_info_.next_funding_time > 0) {
        ImGui::TextColored(TEXT_DIM, "Next Funding");
        int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        int64_t diff_s = (market_info_.next_funding_time - now_ms) / 1000;
        if (diff_s > 0) {
            int h = (int)(diff_s / 3600);
            int m = (int)((diff_s % 3600) / 60);
            std::snprintf(buf, sizeof(buf), "%dh %dm", h, m);
        } else {
            std::snprintf(buf, sizeof(buf), "Now");
        }
        ImGui::TextColored(TEXT_PRIMARY, "%s", buf);
    }

    ImGui::Columns(1);
}

void CryptoTradingScreen::async_fetch_market_info() {
    if (market_info_fetching_) return;
    market_info_fetching_ = true;

    std::string sym = selected_symbol_;
    std::thread([this, sym]() {
        try {
            auto& svc = trading::ExchangeService::instance();
            auto fr = svc.fetch_funding_rate(sym);
            auto oi = svc.fetch_open_interest(sym);

            std::lock_guard<std::mutex> lock(data_mutex_);
            market_info_.funding_rate = fr.funding_rate;
            market_info_.mark_price = fr.mark_price;
            market_info_.index_price = fr.index_price;
            market_info_.next_funding_time = fr.next_funding_timestamp;
            market_info_.open_interest = oi.open_interest;
            market_info_.open_interest_value = oi.open_interest_value;
            market_info_.has_data = true;
        } catch (const std::exception& e) {
            LOG_DEBUG("CryptoTrading", "Market info fetch failed: %s", e.what());
        }
        market_info_fetching_ = false;
    }).detach();
}

// ============================================================================
// Order Entry — right panel top
// ============================================================================
void CryptoTradingScreen::render_order_entry(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##crypto_order_entry", ImVec2(w, h), true);

    ImGui::TextColored(ACCENT, "ORDER ENTRY");
    ImGui::Separator();

    float input_w = w - 24;

    // Side: Buy / Sell
    ImGui::TextColored(TEXT_DIM, "Side");
    {
        float btn_w = (input_w - 4) / 2;
        bool is_buy = (order_form_.side_idx == 0);

        ImGui::PushStyleColor(ImGuiCol_Button, is_buy ? ImVec4(0.0f, 0.55f, 0.25f, 1.0f) : BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_Text, is_buy ? TEXT_PRIMARY : TEXT_DIM);
        if (ImGui::Button("BUY", ImVec2(btn_w, 0))) order_form_.side_idx = 0;
        ImGui::PopStyleColor(2);

        ImGui::SameLine(0, 4);

        bool is_sell = (order_form_.side_idx == 1);
        ImGui::PushStyleColor(ImGuiCol_Button, is_sell ? ImVec4(0.65f, 0.1f, 0.1f, 1.0f) : BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_Text, is_sell ? TEXT_PRIMARY : TEXT_DIM);
        if (ImGui::Button("SELL", ImVec2(btn_w, 0))) order_form_.side_idx = 1;
        ImGui::PopStyleColor(2);
    }

    ImGui::Spacing();

    // Order type
    ImGui::TextColored(TEXT_DIM, "Type");
    ImGui::PushItemWidth(input_w);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    static const char* types[] = {"Market", "Limit", "Stop", "Stop Limit"};
    ImGui::Combo("##order_type", &order_form_.type_idx, types, 4);
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    ImGui::Spacing();

    // Quantity
    ImGui::TextColored(TEXT_DIM, "Quantity");
    ImGui::PushItemWidth(input_w);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::InputText("##qty", order_form_.quantity_buf, sizeof(order_form_.quantity_buf),
                     ImGuiInputTextFlags_CharsDecimal);
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    // Quick quantity buttons
    {
        float btn_w = (input_w - 12) / 4;
        const char* pcts[] = {"25%", "50%", "75%", "100%"};
        float pct_vals[] = {0.25f, 0.50f, 0.75f, 1.00f};
        for (int i = 0; i < 4; i++) {
            if (i > 0) ImGui::SameLine(0, 4);
            ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
            if (ImGui::Button(pcts[i], ImVec2(btn_w, 0))) {
                if (current_ticker_.last > 0 && portfolio_.balance > 0) {
                    double max_qty = (portfolio_.balance * pct_vals[i]) / current_ticker_.last;
                    std::snprintf(order_form_.quantity_buf, sizeof(order_form_.quantity_buf),
                                  "%.6f", max_qty);
                }
            }
            ImGui::PopStyleColor(2);
        }
    }

    ImGui::Spacing();

    // Price (for limit/stop orders)
    if (order_form_.type_idx >= 1) {
        ImGui::TextColored(TEXT_DIM, "Price");
        ImGui::PushItemWidth(input_w);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
        ImGui::InputText("##price", order_form_.price_buf, sizeof(order_form_.price_buf),
                         ImGuiInputTextFlags_CharsDecimal);
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();
    }

    // Stop price (for stop/stoplimit)
    if (order_form_.type_idx >= 2) {
        ImGui::TextColored(TEXT_DIM, "Stop Price");
        ImGui::PushItemWidth(input_w);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
        ImGui::InputText("##stop_price", order_form_.stop_price_buf, sizeof(order_form_.stop_price_buf),
                         ImGuiInputTextFlags_CharsDecimal);
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();
    }

    ImGui::Spacing();

    // Reduce only checkbox
    ImGui::Checkbox("Reduce Only", &order_form_.reduce_only);

    ImGui::Spacing();

    // Submit button
    {
        bool is_buy = (order_form_.side_idx == 0);
        ImVec4 btn_col = is_buy ? ImVec4(0.0f, 0.55f, 0.25f, 1.0f) : ImVec4(0.65f, 0.1f, 0.1f, 1.0f);
        char label[64];
        std::snprintf(label, sizeof(label), "%s %s", is_buy ? "BUY" : "SELL", selected_symbol_.c_str());

        ImGui::PushStyleColor(ImGuiCol_Button, btn_col);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(btn_col.x + 0.1f, btn_col.y + 0.1f, btn_col.z + 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
        if (ImGui::Button(label, ImVec2(input_w, 28))) {
            submit_order();
        }
        ImGui::PopStyleColor(3);
    }

    // Error / Success messages
    if (!order_form_.error.empty()) {
        ImGui::TextColored(MARKET_RED, "%s", order_form_.error.c_str());
    }
    if (!order_form_.success.empty()) {
        ImGui::TextColored(MARKET_GREEN, "%s", order_form_.success.c_str());
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Order Book — right panel bottom
// ============================================================================
void CryptoTradingScreen::render_orderbook(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##crypto_orderbook", ImVec2(w, h), true);

    ImGui::TextColored(ACCENT, "ORDER BOOK");

    std::lock_guard<std::mutex> lock(data_mutex_);

    if (ob_spread_ > 0) {
        ImGui::SameLine();
        char spread_buf[32];
        std::snprintf(spread_buf, sizeof(spread_buf), "Spread: %.2f (%.3f%%)", ob_spread_, ob_spread_pct_);
        ImGui::TextColored(TEXT_DIM, "%s", spread_buf);
    }

    if (ob_fetching_) {
        ImGui::SameLine();
        ImGui::TextColored(WARNING, "[*]");
    }

    ImGui::Separator();

    if (ob_asks_.empty() && ob_bids_.empty()) {
        if (ob_loading_) {
            theme::LoadingSpinner("Loading order book...");
        } else {
            ImGui::TextColored(TEXT_DIM, "No order book data");
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    float avail_h = ImGui::GetContentRegionAvail().y;
    float half_h = avail_h / 2.0f;
    float col_w = (w - 24) / 3.0f;

    // Find max cumulative for depth bar scaling
    double max_cum = 1.0;
    if (!ob_bids_.empty()) max_cum = std::max(max_cum, ob_bids_.back().cumulative);
    if (!ob_asks_.empty()) max_cum = std::max(max_cum, ob_asks_.back().cumulative);

    // Header
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    ImGui::Text("PRICE");
    ImGui::SameLine(col_w + 4);
    ImGui::Text("AMOUNT");
    ImGui::SameLine(col_w * 2 + 8);
    ImGui::Text("TOTAL");
    ImGui::PopStyleColor();

    // Asks (sell side) — reversed so lowest ask is near the spread
    ImGui::BeginChild("##ob_asks", ImVec2(0, half_h - 12), false);
    {
        char buf[32];
        int start = std::max(0, (int)ob_asks_.size() - 12);
        for (int i = start; i < (int)ob_asks_.size(); i++) {
            auto& lvl = ob_asks_[i];
            ImVec2 p = ImGui::GetCursorScreenPos();
            float row_w = ImGui::GetContentRegionAvail().x;

            float bar_w = (float)(lvl.cumulative / max_cum) * row_w;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(
                ImVec2(p.x + row_w - bar_w, p.y),
                ImVec2(p.x + row_w, p.y + ImGui::GetTextLineHeight()),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.6f, 0.1f, 0.1f, 0.15f))
            );

            std::snprintf(buf, sizeof(buf), "%.2f", lvl.price);
            ImGui::TextColored(MARKET_RED, "%s", buf);
            ImGui::SameLine(col_w + 4);
            std::snprintf(buf, sizeof(buf), "%.4f", lvl.amount);
            ImGui::TextColored(TEXT_SECONDARY, "%s", buf);
            ImGui::SameLine(col_w * 2 + 8);
            std::snprintf(buf, sizeof(buf), "%.4f", lvl.cumulative);
            ImGui::TextColored(TEXT_DIM, "%s", buf);
        }
    }
    ImGui::EndChild();

    // Spread line
    ImGui::PushStyleColor(ImGuiCol_Separator, BORDER_BRIGHT);
    ImGui::Separator();
    ImGui::PopStyleColor();

    // Bids (buy side) — top to bottom (highest to lowest)
    ImGui::BeginChild("##ob_bids", ImVec2(0, 0), false);
    {
        char buf[32];
        int count = std::min((int)ob_bids_.size(), 12);
        for (int i = 0; i < count; i++) {
            auto& lvl = ob_bids_[i];
            ImVec2 p = ImGui::GetCursorScreenPos();
            float row_w = ImGui::GetContentRegionAvail().x;

            float bar_w = (float)(lvl.cumulative / max_cum) * row_w;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(
                ImVec2(p.x + row_w - bar_w, p.y),
                ImVec2(p.x + row_w, p.y + ImGui::GetTextLineHeight()),
                ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.5f, 0.2f, 0.15f))
            );

            std::snprintf(buf, sizeof(buf), "%.2f", lvl.price);
            ImGui::TextColored(MARKET_GREEN, "%s", buf);
            ImGui::SameLine(col_w + 4);
            std::snprintf(buf, sizeof(buf), "%.4f", lvl.amount);
            ImGui::TextColored(TEXT_SECONDARY, "%s", buf);
            ImGui::SameLine(col_w * 2 + 8);
            std::snprintf(buf, sizeof(buf), "%.4f", lvl.cumulative);
            ImGui::TextColored(TEXT_DIM, "%s", buf);
        }
    }
    ImGui::EndChild();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Status Bar — shows connection, errors, debug info
// ============================================================================
void CryptoTradingScreen::render_status_bar(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##crypto_status", ImVec2(w, h), true);

    // Connection status
    bool ws_live = trading::ExchangeService::instance().is_ws_connected();
    bool has_data = (current_ticker_.last > 0);
    ImGui::TextColored(ws_live ? MARKET_GREEN : (has_data ? WARNING : MARKET_RED), "[*]");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(TEXT_DIM, "%s", ws_live ? "WS LIVE" : (has_data ? "REST" : "NO DATA"));

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    // Exchange
    ImGui::TextColored(TEXT_DIM, "Exchange:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(TEXT_SECONDARY, "%s", exchange_id_.c_str());

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    // Mode
    bool is_paper = (trading_mode_ == TradingMode::Paper);
    ImGui::TextColored(TEXT_DIM, "Mode:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(is_paper ? WARNING : MARKET_RED, "%s", is_paper ? "PAPER" : "LIVE");

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    // Stats
    ImGui::TextColored(TEXT_DIM, "OK:%d ERR:%d", success_count_, error_count_);

    // Async status
    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    if (ticker_fetching_ || ob_fetching_ || wl_fetching_) {
        ImGui::TextColored(WARNING, "FETCHING");
        if (ticker_fetching_) { ImGui::SameLine(0, 4); ImGui::TextColored(TEXT_DIM, "[T]"); }
        if (ob_fetching_) { ImGui::SameLine(0, 4); ImGui::TextColored(TEXT_DIM, "[OB]"); }
        if (wl_fetching_) { ImGui::SameLine(0, 4); ImGui::TextColored(TEXT_DIM, "[WL]"); }
    } else {
        ImGui::TextColored(TEXT_DIM, "IDLE");
    }

    // Last error (if any)
    if (!last_error_.empty()) {
        ImGui::SameLine(0, 12);
        ImGui::TextColored(BORDER_BRIGHT, "|");
        ImGui::SameLine(0, 12);
        // Truncate error for status bar
        std::string err_short = last_error_.substr(0, 60);
        ImGui::TextColored(MARKET_RED, "%s", err_short.c_str());
    }

    // Right side: last update time
    if (last_data_update_ > 0) {
        time_t now = time(nullptr);
        int elapsed = (int)difftime(now, last_data_update_);
        char time_buf[32];
        std::snprintf(time_buf, sizeof(time_buf), "Updated %ds ago", elapsed);
        float time_w = ImGui::CalcTextSize(time_buf).x + 8;
        float avail = ImGui::GetContentRegionAvail().x;
        if (avail > time_w) {
            ImGui::SameLine(ImGui::GetCursorPosX() + avail - time_w);
            ImGui::TextColored(TEXT_DIM, "%s", time_buf);
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Order submission
// ============================================================================
void CryptoTradingScreen::submit_order() {
    order_form_.error.clear();
    order_form_.success.clear();

    LOG_INFO(TAG, "Submitting order: symbol=%s side=%d type=%d qty='%s' price='%s'",
             selected_symbol_.c_str(), order_form_.side_idx, order_form_.type_idx,
             order_form_.quantity_buf, order_form_.price_buf);

    // Parse quantity
    double qty = 0;
    try {
        qty = std::stod(order_form_.quantity_buf);
    } catch (...) {
        order_form_.error = "Invalid quantity";
        order_form_.msg_timer = 5.0f;
        LOG_ERROR(TAG, "Invalid quantity: '%s'", order_form_.quantity_buf);
        return;
    }
    if (qty <= 0) {
        order_form_.error = "Quantity must be > 0";
        order_form_.msg_timer = 5.0f;
        return;
    }

    static const char* type_strs[] = {"market", "limit", "stop", "stop_limit"};
    std::string order_type = type_strs[order_form_.type_idx];
    std::string side = order_form_.side_idx == 0 ? "buy" : "sell";

    std::optional<double> price;
    std::optional<double> stop_price;

    // For market orders, use current ticker price as reference for margin calculation
    if (order_form_.type_idx == 0) {
        double ref = (side == "buy") ? current_ticker_.ask : current_ticker_.bid;
        if (ref <= 0) ref = current_ticker_.last;
        if (ref <= 0) {
            order_form_.error = "No market price available yet";
            order_form_.msg_timer = 5.0f;
            LOG_ERROR(TAG, "Market order rejected: no price data");
            return;
        }
        price = ref;
    }

    if (order_form_.type_idx == 1 || order_form_.type_idx == 3) {
        try {
            double p = std::stod(order_form_.price_buf);
            if (p <= 0) {
                order_form_.error = "Price must be > 0";
                order_form_.msg_timer = 5.0f;
                return;
            }
            price = p;
        } catch (...) {
            order_form_.error = "Invalid price";
            order_form_.msg_timer = 5.0f;
            return;
        }
    }

    if (order_form_.type_idx >= 2) {
        try {
            double sp = std::stod(order_form_.stop_price_buf);
            if (sp <= 0) {
                order_form_.error = "Stop price must be > 0";
                order_form_.msg_timer = 5.0f;
                return;
            }
            stop_price = sp;
        } catch (...) {
            order_form_.error = "Invalid stop price";
            order_form_.msg_timer = 5.0f;
            return;
        }
    }

    try {
        if (trading_mode_ == TradingMode::Live) {
            // Live trading — route through exchange API
            auto& svc = trading::ExchangeService::instance();
            auto result = svc.place_order(selected_symbol_, side, order_type, qty,
                                           price.value_or(0.0));
            LOG_INFO(TAG, "Live order placed: %s", result.dump(2).c_str());
            order_form_.success = "Live order sent!";
            order_form_.msg_timer = 3.0f;
        } else {
            // Paper trading
            auto order = trading::pt_place_order(
                portfolio_id_, selected_symbol_, side, order_type,
                qty, price, stop_price, order_form_.reduce_only);

            LOG_INFO(TAG, "Order created: id=%s type=%s side=%s qty=%.6f",
                     order.id.c_str(), order_type.c_str(), side.c_str(), qty);

            // For market orders, immediately fill at current price
            if (order_type == "market" && current_ticker_.last > 0) {
                double fill_price = side == "buy" ? current_ticker_.ask : current_ticker_.bid;
                if (fill_price <= 0) fill_price = current_ticker_.last;
                LOG_INFO(TAG, "Market order fill at %.2f", fill_price);
                trading::pt_fill_order(order.id, fill_price);
            } else {
                trading::OrderMatcher::instance().add_order(order);
                LOG_INFO(TAG, "Order added to matcher");
            }

            order_form_.success = "Order placed: " + order.id.substr(0, 8);
            order_form_.msg_timer = 3.0f;
        }

        // Ensure the traded symbol is watched for price updates
        trading::ExchangeService::instance().watch_symbol(selected_symbol_, portfolio_id_);

        // Clear form
        order_form_.quantity_buf[0] = '\0';
        order_form_.price_buf[0] = '\0';
        order_form_.stop_price_buf[0] = '\0';

        // Refresh portfolio data immediately
        refresh_portfolio_data();
    } catch (const std::exception& e) {
        order_form_.error = e.what();
        order_form_.msg_timer = 5.0f;
        LOG_ERROR(TAG, "Order FAILED: %s", e.what());
    }
}

void CryptoTradingScreen::cancel_order(const std::string& order_id) {
    LOG_INFO(TAG, "Cancelling order: %s", order_id.c_str());
    try {
        trading::pt_cancel_order(order_id);
        trading::OrderMatcher::instance().remove_order(order_id);
        refresh_portfolio_data();
        LOG_INFO(TAG, "Order cancelled: %s", order_id.c_str());
    } catch (const std::exception& e) {
        LOG_ERROR(TAG, "Cancel failed: %s", e.what());
    }
}

} // namespace fincept::crypto
