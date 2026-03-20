#pragma once
// EquityStreamManager — manages the broker_ws_bridge.py subprocess for live
// equity price streaming.  Same pipe pattern as ExchangeService / ws_stream.py.
//
// Usage:
//   auto& esm = EquityStreamManager::instance();
//   esm.start(broker, creds, {{"RELIANCE","NSE"}, {"INFY","NSE"}});
//   esm.on_tick([](const EquityTick& t){ /* update UI */ });
//   esm.stop();

#include "broker_types.h"
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <chrono>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fincept::trading {

struct EquityTick {
    std::string symbol;
    std::string exchange;
    double ltp      = 0.0;
    double open     = 0.0;
    double high     = 0.0;
    double low      = 0.0;
    double close    = 0.0;
    double volume   = 0.0;
    double bid      = 0.0;
    double ask      = 0.0;
    int64_t timestamp = 0;
};

struct EquitySymbol {
    std::string symbol;
    std::string exchange;
};

using TickCallback   = std::function<void(const EquityTick&)>;
using StatusCallback = std::function<void(bool connected, const std::string& msg)>;

class EquityStreamManager {
public:
    static EquityStreamManager& instance();

    // Start streaming for given broker+creds+symbol list.
    // Stops any existing stream first.
    void start(const std::string& broker_adapter,
               const BrokerCredentials& creds,
               const std::vector<EquitySymbol>& symbols);

    void stop();
    bool is_connected() const { return connected_; }
    bool is_running()   const { return running_; }

    // Latest cached tick for a symbol (thread-safe)
    bool get_tick(const std::string& symbol, EquityTick& out) const;

    // Register callbacks (returns ID for removal)
    int  on_tick(TickCallback cb);
    void remove_tick_callback(int id);
    int  on_status(StatusCallback cb);
    void remove_status_callback(int id);

    EquityStreamManager(const EquityStreamManager&) = delete;
    EquityStreamManager& operator=(const EquityStreamManager&) = delete;

private:
    EquityStreamManager() = default;
    ~EquityStreamManager() { stop(); }

    void reader_loop();
    void handle_message(const std::string& line);
    void spawn_subprocess(const std::string& broker,
                          const BrokerCredentials& creds,
                          const std::vector<EquitySymbol>& symbols);
    void kill_subprocess();

    // Subprocess handles
#ifdef _WIN32
    void* proc_handle_  = nullptr;
    void* stdout_rd_    = nullptr;
#else
    int proc_pid_    = -1;
    int stdout_fd_   = -1;
#endif

    std::thread reader_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};

    // Tick cache
    mutable std::mutex mutex_;
    std::unordered_map<std::string, EquityTick> tick_cache_;

    // Callbacks
    std::unordered_map<int, TickCallback>   tick_cbs_;
    std::unordered_map<int, StatusCallback> status_cbs_;
    int next_cb_id_ = 1;

    // Last status time (for UI heartbeat display)
    std::chrono::steady_clock::time_point last_tick_time_;
};

} // namespace fincept::trading
