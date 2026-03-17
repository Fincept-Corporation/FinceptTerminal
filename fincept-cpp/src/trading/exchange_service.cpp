// ExchangeService — Bridge between Python ccxt and C++ paper trading engine
// Calls exchange/*.py via python_runner, feeds prices to OrderMatcher

#include "exchange_service.h"
#include "order_matcher.h"
#include "paper_trading.h"
#include "python/python_runner.h"
#include "core/logger.h"
#include "core/event_bus.h"
#include "core/notification.h"
#include <chrono>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#endif

namespace fincept::trading {

static const char* TAG = "ExchangeService";

// Null-safe JSON accessors (used throughout; must appear before first use)
using json = nlohmann::json;
static double jdouble(const json& j, const char* key, double def = 0.0) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) return def;
    return it->get<double>();
}
static int64_t jint64(const json& j, const char* key, int64_t def = 0) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) return def;
    return it->get<int64_t>();
}
static std::string jstr(const json& j, const char* key, const std::string& def = "") {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) return def;
    return it->get<std::string>();
}

// ============================================================================
// Singleton
// ============================================================================

ExchangeService& ExchangeService::instance() {
    static ExchangeService s;
    return s;
}

ExchangeService::~ExchangeService() {
    stop_ws_stream();
    stop_price_feed();
}

// ============================================================================
// Configuration
// ============================================================================

void ExchangeService::set_exchange(const std::string& exchange_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    exchange_id_ = exchange_id;
    LOG_INFO(TAG, "Exchange set to: %s", exchange_id.c_str());
}

std::string ExchangeService::get_exchange() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return exchange_id_;
}

void ExchangeService::set_credentials(const ExchangeCredentials& creds) {
    std::lock_guard<std::mutex> lock(mutex_);
    credentials_ = creds;
    LOG_INFO(TAG, "Credentials updated for %s", exchange_id_.c_str());
}

// ============================================================================
// Watch management
// ============================================================================

void ExchangeService::watch_symbol(const std::string& symbol, const std::string& portfolio_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    watched_[symbol].insert(portfolio_id);
    LOG_INFO(TAG, "Watching %s for portfolio %s", symbol.c_str(), portfolio_id.c_str());
}

void ExchangeService::unwatch_symbol(const std::string& symbol, const std::string& portfolio_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = watched_.find(symbol);
    if (it != watched_.end()) {
        it->second.erase(portfolio_id);
        if (it->second.empty()) {
            watched_.erase(it);
        }
    }
}

// ============================================================================
// Price feed
// ============================================================================

void ExchangeService::start_price_feed(int interval_seconds) {
    if (feed_running_) return;

    feed_interval_ = interval_seconds;
    feed_running_ = true;

    feed_thread_ = std::thread([this]() {
        LOG_INFO(TAG, "Price feed started (interval: %ds)", feed_interval_);
        price_feed_loop();
    });
}

void ExchangeService::stop_price_feed() {
    feed_running_ = false;
    if (feed_thread_.joinable()) {
        feed_thread_.join();
    }
    LOG_INFO(TAG, "Price feed stopped");
}

bool ExchangeService::is_feed_running() const {
    return feed_running_;
}

int ExchangeService::on_price_update(PriceUpdateCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    int id = next_callback_id_++;
    price_callbacks_[id] = std::move(callback);
    return id;
}

void ExchangeService::remove_price_callback(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    price_callbacks_.erase(id);
}

