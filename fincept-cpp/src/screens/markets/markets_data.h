#pragma once
// Markets tab — data fetching layer
// Uses yfinance batch_quotes to fetch quotes for multiple symbols

#include "markets_types.h"
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <string>
#include <vector>
#include <map>

namespace fincept::markets {

class MarketsData {
public:
    // Fetch quotes for a category (runs in background thread)
    void fetch_category(const std::string& category, const std::vector<std::string>& symbols);

    // Fetch all categories at once
    void fetch_all(const std::vector<MarketCategory>& categories);

    std::shared_mutex& mutex() { return mutex_; }

    bool is_loading(const std::string& category) const;
    bool has_data(const std::string& category) const;
    const std::string& error(const std::string& category) const;

    // Get quotes for a category
    const std::vector<MarketQuote>& quotes(const std::string& category) const;

private:
    mutable std::shared_mutex mutex_;

    struct CategoryState {
        std::atomic<bool> loading{false};
        bool has_data = false;
        std::string error;
        std::vector<MarketQuote> quotes;
    };

    mutable std::map<std::string, CategoryState> categories_;
    static const std::string empty_string_;
    static const std::vector<MarketQuote> empty_quotes_;

    void parse_quotes(const std::string& category, const std::string& json_str);
};

} // namespace fincept::markets
