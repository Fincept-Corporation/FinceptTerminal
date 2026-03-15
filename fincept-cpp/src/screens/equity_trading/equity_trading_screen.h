#pragma once
// Equity Trading Screen — Bloomberg Terminal-style stock trading interface.
// Multi-panel layout: watchlist | chart + bottom panel | order entry + depth

#include "equity_trading_types.h"
#include "trading/broker_registry.h"
#include "trading/broker_interface.h"
#include <string>
#include <vector>
#include <future>
#include <optional>

namespace fincept::equity_trading {

class EquityTradingScreen {
public:
    void render();

private:
    void init();

    // Panel renderers
    void render_top_nav(float w);
    void render_ticker_bar(float w);
    void render_watchlist_panel(float x, float y, float w, float h);
    void render_chart_panel(float x, float y, float w, float h);
    void render_bottom_panel(float x, float y, float w, float h);
    void render_order_entry(float x, float y, float w, float h);
    void render_order_book(float x, float y, float w, float h);
    void render_status_bar(float w, float total_h);

    // Sub-renderers
    void render_positions_table(float w, float h);
    void render_holdings_table(float w, float h);
    void render_orders_table(float w, float h);
    void render_broker_config(float w, float h);
    void render_candle_chart(float w, float h);

    // Actions
    void select_symbol(const std::string& symbol, const std::string& exchange);
    void fetch_quote();
    void fetch_watchlist_quotes();
    void place_order();
    void connect_broker(trading::BrokerId broker_id);
    void refresh_portfolio();

    // State
    bool initialized_ = false;

    // Broker connection
    std::optional<trading::BrokerId> active_broker_;
    trading::BrokerCredentials active_creds_;
    bool is_connected_ = false;
    bool is_paper_mode_ = true;

    // Selected symbol
    char symbol_buf_[64] = "RELIANCE";
    std::string selected_symbol_ = "RELIANCE";
    std::string selected_exchange_ = "NSE";
    int exchange_idx_ = 0;

    // Quote data
    trading::BrokerQuote current_quote_;
    bool has_quote_ = false;
    bool loading_quote_ = false;
    std::future<trading::BrokerQuote> quote_future_;

    // Watchlist
    std::vector<WatchlistEntry> watchlist_;
    bool loading_watchlist_ = false;

    // Candle data
    std::vector<trading::BrokerCandle> candles_;
    TimeFrame chart_timeframe_ = TimeFrame::d1;
    bool loading_candles_ = false;
    std::future<std::vector<trading::BrokerCandle>> candle_future_;

    // Portfolio data
    std::vector<trading::BrokerPosition> positions_;
    std::vector<trading::BrokerHolding> holdings_;
    std::vector<trading::BrokerOrderInfo> orders_;
    std::optional<trading::BrokerFunds> funds_;
    bool loading_portfolio_ = false;

    // Order form
    OrderFormState order_form_;

    // UI state
    BottomTab bottom_tab_ = BottomTab::positions;
    SidebarView sidebar_view_ = SidebarView::watchlist;
    bool bottom_minimized_ = false;
    char watchlist_search_[64] = {};
};

} // namespace fincept::equity_trading
