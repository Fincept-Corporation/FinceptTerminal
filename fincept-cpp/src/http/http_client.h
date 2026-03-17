#pragma once
// Production-grade HTTP client wrapping libcurl
// Thread-safe, connection pooling, timeout handling, async support

#include <string>
#include <map>
#include <functional>
#include <future>
#include <mutex>
#include <nlohmann/json.hpp>

namespace fincept::http {

using json = nlohmann::json;

struct HttpResponse {
    int status_code = 0;
    std::string body;
    bool success = false;
    std::string error;

    json json_body() const {
        try {
            return json::parse(body);
        } catch (...) {
            return json();
        }
    }
};

using Headers = std::map<std::string, std::string>;

class HttpClient {
public:
    static HttpClient& instance();

    // Synchronous requests
    HttpResponse get(const std::string& url, const Headers& headers = {});
    HttpResponse post(const std::string& url, const std::string& body = "", const Headers& headers = {});
    HttpResponse put(const std::string& url, const std::string& body = "", const Headers& headers = {});
    HttpResponse del(const std::string& url, const std::string& body = "", const Headers& headers = {});

    // JSON convenience
    HttpResponse post_json(const std::string& url, const json& body, const Headers& headers = {});
    HttpResponse put_json(const std::string& url, const json& body, const Headers& headers = {});

    // Async requests (returns future)
    std::future<HttpResponse> get_async(const std::string& url, const Headers& headers = {});
    std::future<HttpResponse> post_json_async(const std::string& url, const json& body, const Headers& headers = {});

    // Configuration
    void set_timeout(long timeout_seconds);
    void set_connect_timeout(long timeout_seconds);

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

private:
    HttpClient();
    ~HttpClient();

    HttpResponse perform(const std::string& method, const std::string& url,
                         const std::string& body, const Headers& headers);

    long timeout_seconds_ = 15;
    long connect_timeout_seconds_ = 5;
    std::mutex mutex_;
};

} // namespace fincept::http