void ExchangeService::price_feed_loop() {
    while (feed_running_) {
        poll_prices();

        // Sleep in small increments so we can stop quickly
        for (int i = 0; i < feed_interval_ * 10 && feed_running_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void ExchangeService::poll_prices() {
    // Collect symbols to fetch
    std::vector<std::string> symbols;
    std::string exchange;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (watched_.empty()) return;
        for (const auto& [sym, _] : watched_) {
            symbols.push_back(sym);
        }
        exchange = exchange_id_;
    }

    if (symbols.empty()) return;

    // Batch fetch all tickers in one Python call
    std::vector<std::string> args = {exchange};
    args.insert(args.end(), symbols.begin(), symbols.end());

    auto result = python::execute("exchange/fetch_tickers.py", args);
    if (!result.success) {
        LOG_ERROR(TAG, "Price fetch failed: %s", result.error.c_str());
        return;
    }

    // Parse response
    json response;
    try {
        response = json::parse(result.output);
    } catch (const json::parse_error& e) {
        LOG_ERROR(TAG, "JSON parse error: %s", e.what());
        return;
    }

    if (!response.value("success", false)) {
        LOG_ERROR(TAG, "Script error: %s",
                  response.value("error", "unknown").c_str());
        return;
    }

    auto& data = response["data"];
    auto& tickers_arr = data["tickers"];

    // Process each ticker
    std::vector<std::pair<std::string, TickerData>> updates;

    for (const auto& t : tickers_arr) {
        TickerData ticker = parse_ticker(t);
        if (ticker.symbol.empty()) continue;

        // Update cache
        {
            std::lock_guard<std::mutex> lock(mutex_);
            price_cache_[ticker.symbol] = ticker;
        }

        updates.emplace_back(ticker.symbol, ticker);
    }

    // Feed prices to OrderMatcher and update positions
    for (const auto& [symbol, ticker] : updates) {
        PriceData pd;
        pd.last = ticker.last;
        pd.bid = ticker.bid;
        pd.ask = ticker.ask;
        pd.high = ticker.high;
        pd.low = ticker.low;
        pd.volume = ticker.base_volume;
        pd.change = ticker.change;
        pd.change_percent = ticker.percentage;
        pd.timestamp = ticker.timestamp;

        // Get portfolio IDs watching this symbol
        std::unordered_set<std::string> portfolios;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = watched_.find(symbol);
            if (it != watched_.end()) {
                portfolios = it->second;
            }
        }

        // Check pending orders against this price
        for (const auto& portfolio_id : portfolios) {
            OrderMatcher::instance().check_orders(symbol, pd, portfolio_id);

            // Update position mark prices
            try {
                pt_update_position_price(portfolio_id, symbol, ticker.last);
            } catch (const std::exception& e) {
                // Position may not exist — that's fine
            }
        }

        // Notify price callbacks (for UI updates)
        std::vector<PriceUpdateCallback> callbacks;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& [_, cb] : price_callbacks_) {
                callbacks.push_back(cb);
            }
        }
        for (const auto& cb : callbacks) {
            try {
                cb(symbol, ticker);
            } catch (const std::exception& e) {
                LOG_ERROR(TAG, "Price callback error: %s", e.what());
            }
        }

        // Publish to EventBus
        core::EventBus::instance().publish("exchange.price_update", {
            {"symbol", symbol},
            {"last", ticker.last},
            {"bid", ticker.bid},
            {"ask", ticker.ask},
            {"change", ticker.change},
            {"percentage", ticker.percentage},
            {"volume", ticker.base_volume},
            {"exchange", exchange},
        });
    }

    LOG_DEBUG(TAG, "Updated %zu tickers from %s",
              updates.size(), exchange.c_str());
}

// ============================================================================
// One-shot data fetches
// ============================================================================

TickerData ExchangeService::fetch_ticker(const std::string& symbol) {
    std::string exchange = get_exchange();
    auto j = call_script("exchange/fetch_ticker.py", {exchange, symbol});
    return parse_ticker(j["data"]);
}

std::vector<TickerData> ExchangeService::fetch_tickers(const std::vector<std::string>& symbols) {
    std::string exchange = get_exchange();
    std::vector<std::string> args = {exchange};
    args.insert(args.end(), symbols.begin(), symbols.end());

    auto j = call_script("exchange/fetch_tickers.py", args);

    std::vector<TickerData> result;
    for (const auto& t : j["data"]["tickers"]) {
        result.push_back(parse_ticker(t));
    }
    return result;
}

OrderBookData ExchangeService::fetch_orderbook(const std::string& symbol, int limit) {
    std::string exchange = get_exchange();
    auto j = call_script("exchange/fetch_orderbook.py",
                          {exchange, symbol, std::to_string(limit)});
    return parse_orderbook(j["data"]);
}

std::vector<Candle> ExchangeService::fetch_ohlcv(const std::string& symbol,
                                                   const std::string& timeframe,
                                                   int limit) {
    std::string exchange = get_exchange();
    auto j = call_script("exchange/fetch_ohlcv.py",
                          {exchange, symbol, timeframe, std::to_string(limit)});

    std::vector<Candle> result;
    for (const auto& c : j["data"]["candles"]) {
        result.push_back(parse_candle(c));
    }
    return result;
}

