#pragma once
#include "equity_trading_types.h"
#include "trading/broker_registry.h"
#include "trading/broker_interface.h"
#include "trading/paper_trading.h"
#include "trading/equity_stream.h"
#include <string>
#include <vector>
#include <future>
#include <optional>
#include <array>
#include <chrono>
#include <nlohmann/json.hpp>

namespace fincept::equity_trading {

class EquityTradingScreen {
public:
    void render();

private:
    // Section renderers
    void section_overview();
    void section_watchlist();
    void section_order();
    void section_positions();
    void section_orders();
    void section_funds();
    void section_brokers();

    // Broker connect result — produced by background thread, consumed on main thread
    struct ConnectResult {
        bool success = false;
        trading::BrokerId broker_id{};
        trading::BrokerCredentials creds;
        std::string error;
    };

    // Actions
    void select_symbol(const std::string& symbol, const std::string& exchange);
    void fetch_quote();
    void fetch_candles();
    void place_order();
    void connect_broker(trading::BrokerId broker_id);
    void finish_connect(ConnectResult r);  // called on main thread after future resolves
    void refresh_portfolio();
    void refresh_paper_portfolio();
    void cancel_order_async(const std::string& order_id);
    void modify_order_async(const std::string& order_id, double price, double qty, double trigger_price);
    void exchange_token_async(int broker_idx);
    std::string generate_oauth_url(trading::BrokerId id, const char* api_key);
    static std::string today_ist_string();

    void set_status(std::string msg) {
        status_msg_ = std::move(msg);
        status_time_ = std::chrono::steady_clock::now();
    }

    // State
    bool inited_ = false;
    int  section_ = 0;  // active nav section

    // Broker
    std::optional<trading::BrokerId> active_broker_;
    trading::BrokerCredentials active_creds_;
    bool connected_  = false;
    bool paper_mode_ = true;
    int  broker_sel_ = -1;
    struct CredUI {
        char api_key[128] = {}, api_secret[128] = {}, user_id[64] = {};
        char password[64] = {}, totp_secret[64] = {}, auth_code[256] = {};
        bool show_secret = false, show_pw = false;
        bool token_valid = false, checking = false;
        std::string oauth_url, err, ok;
    };
    std::array<CredUI, BROKER_META_COUNT> cred_ui_;

    // Paper trading
    std::string paper_id_;

    // Symbol
    char sym_buf_[64] = "RELIANCE";
    std::string sel_sym_ = "RELIANCE";
    std::string sel_exch_ = "NSE";
    int exch_idx_ = 0;

    // Quote
    trading::BrokerQuote quote_;
    bool has_quote_ = false, loading_quote_ = false;
    std::future<trading::BrokerQuote> quote_fut_;

    // Watchlist
    std::vector<WatchlistEntry> watchlist_;

    // Candles
    std::vector<trading::BrokerCandle> candles_;
    TimeFrame timeframe_ = TimeFrame::d1;
    bool loading_candles_ = false;
    std::future<std::vector<trading::BrokerCandle>> candle_fut_;

    // Portfolio
    std::vector<trading::BrokerPosition>  positions_;
    std::vector<trading::BrokerOrderInfo> orders_;
    std::optional<trading::BrokerFunds>   funds_;
    bool loading_port_ = false;

    // Order form
    OrderFormState order_form_;

    // Modify order modal state
    struct ModifyState {
        bool open = false;
        std::string order_id;
        double price = 0.0, qty = 0.0, trigger_price = 0.0;
    } modify_state_;

    // WebSocket streaming state
    bool   ws_streaming_  = false;   // true when WS bridge is running
    int    ws_tick_cb_id_ = -1;      // callback ID registered with EquityStreamManager
    int    ws_status_cb_id_ = -1;

    // Start / stop the WS bridge for the current broker + watchlist
    void start_ws_stream();
    void stop_ws_stream();

    // Bulk-fetch LTP for all watchlist entries via REST
    void refresh_watchlist_quotes();
    bool loading_wl_quotes_ = false;
    std::chrono::steady_clock::time_point last_wl_refresh_;
    static constexpr int WL_REFRESH_SECS = 15;

    // Pending broker connect future — polled on main thread
    std::future<ConnectResult> connect_fut_;
    bool connecting_     = false;
    int  connecting_idx_ = -1;

    // Status
    std::string status_msg_;
    std::chrono::steady_clock::time_point status_time_;
    std::chrono::steady_clock::time_point last_port_refresh_;
    std::chrono::steady_clock::time_point last_quote_refresh_;
    static constexpr int QUOTE_REFRESH_SECS = 5;
    static constexpr int PORT_REFRESH_SECS  = 30;
};

} // namespace fincept::equity_trading
