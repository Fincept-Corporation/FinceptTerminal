// Geopolitics data fetching — calls Fincept API for news events, countries, categories

#include "geopolitics_data.h"
#include "http/http_client.h"
#include "auth/auth_manager.h"
#include "storage/cache_service.h"
#include "core/logger.h"
#include <thread>

namespace fincept::geopolitics {

static const char* API_BASE = "https://api.fincept.in";

static http::Headers make_headers(const std::string& api_key) {
    http::Headers h;
    h["X-API-Key"] = api_key;
    h["accept"] = "application/json";

    auto& auth = auth::AuthManager::instance();
    auto token = auth.session().session_token;
    if (!token.empty()) {
        h["X-Session-Token"] = token;
    }
    LOG_DEBUG("Geo", "Headers: X-API-Key=%s..., session_token=%s",
              api_key.substr(0, std::min((size_t)8, api_key.size())).c_str(),
              token.empty() ? "(none)" : token.substr(0, 8).c_str());
    return h;
}

// ============================================================================
// Fetch events
// ============================================================================
void GeopoliticsData::fetch_events(const std::string& api_key, const std::string& country,
                                    const std::string& category, int page, int limit) {
    LOG_INFO("Geo", "=== fetch_events called: api_key_len=%zu, country='%s', category='%s', page=%d, limit=%d",
             api_key.size(), country.c_str(), category.c_str(), page, limit);

    if (loading_.load()) {
        LOG_WARN("Geo", "fetch_events SKIPPED — already loading");
        return;
    }
    loading_ = true;

    std::thread([this, api_key, country, category, page, limit]() {
        std::string url = std::string(API_BASE) + "/research/news-events?";
        if (!country.empty()) url += "country=" + country + "&";
        if (!category.empty()) url += "event_category=" + category + "&";
        url += "page=" + std::to_string(page) + "&limit=" + std::to_string(limit);

        LOG_INFO("Geo", "GET %s", url.c_str());

        auto& cache = CacheService::instance();
        std::string cache_key = CacheService::make_key("api-response", "geo-events",
            country + "_" + category + "_p" + std::to_string(page));

        // Try cache first
        auto cached_body = cache.get(cache_key);
        std::string body;
        int status_code = 200;
        bool success = false;

        if (cached_body) {
            body = *cached_body;
            success = true;
            LOG_INFO("Geo", "Cache HIT for geo-events key");
        } else {
            auto headers = make_headers(api_key);
            auto resp = http::HttpClient::instance().get(url, headers);
            body = resp.body;
            status_code = resp.status_code;
            success = resp.success;

            LOG_INFO("Geo", "Response: status=%d, success=%d, body_len=%zu, error='%s'",
                     resp.status_code, (int)resp.success, resp.body.size(), resp.error.c_str());

            if (!resp.body.empty()) {
                std::string preview = resp.body.substr(0, std::min((size_t)500, resp.body.size()));
                LOG_DEBUG("Geo", "Body preview: %s", preview.c_str());
            }

            // Cache successful responses for 15 minutes
            if (resp.success && !resp.body.empty()) {
                cache.set(cache_key, resp.body, "api-response", CacheTTL::FIFTEEN_MIN);
            }
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (!success) {
            // Try to extract error message from API response body
            if (!body.empty()) {
                try {
                    auto err_json = nlohmann::json::parse(body);
                    error_ = err_json.value("message", "API error (HTTP " + std::to_string(status_code) + ")");
                } catch (...) {
                    error_ = "HTTP " + std::to_string(status_code);
                }
            } else {
                error_ = "Network error";
            }
            LOG_ERROR("Geo", "Fetch events FAILED: %s", error_.c_str());
            loading_ = false;
            return;
        }

        try {
            auto j = nlohmann::json::parse(body);

            // Log top-level keys
            std::string keys;
            for (auto it = j.begin(); it != j.end(); ++it) {
                if (!keys.empty()) keys += ", ";
                keys += it.key();
            }
            LOG_INFO("Geo", "JSON top-level keys: [%s]", keys.c_str());

            if (!j.value("success", false)) {
                error_ = j.value("message", "API error");
                LOG_ERROR("Geo", "API returned success=false: %s", error_.c_str());
                loading_ = false;
                return;
            }

            events_.clear();

            // API wraps everything under "data"
            auto& data = j.contains("data") ? j["data"] : j;

            if (!data.contains("events")) {
                // Log all keys for debugging
                std::string data_keys;
                for (auto it = data.begin(); it != data.end(); ++it) {
                    if (!data_keys.empty()) data_keys += ", ";
                    data_keys += it.key();
                }
                LOG_ERROR("Geo", "No 'events' key in data! Keys: [%s]", data_keys.c_str());
                error_ = "API response missing 'events' field";
                loading_ = false;
                return;
            }

            auto& events_arr = data["events"];
            LOG_INFO("Geo", "events array size: %zu", events_arr.size());

            int with_coords = 0;
            for (const auto& ev_json : events_arr) {
                GeoEvent ev;
                ev.url = ev_json.value("url", "");
                ev.domain = ev_json.value("domain", "");
                ev.event_category = ev_json.value("event_category", "");
                ev.matched_keywords = ev_json.value("matched_keywords", "");
                ev.city = ev_json.value("city", "");
                ev.country = ev_json.value("country", "");
                ev.latitude = ev_json.value("latitude", 0.0);
                ev.longitude = ev_json.value("longitude", 0.0);
                ev.extracted_date = ev_json.value("extracted_date", "");
                ev.created_at = ev_json.value("created_at", "");

                if (ev.latitude != 0.0 || ev.longitude != 0.0) with_coords++;

                events_.push_back(std::move(ev));
            }

            // Pagination is under data.pagination
            if (data.contains("pagination")) {
                auto& pg = data["pagination"];
                current_page_ = pg.value("current_page", page);
                total_pages_ = pg.value("total_pages", 1);
                total_events_ = pg.value("total_events", 0);
            } else {
                current_page_ = j.value("page", page);
                total_events_ = j.value("total", 0);
                total_pages_ = (total_events_ + limit - 1) / limit;
            }

            error_.clear();
            LOG_INFO("Geo", "SUCCESS: %zu events parsed, %d with coords, page %d/%d, total=%d",
                     events_.size(), with_coords, current_page_, total_pages_, total_events_);

            // Log first event for verification
            if (!events_.empty()) {
                auto& first = events_[0];
                LOG_DEBUG("Geo", "First event: cat='%s', city='%s', country='%s', lat=%.4f, lon=%.4f",
                          first.event_category.c_str(), first.city.c_str(), first.country.c_str(),
                          first.latitude, first.longitude);
            }
        } catch (const std::exception& ex) {
            error_ = std::string("Parse error: ") + ex.what();
            LOG_ERROR("Geo", "Parse EXCEPTION: %s", ex.what());
        }

        loading_ = false;
    }).detach();
}

// ============================================================================
// Fetch unique countries
// ============================================================================
void GeopoliticsData::fetch_countries(const std::string& api_key) {
    LOG_INFO("Geo", "=== fetch_countries called, api_key_len=%zu", api_key.size());
    std::thread([this, api_key]() {
        auto& cache = CacheService::instance();
        std::string cache_key = CacheService::make_key("api-response", "geo-countries", "list");

        std::string body;
        auto cached = cache.get(cache_key);
        if (cached) {
            body = *cached;
            LOG_INFO("Geo", "Cache HIT for geo-countries");
        } else {
            std::string url = std::string(API_BASE) + "/research/news-events?get_unique_countries=true";
            LOG_INFO("Geo", "GET %s", url.c_str());
            auto resp = http::HttpClient::instance().get(url, make_headers(api_key));

            LOG_INFO("Geo", "Countries response: status=%d, success=%d, body_len=%zu",
                     resp.status_code, (int)resp.success, resp.body.size());

            if (!resp.success) {
                std::lock_guard<std::mutex> lock(mutex_);
                LOG_ERROR("Geo", "Countries fetch FAILED: %s (body: %.200s)",
                          resp.error.c_str(), resp.body.c_str());
                return;
            }
            body = resp.body;
            if (!body.empty()) {
                cache.set(cache_key, body, "api-response", CacheTTL::ONE_HOUR);
            }
        }

        std::lock_guard<std::mutex> lock(mutex_);
        try {
            auto j = nlohmann::json::parse(body);
            if (!j.value("success", false)) {
                LOG_ERROR("Geo", "Countries API returned success=false");
                return;
            }

            // Log structure
            std::string keys;
            for (auto it = j.begin(); it != j.end(); ++it) {
                if (!keys.empty()) keys += ", ";
                keys += it.key();
            }
            LOG_DEBUG("Geo", "Countries JSON keys: [%s]", keys.c_str());

            countries_.clear();
            if (!j.contains("data") || !j["data"].contains("unique_countries")) {
                LOG_ERROR("Geo", "Countries: missing data.unique_countries! Keys: [%s]", keys.c_str());
                return;
            }

            auto& arr = j["data"]["unique_countries"];
            for (auto& c : arr) {
                countries_.push_back({
                    c.value("country", ""),
                    c.value("event_count", 0)
                });
            }

            std::sort(countries_.begin(), countries_.end(),
                [](const UniqueCountry& a, const UniqueCountry& b) {
                    return a.event_count > b.event_count;
                });

            LOG_INFO("Geo", "Loaded %zu countries", countries_.size());
        } catch (const std::exception& ex) {
            LOG_ERROR("Geo", "Countries parse EXCEPTION: %s", ex.what());
        }
    }).detach();
}

// ============================================================================
// Fetch unique categories
// ============================================================================
void GeopoliticsData::fetch_categories(const std::string& api_key) {
    LOG_INFO("Geo", "=== fetch_categories called, api_key_len=%zu", api_key.size());
    std::thread([this, api_key]() {
        auto& cache = CacheService::instance();
        std::string cache_key = CacheService::make_key("api-response", "geo-categories", "list");

        std::string body;
        auto cached = cache.get(cache_key);
        if (cached) {
            body = *cached;
            LOG_INFO("Geo", "Cache HIT for geo-categories");
        } else {
            std::string url = std::string(API_BASE) + "/research/news-events?get_unique_categories=true";
            LOG_INFO("Geo", "GET %s", url.c_str());
            auto resp = http::HttpClient::instance().get(url, make_headers(api_key));

            LOG_INFO("Geo", "Categories response: status=%d, success=%d, body_len=%zu",
                     resp.status_code, (int)resp.success, resp.body.size());

            if (!resp.success) {
                std::lock_guard<std::mutex> lock(mutex_);
                LOG_ERROR("Geo", "Categories fetch FAILED: %s (body: %.200s)",
                          resp.error.c_str(), resp.body.c_str());
                return;
            }
            body = resp.body;
            if (!body.empty()) {
                cache.set(cache_key, body, "api-response", CacheTTL::ONE_HOUR);
            }
        }

        std::lock_guard<std::mutex> lock(mutex_);
        try {
            auto j = nlohmann::json::parse(body);
            if (!j.value("success", false)) {
                LOG_ERROR("Geo", "Categories API returned success=false");
                return;
            }

            categories_.clear();
            if (!j.contains("data") || !j["data"].contains("unique_categories")) {
                LOG_ERROR("Geo", "Categories: missing data.unique_categories!");
                return;
            }

            auto& arr = j["data"]["unique_categories"];
            for (auto& c : arr) {
                categories_.push_back({
                    c.value("event_category", ""),
                    c.value("event_count", 0)
                });
            }

            std::sort(categories_.begin(), categories_.end(),
                [](const UniqueCategory& a, const UniqueCategory& b) {
                    return a.event_count > b.event_count;
                });

            LOG_INFO("Geo", "Loaded %zu categories", categories_.size());
        } catch (const std::exception& ex) {
            LOG_ERROR("Geo", "Categories parse EXCEPTION: %s", ex.what());
        }
    }).detach();
}

// ============================================================================
// Thread-safe accessors
// ============================================================================
std::vector<GeoEvent> GeopoliticsData::events() {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_;
}

std::vector<UniqueCountry> GeopoliticsData::countries() {
    std::lock_guard<std::mutex> lock(mutex_);
    return countries_;
}

std::vector<UniqueCategory> GeopoliticsData::categories() {
    std::lock_guard<std::mutex> lock(mutex_);
    return categories_;
}

std::string GeopoliticsData::error() {
    std::lock_guard<std::mutex> lock(mutex_);
    return error_;
}

} // namespace fincept::geopolitics