std::vector<MarketInfo> ExchangeService::fetch_markets(const std::string& type) {
    std::string exchange = get_exchange();
    std::vector<std::string> args = {exchange};
    if (!type.empty()) args.push_back(type);

    auto j = call_script("exchange/fetch_markets.py", args);

    std::vector<MarketInfo> result;
    for (const auto& m : j["data"]["markets"]) {
        result.push_back(parse_market(m));
    }
    return result;
}

std::vector<ExchangeInfo> ExchangeService::list_exchanges() {
    auto j = call_script("exchange/list_exchanges.py", {});

    std::vector<ExchangeInfo> result;
    for (const auto& e : j["data"]["exchanges"]) {
        ExchangeInfo info;
        info.id = e.value("id", "");
        info.name = e.value("name", "");
        info.has_fetch_ticker = e.value("has_fetch_ticker", false);
        info.has_fetch_order_book = e.value("has_fetch_order_book", false);
        info.has_fetch_ohlcv = e.value("has_fetch_ohlcv", false);
        info.has_create_order = e.value("has_create_order", false);
        info.has_fetch_balance = e.value("has_fetch_balance", false);
        info.has_fetch_positions = e.value("has_fetch_positions", false);
        info.has_set_leverage = e.value("has_set_leverage", false);
        result.push_back(info);
    }
    return result;
}

std::vector<std::string> ExchangeService::list_exchange_ids() {
    auto j = call_script("exchange/list_exchange_ids.py", {});
    std::vector<std::string> result;
    for (const auto& id : j["data"]["ids"]) {
        if (id.is_string()) result.push_back(id.get<std::string>());
    }
    return result;
}

// ============================================================================
// Authenticated operations
// ============================================================================

json ExchangeService::fetch_balance() {
    std::string exchange = get_exchange();
    return call_script_with_credentials("exchange/fetch_balance.py", {exchange});
}

json ExchangeService::place_order(const std::string& symbol, const std::string& side,
                                    const std::string& type, double amount, double price) {
    std::string exchange = get_exchange();
    std::vector<std::string> args = {exchange, symbol, side, type, std::to_string(amount)};
    if (price > 0.0) {
        args.push_back(std::to_string(price));
    }
    return call_script_with_credentials("exchange/place_order.py", args);
}

json ExchangeService::cancel_order(const std::string& order_id, const std::string& symbol) {
    std::string exchange = get_exchange();
    return call_script_with_credentials("exchange/cancel_order.py",
                                         {exchange, order_id, symbol});
}

// ============================================================================
// New data fetches — trades, funding rate, open interest, etc.
// ============================================================================

std::vector<TradeData> ExchangeService::fetch_trades(const std::string& symbol, int limit) {
    std::string exchange = get_exchange();
    auto j = call_script("exchange/fetch_trades.py",
                          {exchange, symbol, std::to_string(limit)});
    std::vector<TradeData> result;
    for (const auto& t : j["data"]["trades"]) {
        TradeData td;
        td.id = jstr(t, "id");
        td.symbol = symbol;
        td.side = jstr(t, "side");
        td.price = jdouble(t, "price");
        td.amount = jdouble(t, "amount");
        td.cost = jdouble(t, "cost");
        td.timestamp = jint64(t, "timestamp");
        result.push_back(td);
    }
    return result;
}

FundingRateData ExchangeService::fetch_funding_rate(const std::string& symbol) {
    std::string exchange = get_exchange();
    FundingRateData fr;
    try {
        auto j = call_script("exchange/fetch_funding_rate.py", {exchange, symbol});
        auto& d = j["data"];
        fr.symbol = symbol;
        fr.funding_rate = jdouble(d, "funding_rate");
        fr.mark_price = jdouble(d, "mark_price");
        fr.index_price = jdouble(d, "index_price");
        fr.funding_timestamp = jint64(d, "funding_timestamp");
        fr.next_funding_timestamp = jint64(d, "next_funding_timestamp");
    } catch (const std::exception& e) {
        LOG_DEBUG(TAG, "Funding rate not available for %s: %s", symbol.c_str(), e.what());
    }
    return fr;
}

