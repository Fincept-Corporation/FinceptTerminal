#pragma once
// ExchangeService — Bridge between Python ccxt scripts and C++ paper trading
// Calls exchange/*.py scripts via python_runner, parses JSON, feeds OrderMatcher.
//
// Usage:
//   auto& svc = ExchangeService::instance();
//   svc.set_exchange("binance");
//   svc.watch_symbol("BTC/USDT", "portfolio-123");
//   svc.start_price_feed(5);  // poll every 5 seconds
//   svc.stop_price_feed();

#include "portfolio/portfolio_types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <nlohmann/json.hpp>

namespace fincept::trading {

using json = nlohmann::json;

// Ticker data returned from fetch_ticker/fetch_tickers
struct TickerData {
    std::string symbol;
    double last = 0.0;
    double bid = 0.0;
    double ask = 0.0;
    double high = 0.0;
    double low = 0.0;
    double open = 0.0;
    double close = 0.0;
    double change = 0.0;
    double percentage = 0.0;
    double base_volume = 0.0;
    double quote_volume = 0.0;
    int64_t timestamp = 0;
};

// OHLCV candle
struct Candle {
    int64_t timestamp = 0;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    double volume = 0.0;
};

// Order book entry
struct OrderBookData {
    std::string symbol;
    std::vector<std::pair<double, double>> bids; // price, amount
    std::vector<std::pair<double, double>> asks;
    double best_bid = 0.0;
    double best_ask = 0.0;
    double spread = 0.0;
    double spread_pct = 0.0;
};

// Market info
struct MarketInfo {
    std::string symbol;
    std::string base;
    std::string quote;
    std::string type; // "spot", "swap", "future"
    bool active = true;
    double maker_fee = 0.0;
    double taker_fee = 0.0;
    double min_amount = 0.0;
    double min_cost = 0.0;
};

// Exchange info
struct ExchangeInfo {
    std::string id;
    std::string name;
    bool has_fetch_ticker = false;
    bool has_fetch_order_book = false;
    bool has_fetch_ohlcv = false;
    bool has_create_order = false;
    bool has_fetch_balance = false;
    bool has_fetch_positions = false;
    bool has_set_leverage = false;
};

// Public trade
struct TradeData {
    std::string id;
    std::string symbol;
    std::string side;  // "buy" | "sell"
    double price = 0.0;
    double amount = 0.0;
    double cost = 0.0;
    int64_t timestamp = 0;
};

// Funding rate info
struct FundingRateData {
    std::string symbol;
    double funding_rate = 0.0;
    double mark_price = 0.0;
    double index_price = 0.0;
    int64_t funding_timestamp = 0;
    int64_t next_funding_timestamp = 0;
};

// Open interest
struct OpenInterestData {
    std::string symbol;
    double open_interest = 0.0;
    double open_interest_value = 0.0;
    int64_t timestamp = 0;
};

// Credential set for authenticated operations
struct ExchangeCredentials {
    std::string api_key;
    std::string secret;
    std::string password; // for some exchanges (e.g., OKX)
};

// Callback for price updates
using PriceUpdateCallback = std::function<void(const std::string& symbol, const TickerData& ticker)>;

// Callback for orderbook updates
using OrderBookCallback = std::function<void(const std::string& symbol, const OrderBookData& ob)>;

// Callback for candle updates
using CandleCallback = std::function<void(const std::string& symbol, const Candle& candle)>;

// Callback for trade updates (Time & Sales)
using TradeCallback = std::function<void(const std::string& symbol, const TradeData& trade)>;

class ExchangeService {
public:
    static ExchangeService& instance();

    // --- Configuration ---
    void set_exchange(const std::string& exchange_id);
    std::string get_exchange() const;
    void set_credentials(const ExchangeCredentials& creds);

    // --- Price feed (polls ccxt, feeds OrderMatcher) ---
    void watch_symbol(const std::string& symbol, const std::string& portfolio_id);
    void unwatch_symbol(const std::string& symbol, const std::string& portfolio_id);
    void start_price_feed(int interval_seconds = 5);
    void stop_price_feed();
    bool is_feed_running() const;

    // Register callbacks for real-time updates (UI refresh)
    int on_price_update(PriceUpdateCallback callback);
    void remove_price_callback(int id);
    int on_orderbook_update(OrderBookCallback callback);
    void remove_orderbook_callback(int id);
    int on_candle_update(CandleCallback callback);
    void remove_candle_callback(int id);
    int on_trade_update(TradeCallback callback);
    void remove_trade_callback(int id);

