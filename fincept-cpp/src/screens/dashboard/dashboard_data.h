#pragma once
// DashboardData — singleton data service for the dashboard
// Fetches real market data via yfinance Python subprocess in background threads
// Thread-safe: all data access through mutex-locked getters

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>

namespace fincept::dashboard {

struct QuoteEntry {
    std::string symbol;
    std::string label;       // display name
    double price          = 0;
    double change         = 0;
    double change_percent = 0;
    double high           = 0;
    double low            = 0;
    double open           = 0;
    double previous_close = 0;
    double volume         = 0;
};

struct NewsItem {
    std::string headline;
    std::string source;
    std::string category;
    std::string time_str;
    int64_t sort_ts = 0;
    int priority = 3;  // 0=flash, 1=urgent, 2=breaking, 3=routine
};

// Category IDs
enum class DataCategory {
    Indices,
    Forex,
    Commodities,
    Crypto,
    SectorETFs,
    TopMovers,
    Ticker,       // aggregated ticker bar data
    MarketPulse,  // VIX, TNX, DXY, Gold, Oil, BTC
    Count
};

inline const char* category_name(DataCategory c) {
    switch (c) {
        case DataCategory::Indices:     return "Indices";
        case DataCategory::Forex:       return "Forex";
        case DataCategory::Commodities: return "Commodities";
        case DataCategory::Crypto:      return "Crypto";
        case DataCategory::SectorETFs:  return "SectorETFs";
        case DataCategory::TopMovers:   return "TopMovers";
        case DataCategory::Ticker:      return "Ticker";
        case DataCategory::MarketPulse: return "MarketPulse";
        default: return "Unknown";
    }
}

class DashboardData {
public:
    static DashboardData& instance();

    // Initialize and start first fetch
    void initialize();

    // Call each frame — checks timers and triggers refresh if needed
    void update(float dt);

    // Force refresh all categories now
    void refresh_all();

    // Force refresh one category
    void refresh(DataCategory cat);

    // Data access (thread-safe copies)
    std::vector<QuoteEntry> get_quotes(DataCategory cat) const;
    std::vector<NewsItem>   get_news() const;

    // Status
    bool is_loading(DataCategory cat) const;
    bool has_data(DataCategory cat) const;
    std::string get_error(DataCategory cat) const;
    float time_since_refresh(DataCategory cat) const;

    // Configuration
    void set_refresh_interval(float seconds);
    float get_refresh_interval() const { return refresh_interval_; }

    // Timestamp of last successful fetch for display
    std::string last_refresh_str(DataCategory cat) const;

    DashboardData(const DashboardData&) = delete;
    DashboardData& operator=(const DashboardData&) = delete;

private:
    DashboardData() = default;

    struct CategoryState {
        std::vector<QuoteEntry> quotes;
        std::atomic<bool> loading{false};
        bool has_data = false;
        std::string error;
        float timer = 0;
        std::chrono::steady_clock::time_point last_fetch{};
    };

    mutable std::mutex mutex_;
    CategoryState categories_[static_cast<int>(DataCategory::Count)];

    std::vector<NewsItem> news_;
    std::atomic<bool> news_loading_{false};
    bool news_has_data_ = false;
    float news_timer_ = 0;

    float refresh_interval_ = 600.0f;  // 10 minutes default
    bool initialized_ = false;

    // Symbol lists (matching React originals)
    static std::vector<std::string> symbols_for(DataCategory cat);
    static std::map<std::string, std::string> labels_for(DataCategory cat);

    // Background fetch
    void fetch_category(DataCategory cat);
    void fetch_news_feeds();

    // Python subprocess runner
    static std::string run_yfinance(const std::string& args);

    // Parse yfinance JSON output into QuoteEntry vector
    static std::vector<QuoteEntry> parse_quotes(const std::string& json_str,
                                                 const std::map<std::string, std::string>& labels);
};

} // namespace fincept::dashboard