OpenInterestData ExchangeService::fetch_open_interest(const std::string& symbol) {
    std::string exchange = get_exchange();
    OpenInterestData oi;
    try {
        auto j = call_script("exchange/fetch_open_interest.py", {exchange, symbol});
        auto& d = j["data"];
        oi.symbol = symbol;
        oi.open_interest = jdouble(d, "open_interest");
        oi.open_interest_value = jdouble(d, "open_interest_value");
        oi.timestamp = jint64(d, "timestamp");
    } catch (const std::exception& e) {
        LOG_DEBUG(TAG, "Open interest not available for %s: %s", symbol.c_str(), e.what());
    }
    return oi;
}

json ExchangeService::set_leverage(const std::string& symbol, int leverage) {
    std::string exchange = get_exchange();
    return call_script_with_credentials("exchange/set_leverage.py",
                                         {exchange, symbol, std::to_string(leverage)});
}

json ExchangeService::set_margin_mode(const std::string& symbol, const std::string& mode) {
    std::string exchange = get_exchange();
    return call_script_with_credentials("exchange/set_margin_mode.py",
                                         {exchange, symbol, mode});
}

json ExchangeService::fetch_positions_live(const std::string& symbol) {
    std::string exchange = get_exchange();
    std::vector<std::string> args = {exchange};
    if (!symbol.empty()) args.push_back(symbol);
    return call_script_with_credentials("exchange/fetch_positions.py", args);
}

json ExchangeService::fetch_open_orders_live(const std::string& symbol) {
    std::string exchange = get_exchange();
    std::vector<std::string> args = {exchange};
    if (!symbol.empty()) args.push_back(symbol);
    return call_script_with_credentials("exchange/fetch_open_orders.py", args);
}

json ExchangeService::fetch_my_trades_live(const std::string& symbol, int limit) {
    std::string exchange = get_exchange();
    return call_script_with_credentials("exchange/fetch_my_trades.py",
                                         {exchange, symbol, std::to_string(limit)});
}

json ExchangeService::fetch_trading_fees(const std::string& symbol) {
    std::string exchange = get_exchange();
    std::vector<std::string> args = {exchange};
    if (!symbol.empty()) args.push_back(symbol);
    return call_script("exchange/fetch_trading_fees.py", args);
}

// ============================================================================
// Cache access
// ============================================================================

const std::unordered_map<std::string, TickerData>& ExchangeService::get_price_cache() const {
    return price_cache_;
}

TickerData ExchangeService::get_cached_price(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = price_cache_.find(symbol);
    if (it != price_cache_.end()) return it->second;
    return {};
}

// ============================================================================
// JSON parsing (helpers defined at top of file)
// ============================================================================

TickerData ExchangeService::parse_ticker(const json& j) {
    TickerData t;
    t.symbol = jstr(j, "symbol");
    t.last = jdouble(j, "last");
    t.bid = jdouble(j, "bid");
    t.ask = jdouble(j, "ask");
    t.high = jdouble(j, "high");
    t.low = jdouble(j, "low");
    t.open = jdouble(j, "open");
    t.close = jdouble(j, "close");
    t.change = jdouble(j, "change");
    t.percentage = jdouble(j, "percentage");
    t.base_volume = jdouble(j, "base_volume");
    t.quote_volume = jdouble(j, "quote_volume");
    t.timestamp = jint64(j, "timestamp");
    return t;
}

OrderBookData ExchangeService::parse_orderbook(const json& j) {
    OrderBookData ob;
    ob.symbol = jstr(j, "symbol");
    ob.best_bid = jdouble(j, "best_bid");
    ob.best_ask = jdouble(j, "best_ask");
    ob.spread = jdouble(j, "spread");
    ob.spread_pct = jdouble(j, "spread_pct");

    if (j.contains("bids")) {
        for (const auto& b : j["bids"]) {
            if (b.is_array() && b.size() >= 2) {
                ob.bids.emplace_back(b[0].get<double>(), b[1].get<double>());
            }
        }
    }
    if (j.contains("asks")) {
        for (const auto& a : j["asks"]) {
            if (a.is_array() && a.size() >= 2) {
                ob.asks.emplace_back(a[0].get<double>(), a[1].get<double>());
            }
        }
    }
    return ob;
}