    // --- WebSocket streaming (real-time via ws_stream.py subprocess) ---
    void start_ws_stream(const std::string& primary_symbol,
                         const std::vector<std::string>& all_symbols);
    void stop_ws_stream();
    bool is_ws_connected() const;

    // Change which symbol is "primary" for WS routing — does NOT restart subprocess.
    // Callbacks on CryptoTradingScreen read selected_symbol_ dynamically so this
    // takes effect immediately on the next incoming WS message.
    void set_ws_primary_symbol(const std::string& symbol);
    std::string get_ws_primary_symbol() const;

    // --- One-shot data fetches (called from UI) ---
    TickerData fetch_ticker(const std::string& symbol);
    std::vector<TickerData> fetch_tickers(const std::vector<std::string>& symbols);
    OrderBookData fetch_orderbook(const std::string& symbol, int limit = 20);
    std::vector<Candle> fetch_ohlcv(const std::string& symbol,
                                     const std::string& timeframe = "1h",
                                     int limit = 100);
    std::vector<MarketInfo> fetch_markets(const std::string& type = "");
    std::vector<ExchangeInfo> list_exchanges();
    std::vector<std::string> list_exchange_ids(); // fast — no instantiation
    std::vector<TradeData> fetch_trades(const std::string& symbol, int limit = 50);
    FundingRateData fetch_funding_rate(const std::string& symbol);
    OpenInterestData fetch_open_interest(const std::string& symbol);

    // --- Authenticated operations ---
    json fetch_balance();
    json place_order(const std::string& symbol, const std::string& side,
                     const std::string& type, double amount,
                     double price = 0.0);
    json cancel_order(const std::string& order_id, const std::string& symbol);
    json set_leverage(const std::string& symbol, int leverage);
    json set_margin_mode(const std::string& symbol, const std::string& mode);
    json fetch_positions_live(const std::string& symbol = "");
    json fetch_open_orders_live(const std::string& symbol = "");
    json fetch_my_trades_live(const std::string& symbol, int limit = 50);
    json fetch_trading_fees(const std::string& symbol = "");

    // --- Cache ---
    const std::unordered_map<std::string, TickerData>& get_price_cache() const;
    TickerData get_cached_price(const std::string& symbol) const;

    ExchangeService(const ExchangeService&) = delete;
    ExchangeService& operator=(const ExchangeService&) = delete;

private:
    ExchangeService() = default;
    ~ExchangeService();

    // Run one cycle of the price feed
    void price_feed_loop();
    void poll_prices();

    // Parse ccxt JSON responses
    static TickerData parse_ticker(const json& j);
    static OrderBookData parse_orderbook(const json& j);
    static Candle parse_candle(const json& j);
    static MarketInfo parse_market(const json& j);

    // Call a Python exchange script and get parsed JSON
    json call_script(const std::string& script, const std::vector<std::string>& args);
    json call_script_with_credentials(const std::string& script,
                                       const std::vector<std::string>& args);

    std::string exchange_id_ = "binance";
    ExchangeCredentials credentials_;

    // Watched symbols: symbol → set of portfolio IDs
    std::unordered_map<std::string, std::unordered_set<std::string>> watched_;

    // Price cache: symbol → latest ticker
    std::unordered_map<std::string, TickerData> price_cache_;

    // Callbacks
    std::unordered_map<int, PriceUpdateCallback> price_callbacks_;
    std::unordered_map<int, OrderBookCallback> orderbook_callbacks_;
    std::unordered_map<int, CandleCallback> candle_callbacks_;
    std::unordered_map<int, TradeCallback> trade_callbacks_;
    int next_callback_id_ = 1;

    // Feed thread (polling fallback)
    std::thread feed_thread_;
    std::atomic<bool> feed_running_{false};
    int feed_interval_ = 5;

    // WebSocket bridge (ws_stream.py subprocess)
    void ws_reader_loop();
    void handle_ws_message(const json& msg);

#ifdef _WIN32
    void* ws_process_ = nullptr;   // HANDLE
    void* ws_stdout_rd_ = nullptr; // HANDLE
#else
    int ws_pid_ = -1;
    int ws_stdout_fd_ = -1;
#endif
    std::thread ws_thread_;
    std::atomic<bool> ws_running_{false};
    std::atomic<bool> ws_connected_{false};

    // WS reconnection state
    std::atomic<int> ws_reconnect_attempts_{0};
    static constexpr int WS_MAX_RECONNECTS = 3;
    std::string ws_primary_symbol_;
    std::vector<std::string> ws_all_symbols_;
    void ws_spawn_subprocess();  // internal helper to (re)spawn WS subprocess

    mutable std::mutex mutex_;
};

} // namespace fincept::trading
