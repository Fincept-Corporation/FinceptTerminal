// Production HTTP client implementation using libcurl
// Thread-safe with connection reuse and proper error handling

#include "http_client.h"
#include "core/logger.h"
#include "core/raii.h"
#include <curl/curl.h>
#include <stdexcept>
#include <cstring>
#include <cstdlib>

namespace fincept::http {

// libcurl write callback
static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* response_body = static_cast<std::string*>(userdata);
    size_t total = size * nmemb;
    response_body->append(ptr, total);
    return total;
}

// Lazy curl global init — called once on first HttpClient access.
// Avoids static initialization order issues and DLL-load deadlocks on Windows.
static void ensure_curl_initialized() {
    static bool initialized = false;
    if (!initialized) {
        curl_global_init(CURL_GLOBAL_ALL);
        std::atexit([] { curl_global_cleanup(); });
        initialized = true;
    }
}

HttpClient& HttpClient::instance() {
    static HttpClient client;
    return client;
}

HttpClient::HttpClient() {
    ensure_curl_initialized();
}
HttpClient::~HttpClient() = default;

void HttpClient::set_timeout(long timeout_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    timeout_seconds_ = timeout_seconds;
}

void HttpClient::set_connect_timeout(long timeout_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    connect_timeout_seconds_ = timeout_seconds;
}

HttpResponse HttpClient::perform(const std::string& method, const std::string& url,
                                  const std::string& body, const Headers& headers) {
    HttpResponse response;

    core::CurlHandle curl;
    if (!curl) {
        response.error = "Failed to initialize CURL";
        return response;
    }

    std::string response_body;

    // URL
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());

    // Method
    if (method == "POST") {
        curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, (long)body.size());
    } else if (method == "PUT") {
        curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, (long)body.size());
    } else if (method == "DELETE") {
        curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, "DELETE");
        if (!body.empty()) {
            curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, body.c_str());
            curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, (long)body.size());
        }
    }
    // GET is default

    // Headers
    core::CurlSlist header_list;
    header_list.append("Content-Type: application/json");
    header_list.append("Accept: application/json");
    for (const auto& [key, value] : headers) {
        std::string header = key + ": " + value;
        header_list.append(header.c_str());
    }
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, header_list.get());

    // Response
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response_body);

    // Timeouts
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, timeout_seconds_);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, connect_timeout_seconds_);

    // SSL
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L);

    // Follow redirects
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 5L);

    // Connection reuse
    curl_easy_setopt(curl.get(), CURLOPT_TCP_KEEPALIVE, 1L);

    // User agent — must look like a real browser or Cloudflare blocks with 1010
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) FinceptTerminal/4.0.0 Chrome/131.0.0.0 Safari/537.36");

    // Perform
    CURLcode res = curl_easy_perform(curl.get());

    if (res != CURLE_OK) {
        response.error = curl_easy_strerror(res);
        if (res == CURLE_OPERATION_TIMEDOUT) {
            response.error = "Request timed out";
        } else if (res == CURLE_COULDNT_CONNECT) {
            response.error = "Could not connect to server";
        } else if (res == CURLE_COULDNT_RESOLVE_HOST) {
            response.error = "Could not resolve host";
        }
        LOG_ERROR("HTTP", "%s %s failed: %s", method.c_str(), url.c_str(), response.error.c_str());
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &http_code);
        response.status_code = static_cast<int>(http_code);
        response.body = std::move(response_body);
        response.success = (http_code >= 200 && http_code < 300);
        if (!response.success) {
            LOG_WARN("HTTP", "%s %s returned %d", method.c_str(), url.c_str(), static_cast<int>(http_code));
        } else {
            LOG_DEBUG("HTTP", "%s %s -> %d", method.c_str(), url.c_str(), static_cast<int>(http_code));
        }
    }
    // header_list and curl cleaned up automatically by destructors

    return response;
}

HttpResponse HttpClient::get(const std::string& url, const Headers& headers) {
    return perform("GET", url, "", headers);
}

HttpResponse HttpClient::post(const std::string& url, const std::string& body, const Headers& headers) {
    return perform("POST", url, body, headers);
}

HttpResponse HttpClient::put(const std::string& url, const std::string& body, const Headers& headers) {
    return perform("PUT", url, body, headers);
}

HttpResponse HttpClient::del(const std::string& url, const std::string& body, const Headers& headers) {
    return perform("DELETE", url, body, headers);
}

HttpResponse HttpClient::post_json(const std::string& url, const json& body, const Headers& headers) {
    return perform("POST", url, body.dump(), headers);
}

HttpResponse HttpClient::put_json(const std::string& url, const json& body, const Headers& headers) {
    return perform("PUT", url, body.dump(), headers);
}

std::future<HttpResponse> HttpClient::get_async(const std::string& url, const Headers& headers) {
    return std::async(std::launch::async, [this, url, headers]() {
        return this->get(url, headers);
    });
}

std::future<HttpResponse> HttpClient::post_json_async(const std::string& url, const json& body, const Headers& headers) {
    return std::async(std::launch::async, [this, url, body, headers]() {
        return this->post_json(url, body, headers);
    });
}

} // namespace fincept::http