Candle ExchangeService::parse_candle(const json& j) {
    Candle c;
    c.timestamp = j.value("timestamp", static_cast<int64_t>(0));
    c.open = j.value("open", 0.0);
    c.high = j.value("high", 0.0);
    c.low = j.value("low", 0.0);
    c.close = j.value("close", 0.0);
    c.volume = j.value("volume", 0.0);
    return c;
}

MarketInfo ExchangeService::parse_market(const json& j) {
    MarketInfo m;
    m.symbol = j.value("symbol", "");
    m.base = j.value("base", "");
    m.quote = j.value("quote", "");
    m.type = j.value("type", "spot");
    m.active = j.value("active", true);
    m.maker_fee = j.value("maker_fee", 0.0);
    m.taker_fee = j.value("taker_fee", 0.0);
    m.min_amount = j.value("min_amount", 0.0);
    m.min_cost = j.value("min_cost", 0.0);
    return m;
}

// ============================================================================
// Script execution helpers
// ============================================================================

json ExchangeService::call_script(const std::string& script,
                                    const std::vector<std::string>& args) {
    auto result = python::execute(script, args);
    if (!result.success) {
        throw std::runtime_error("Exchange script failed: " + result.error);
    }

    json response;
    try {
        response = json::parse(result.output);
    } catch (const json::parse_error& e) {
        throw std::runtime_error(std::string("JSON parse error: ") + e.what());
    }

    if (!response.value("success", false)) {
        std::string error = response.value("error", "Unknown exchange error");
        std::string code = response.value("code", "EXCHANGE_ERROR");
        throw std::runtime_error("[" + code + "] " + error);
    }

    return response;
}

json ExchangeService::call_script_with_credentials(const std::string& script,
                                                      const std::vector<std::string>& args) {
    json creds;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        creds = {
            {"api_key", credentials_.api_key},
            {"secret", credentials_.secret},
        };
        if (!credentials_.password.empty()) {
            creds["password"] = credentials_.password;
        }
    }

    auto result = python::execute_with_stdin(script, args, creds.dump());
    if (!result.success) {
        throw std::runtime_error("Exchange script failed: " + result.error);
    }

    json response;
    try {
        response = json::parse(result.output);
    } catch (const json::parse_error& e) {
        throw std::runtime_error(std::string("JSON parse error: ") + e.what());
    }

    if (!response.value("success", false)) {
        std::string error = response.value("error", "Unknown exchange error");
        std::string code = response.value("code", "EXCHANGE_ERROR");
        throw std::runtime_error("[" + code + "] " + error);
    }

    return response;
}

// ============================================================================
// Orderbook & Candle callback registration
// ============================================================================

int ExchangeService::on_orderbook_update(OrderBookCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    int id = next_callback_id_++;
    orderbook_callbacks_[id] = std::move(callback);
    return id;
}

void ExchangeService::remove_orderbook_callback(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    orderbook_callbacks_.erase(id);
}

int ExchangeService::on_candle_update(CandleCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    int id = next_callback_id_++;
    candle_callbacks_[id] = std::move(callback);
    return id;
}

void ExchangeService::remove_candle_callback(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    candle_callbacks_.erase(id);
}

int ExchangeService::on_trade_update(TradeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    int id = next_callback_id_++;
    trade_callbacks_[id] = std::move(callback);
    return id;
}

void ExchangeService::remove_trade_callback(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    trade_callbacks_.erase(id);
}

// ============================================================================
// WebSocket streaming — spawns ws_stream.py subprocess
// ============================================================================

void ExchangeService::start_ws_stream(const std::string& primary_symbol,
                                        const std::vector<std::string>& all_symbols) {
    if (ws_running_) {
        LOG_INFO(TAG, "WS stream already running, stopping first");
        stop_ws_stream();
    }

    // Save symbols for reconnection
    ws_primary_symbol_ = primary_symbol;
    ws_all_symbols_ = all_symbols;
    ws_reconnect_attempts_ = 0;

    ws_running_ = true;
    ws_connected_ = false;

    ws_spawn_subprocess();

    // Start reader thread
    ws_thread_ = std::thread([this]() {
        LOG_INFO(TAG, "WS reader thread started");
        ws_reader_loop();
        LOG_INFO(TAG, "WS reader thread exited");
    });
}

