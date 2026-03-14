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

    // --- WebSocket streaming (real-time via ws_stream.py subprocess) ---
    void start_ws_stream(const std::string& primary_symbol,
                         const std::vector<std::string>& all_symbols);
    void stop_ws_stream();
    bool is_ws_connected() const;

    // --- One-shot data fetches (called from UI) ---
    TickerData fetch_ticker(const std::string& symbol);
    std::vector<TickerData> fetch_tickers(const std::vector<std::string>& symbols);
    OrderBookData fetch_orderbook(const std::string& symbol, int limit = 20);
    std::vector<Candle> fetch_ohlcv(const std::string& symbol,
                                     const std::string& timeframe = "1h",
                                     int limit = 100);
    std::vector<MarketInfo> fetch_markets(const std::string& type = "");
    std::vector<ExchangeInfo> list_exchanges();

    // --- Authenticated operations ---
    json fetch_balance();
    json place_order(const std::string& symbol, const std::string& side,
                     const std::string& type, double amount,
                     double price = 0.0);
    json cancel_order(const std::string& order_id, const std::string& symbol);

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

    mutable std::mutex mutex_;
};

} // namespace fincept::trading
