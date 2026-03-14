#pragma once
// QuantLib API — Unified HTTP client for all 590+ QuantLib endpoints
// All calls go to https://api.fincept.in/quantlib/...

#include <string>
#include <future>
#include <map>
#include <nlohmann/json.hpp>

namespace fincept::quantlib {

using json = nlohmann::json;

struct ApiResult {
    bool success = false;
    json data;
    std::string error;
    int status_code = 0;
    double elapsed_ms = 0;
};

class QuantLibApi {
public:
    static QuantLibApi& instance();

    // POST to /quantlib/{path} with JSON body
    std::future<ApiResult> post(const std::string& path, const json& body);

    // GET to /quantlib/{path} with optional query params
    std::future<ApiResult> get(const std::string& path,
                                const std::map<std::string, std::string>& params = {});

private:
    QuantLibApi() = default;
    std::string get_api_key();

    static constexpr const char* BASE_URL = "https://api.fincept.in";
};

} // namespace fincept::quantlib