void ExchangeService::ws_spawn_subprocess() {
    // Resolve Python and script paths
    auto python = python::resolve_python_path("exchange/ws_stream.py");
    if (python.empty()) {
        LOG_ERROR(TAG, "Python not found for WS stream");
        return;
    }

    auto script = python::resolve_script_path("exchange/ws_stream.py");
    if (script.empty()) {
        LOG_ERROR(TAG, "ws_stream.py not found");
        return;
    }

    std::string exchange;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        exchange = exchange_id_;
    }

    LOG_INFO(TAG, "Starting WS stream: exchange=%s primary=%s symbols=%zu",
             exchange.c_str(), ws_primary_symbol_.c_str(), ws_all_symbols_.size());

#ifdef _WIN32
    std::string cmd = "\"" + python.string() + "\" -u -B \"" + script.string() + "\" " + exchange + " " + ws_primary_symbol_;
    for (const auto& s : ws_all_symbols_) {
        if (s != ws_primary_symbol_) {
            cmd += " " + s;
        }
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hStdoutRead = nullptr, hStdoutWrite = nullptr;
    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
        LOG_ERROR(TAG, "Failed to create WS stdout pipe");
        return;
    }
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStdoutWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi{};

    BOOL ok = CreateProcessA(
        nullptr, cmd.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi
    );

    CloseHandle(hStdoutWrite);

    if (!ok) {
        LOG_ERROR(TAG, "Failed to spawn ws_stream.py subprocess");
        CloseHandle(hStdoutRead);
        return;
    }

    ws_process_ = pi.hProcess;
    ws_stdout_rd_ = hStdoutRead;
    CloseHandle(pi.hThread);

    LOG_INFO(TAG, "WS subprocess spawned (PID handle=%p)", ws_process_);

#else
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        LOG_ERROR(TAG, "Failed to create WS pipe");
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        LOG_ERROR(TAG, "Failed to fork for WS stream");
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        std::vector<const char*> argv;
        std::string py_str = python.string();
        std::string sc_str = script.string();
        argv.push_back(py_str.c_str());
        argv.push_back("-u");
        argv.push_back("-B");
        argv.push_back(sc_str.c_str());
        argv.push_back(exchange.c_str());
        argv.push_back(ws_primary_symbol_.c_str());

        std::vector<std::string> extra;
        for (const auto& s : ws_all_symbols_) {
            if (s != ws_primary_symbol_) extra.push_back(s);
        }
        for (const auto& s : extra) {
            argv.push_back(s.c_str());
        }
        argv.push_back(nullptr);

        execvp(argv[0], const_cast<char* const*>(argv.data()));
        _exit(1);
    }

    close(pipefd[1]);
    ws_pid_ = pid;
    ws_stdout_fd_ = pipefd[0];

    LOG_INFO(TAG, "WS subprocess spawned (PID=%d)", ws_pid_);
#endif
}

void ExchangeService::stop_ws_stream() {
    if (!ws_running_) return;

    LOG_INFO(TAG, "Stopping WS stream...");
    ws_running_ = false;
    ws_connected_ = false;

#ifdef _WIN32
    // Terminate the subprocess
    if (ws_process_) {
        TerminateProcess((HANDLE)ws_process_, 0);
        WaitForSingleObject((HANDLE)ws_process_, 3000);
        CloseHandle((HANDLE)ws_process_);
        ws_process_ = nullptr;
    }
    if (ws_stdout_rd_) {
        CloseHandle((HANDLE)ws_stdout_rd_);
        ws_stdout_rd_ = nullptr;
    }
#else
    if (ws_pid_ > 0) {
        kill(ws_pid_, SIGTERM);
        int status;
        waitpid(ws_pid_, &status, WNOHANG);
        ws_pid_ = -1;
    }
    if (ws_stdout_fd_ >= 0) {
        close(ws_stdout_fd_);
        ws_stdout_fd_ = -1;
    }
#endif

    if (ws_thread_.joinable()) {
        ws_thread_.join();
    }

    LOG_INFO(TAG, "WS stream stopped");
}

bool ExchangeService::is_ws_connected() const {
    return ws_connected_;
}

void ExchangeService::set_ws_primary_symbol(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    ws_primary_symbol_ = symbol;
    LOG_INFO(TAG, "WS primary symbol changed to: %s", symbol.c_str());
}

