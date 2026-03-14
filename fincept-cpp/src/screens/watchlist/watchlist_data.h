#pragma once
// Watchlist data layer — quote fetching via python/yfinance
// Same pattern as MarketsData: background thread fetch, mutex-guarded results

#include "watchlist_types.h"
#include <mutex>
#include <atomic>
#include <map>

namespace fincept::watchlist {

class WatchlistData {
public:
    // Fetch quotes for a list of symbols (background thread)
    void fetch_quotes(const std::vector<std::string>& symbols);

    // Check state
    bool is_loading() const { return loading_.load(); }
    bool has_data() const { return has_data_; }
    const std::string& error() const { return error_; }

    // Get quote for a symbol (call under lock or after has_data)
    const StockQuote* quote(const std::string& symbol) const;

    std::mutex& mutex() { return mutex_; }

private:
    std::mutex mutex_;
    std::atomic<bool> loading_{false};
    bool has_data_ = false;
    std::string error_;
    std::map<std::string, StockQuote> quotes_;

    void parse_quotes(const std::string& json_str);
};

} // namespace fincept::watchlist
