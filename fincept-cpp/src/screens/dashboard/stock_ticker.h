#pragma once
// Fincept-style scrolling stock ticker bar
// Fetches real market data and renders a smooth horizontal scroll

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>

namespace fincept::dashboard {

struct TickerItem {
    std::string symbol;
    std::string name;
    float price = 0.0f;
    float change = 0.0f;
    float change_pct = 0.0f;
    bool valid = false;
};

class StockTicker {
public:
    static StockTicker& instance();

    void initialize();
    void shutdown();
    void render(float bar_height = 24.0f);

    StockTicker(const StockTicker&) = delete;
    StockTicker& operator=(const StockTicker&) = delete;

private:
    StockTicker() = default;
    ~StockTicker() = default;

    void fetch_prices();
    void fetch_loop();

    std::vector<TickerItem> items_;
    std::mutex mutex_;
    std::atomic<bool> running_{false};
    std::thread fetch_thread_;
    float scroll_offset_ = 0.0f;
    bool fetched_ = false;
    double last_fetch_time_ = 0.0;
};

} // namespace fincept::dashboard