std::string ExchangeService::get_ws_primary_symbol() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ws_primary_symbol_;
}

void ExchangeService::ws_reader_loop() {
    // Read stdout line by line, parse JSON, dispatch
    std::string line_buf;

#ifdef _WIN32
    HANDLE rd = (HANDLE)ws_stdout_rd_;
    char ch;
    DWORD bytes_read;

    while (ws_running_) {
        if (!ReadFile(rd, &ch, 1, &bytes_read, nullptr) || bytes_read == 0) {
            // Pipe closed or process exited
            LOG_INFO(TAG, "WS pipe closed");
            break;
        }

        if (ch == '\n') {
            // Got a complete line
            if (!line_buf.empty() && line_buf[0] == '{') {
                try {
                    json msg = json::parse(line_buf);
                    handle_ws_message(msg);
                } catch (const json::parse_error& e) {
                    LOG_ERROR(TAG, "WS JSON parse error: %s (line: %.100s...)",
                              e.what(), line_buf.c_str());
                }
            } else if (!line_buf.empty()) {
                // Non-JSON line (Python debug output)
                LOG_DEBUG(TAG, "WS stderr: %s", line_buf.c_str());
            }
            line_buf.clear();
        } else if (ch != '\r') {
            line_buf += ch;
        }
    }
#else
    int fd = ws_stdout_fd_;
    char ch;

    while (ws_running_) {
        ssize_t n = read(fd, &ch, 1);
        if (n <= 0) {
            LOG_INFO(TAG, "WS pipe closed");
            break;
        }

        if (ch == '\n') {
            if (!line_buf.empty() && line_buf[0] == '{') {
                try {
                    json msg = json::parse(line_buf);
                    handle_ws_message(msg);
                } catch (const json::parse_error& e) {
                    LOG_ERROR(TAG, "WS JSON parse error: %s", e.what());
                }
            } else if (!line_buf.empty()) {
                LOG_DEBUG(TAG, "WS stderr: %s", line_buf.c_str());
            }
            line_buf.clear();
        } else if (ch != '\r') {
            line_buf += ch;
        }
    }
#endif

    ws_connected_ = false;

    // Auto-reconnect if WS is still supposed to be running
    if (ws_running_ && ws_reconnect_attempts_ < WS_MAX_RECONNECTS) {
        ws_reconnect_attempts_++;
        LOG_INFO(TAG, "WS disconnected, reconnecting (attempt %d/%d)...",
                 ws_reconnect_attempts_.load(), WS_MAX_RECONNECTS);

        // Clean up old handles before respawning
#ifdef _WIN32
        if (ws_process_) {
            CloseHandle((HANDLE)ws_process_);
            ws_process_ = nullptr;
        }
        if (ws_stdout_rd_) {
            CloseHandle((HANDLE)ws_stdout_rd_);
            ws_stdout_rd_ = nullptr;
        }
#else
        if (ws_pid_ > 0) {
            int status;
            waitpid(ws_pid_, &status, WNOHANG);
            ws_pid_ = -1;
        }
        if (ws_stdout_fd_ >= 0) {
            close(ws_stdout_fd_);
            ws_stdout_fd_ = -1;
        }
#endif

        std::this_thread::sleep_for(std::chrono::seconds(5 * ws_reconnect_attempts_.load()));
        ws_spawn_subprocess();

        // Continue reading from the new subprocess
        ws_reader_loop();
        return;
    }

    // All reconnect attempts exhausted — notify user
    if (ws_running_) {
        LOG_ERROR(TAG, "WS permanently disconnected after %d retries", WS_MAX_RECONNECTS);
        core::notify::send("Connection Lost",
            "WebSocket disconnected from " + exchange_id_ + " after " +
            std::to_string(WS_MAX_RECONNECTS) + " retries",
            core::NotifyLevel::Error);
    }
}

