#pragma once
// Geopolitics data — async HTTP fetching from Fincept API

#include "geopolitics_types.h"
#include <vector>
#include <string>
#include <mutex>
#include <atomic>

namespace fincept::geopolitics {

class GeopoliticsData {
public:
    // Fetch operations (run on background thread)
    void fetch_events(const std::string& api_key, const std::string& country = "",
                      const std::string& category = "", int page = 1, int limit = 50);
    void fetch_countries(const std::string& api_key);
    void fetch_categories(const std::string& api_key);

    // Thread-safe accessors
    std::vector<GeoEvent> events();
    std::vector<UniqueCountry> countries();
    std::vector<UniqueCategory> categories();
    std::string error();
    bool is_loading() const { return loading_.load(); }

    // Pagination
    int current_page() const { return current_page_; }
    int total_pages() const { return total_pages_; }
    int total_events() const { return total_events_; }

private:
    std::mutex mutex_;
    std::vector<GeoEvent> events_;
    std::vector<UniqueCountry> countries_;
    std::vector<UniqueCategory> categories_;
    std::string error_;
    std::atomic<bool> loading_{false};

    int current_page_ = 1;
    int total_pages_ = 1;
    int total_events_ = 0;
};

} // namespace fincept::geopolitics
