#include "quantlib_api.h"
#include "http/http_client.h"
#include "auth/auth_manager.h"
#include "storage/cache_service.h"
#include "core/logger.h"
#include <chrono>

namespace fincept::quantlib {

QuantLibApi& QuantLibApi::instance() {
    static QuantLibApi inst;
    return inst;
}

std::string QuantLibApi::get_api_key() {
    auto& auth = auth::AuthManager::instance();
    if (auth.is_authenticated()) {
        return auth.session().api_key;
    }
    return "";
}

std::future<ApiResult> QuantLibApi::post(const std::string& path, const json& body) {
    return std::async(std::launch::async, [this, path, body]() -> ApiResult {
        ApiResult result;
        auto start = std::chrono::high_resolution_clock::now();

        std::string url = std::string(BASE_URL) + "/quantlib/" + path;
        std::string api_key = get_api_key();

        std::map<std::string, std::string> headers;
        if (!api_key.empty()) {
            headers["X-API-Key"] = api_key;
        }

        auto& http_client = http::HttpClient::instance();
        auto resp = http_client.post_json(url, body, headers);

        auto end = std::chrono::high_resolution_clock::now();
        result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
        result.status_code = resp.status_code;

        if (resp.success) {
            result.data = resp.json_body();
            result.success = true;
        } else {
            result.error = resp.error.empty()
                ? ("HTTP " + std::to_string(resp.status_code))
                : resp.error;
        }

        return result;
    });
}

std::future<ApiResult> QuantLibApi::get(const std::string& path,
                                          const std::map<std::string, std::string>& params) {
    return std::async(std::launch::async, [this, path, params]() -> ApiResult {
        ApiResult result;
        auto start = std::chrono::high_resolution_clock::now();

        std::string url = std::string(BASE_URL) + "/quantlib/" + path;

        if (!params.empty()) {
            url += "?";
            bool first = true;
            for (auto& [k, v] : params) {
                if (!first) url += "&";
                url += k + "=" + v;
                first = false;
            }
        }

        // Check cache for GET requests
        auto& cache = CacheService::instance();
        std::string params_key;
        for (auto& [k, v] : params) params_key += k + "=" + v + "&";
        std::string cache_key = CacheService::make_key("api-response", "quantlib", path + "_" + params_key);

        auto cached = cache.get(cache_key);
        if (cached && !cached->empty()) {
            auto end = std::chrono::high_resolution_clock::now();
            result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
            result.data = nlohmann::json::parse(*cached, nullptr, false);
            if (!result.data.is_discarded()) {
                result.success = true;
                result.status_code = 200;
                return result;
            }
        }

        std::string api_key = get_api_key();
        std::map<std::string, std::string> headers;
        if (!api_key.empty()) {
            headers["X-API-Key"] = api_key;
        }

        auto& http_client = http::HttpClient::instance();
        auto resp = http_client.get(url, headers);

        auto end = std::chrono::high_resolution_clock::now();
        result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
        result.status_code = resp.status_code;

        if (resp.success) {
            result.data = resp.json_body();
            result.success = true;
            if (!resp.body.empty()) {
                cache.set(cache_key, resp.body, "api-response", CacheTTL::FIFTEEN_MIN);
            }
        } else {
            result.error = resp.error.empty()
                ? ("HTTP " + std::to_string(resp.status_code))
                : resp.error;
        }

        return result;
    });
}

} // namespace fincept::quantlib