void ExchangeService::handle_ws_message(const json& msg) {
    std::string type = msg.value("type", "");

    if (type == "status") {
        bool connected = msg.value("connected", false);
        ws_connected_ = connected;
        LOG_INFO(TAG, "WS status: connected=%s", connected ? "true" : "false");
        return;
    }

    if (type == "error") {
        std::string error_msg = msg.value("message", "unknown");
        LOG_ERROR(TAG, "WS error: %s", error_msg.c_str());
        return;
    }

    if (type == "ticker") {
        TickerData ticker = parse_ticker(msg);
        if (ticker.symbol.empty()) return;

        // Update price cache
        {
            std::lock_guard<std::mutex> lock(mutex_);
            price_cache_[ticker.symbol] = ticker;
        }

        // Notify price callbacks
        std::vector<PriceUpdateCallback> cbs;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& [_, cb] : price_callbacks_) {
                cbs.push_back(cb);
            }
        }
        for (const auto& cb : cbs) {
            try { cb(ticker.symbol, ticker); }
            catch (const std::exception& e) {
                LOG_ERROR(TAG, "WS price callback error: %s", e.what());
            }
        }

        // Feed to OrderMatcher for paper trading
        PriceData pd;
        pd.last = ticker.last;
        pd.bid = ticker.bid;
        pd.ask = ticker.ask;
        pd.high = ticker.high;
        pd.low = ticker.low;
        pd.volume = ticker.base_volume;
        pd.change = ticker.change;
        pd.change_percent = ticker.percentage;
        pd.timestamp = ticker.timestamp;

        std::unordered_set<std::string> portfolios;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = watched_.find(ticker.symbol);
            if (it != watched_.end()) portfolios = it->second;
        }

        for (const auto& pid : portfolios) {
            OrderMatcher::instance().check_orders(ticker.symbol, pd, pid);
            try { pt_update_position_price(pid, ticker.symbol, ticker.last); }
            catch (...) {}
        }

        return;
    }

    if (type == "orderbook") {
        OrderBookData ob = parse_orderbook(msg);
        if (ob.symbol.empty()) return;

        // Notify orderbook callbacks
        std::vector<OrderBookCallback> cbs;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& [_, cb] : orderbook_callbacks_) {
                cbs.push_back(cb);
            }
        }
        for (const auto& cb : cbs) {
            try { cb(ob.symbol, ob); }
            catch (const std::exception& e) {
                LOG_ERROR(TAG, "WS orderbook callback error: %s", e.what());
            }
        }
        return;
    }

    if (type == "trade") {
        TradeData td;
        td.id = jstr(msg, "id");
        td.symbol = jstr(msg, "symbol");
        td.side = jstr(msg, "side");
        td.price = jdouble(msg, "price");
        td.amount = jdouble(msg, "amount");
        td.cost = jdouble(msg, "cost");
        td.timestamp = jint64(msg, "timestamp");
        if (td.symbol.empty()) return;

        // Notify trade callbacks
        std::vector<TradeCallback> cbs;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& [_, cb] : trade_callbacks_) cbs.push_back(cb);
        }
        for (const auto& cb : cbs) {
            try { cb(td.symbol, td); }
            catch (const std::exception& e) {
                LOG_ERROR(TAG, "WS trade callback error: %s", e.what());
            }
        }

        // Feed trade to OrderMatcher for more accurate paper fills
        PriceData pd;
        pd.last = td.price;
        pd.bid = (td.side == "sell") ? td.price : 0.0;
        pd.ask = (td.side == "buy") ? td.price : 0.0;
        pd.timestamp = td.timestamp;

        std::unordered_set<std::string> portfolios;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = watched_.find(td.symbol);
            if (it != watched_.end()) portfolios = it->second;
        }
        for (const auto& pid : portfolios) {
            OrderMatcher::instance().check_orders(td.symbol, pd, pid);
        }
        return;
    }

    if (type == "ohlcv") {
        auto candle_it = msg.find("candle");
        if (candle_it == msg.end()) return;

        Candle candle = parse_candle(*candle_it);
        std::string symbol = msg.value("symbol", "");
        if (symbol.empty()) return;

        // Notify candle callbacks
        std::vector<CandleCallback> cbs;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& [_, cb] : candle_callbacks_) {
                cbs.push_back(cb);
            }
        }
        for (const auto& cb : cbs) {
            try { cb(symbol, candle); }
            catch (const std::exception& e) {
                LOG_ERROR(TAG, "WS candle callback error: %s", e.what());
            }
        }
        return;
    }

    LOG_DEBUG(TAG, "WS unknown message type: %s", type.c_str());
}

} // namespace fincept::trading
