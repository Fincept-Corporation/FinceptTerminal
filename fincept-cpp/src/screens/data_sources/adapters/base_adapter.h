// Base Adapter Interface for Data Source Connections
// All data source adapters extend this base class
#pragma once

#include "../data_sources_types.h"
#include "http/http_client.h"
#include "core/logger.h"
#include <string>
#include <map>
#include <future>
#include <chrono>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#endif

namespace fincept::data_sources::adapters {

using Clock = std::chrono::steady_clock;

// Result from connection test
struct TestResult {
    bool success = false;
    std::string message;
    double elapsed_ms = 0.0;
};

// Result from a query operation
struct QueryResult {
    bool success = false;
    std::string data;       // JSON string
    std::string error;
    int row_count = 0;
};

// Metadata about the adapter
struct AdapterMetadata {
    std::string type;
    std::string name;
    std::string category;
    std::map<std::string, std::string> extra;
};

class BaseAdapter {
public:
    virtual ~BaseAdapter() = default;

    // Every adapter must provide its definition (fields, category, etc.)
    virtual DataSourceDef get_definition() const = 0;

    // Test connectivity — must be implemented by each adapter
    virtual TestResult test_connection(const std::map<std::string, std::string>& config) = 0;

    // Async test wrapper
    std::future<TestResult> test_connection_async(const std::map<std::string, std::string>& config) {
        return std::async(std::launch::async, [this, config]() {
            return test_connection(config);
        });
    }

    // Optional: query data (override if supported)
    virtual QueryResult query(const std::string& /*query_str*/,
                              const std::map<std::string, std::string>& /*config*/) {
        return {false, "", "Query not implemented for this adapter", 0};
    }

    // Optional: get metadata
    virtual AdapterMetadata get_metadata(const std::map<std::string, std::string>& config) {
        auto def = get_definition();
        return {def.type, def.name, category_id(def.category), {}};
    }

protected:
    // ========================================================================
    // Helper: get config value with default
    // ========================================================================
    std::string get_or(const std::map<std::string, std::string>& m,
                       const std::string& key, const std::string& def = "") const {
        auto it = m.find(key);
        return (it != m.end() && !it->second.empty()) ? it->second : def;
    }

    int get_int(const std::map<std::string, std::string>& m,
                const std::string& key, int def = 0) const {
        auto it = m.find(key);
        if (it != m.end() && !it->second.empty()) {
            try { return std::stoi(it->second); } catch (...) {}
        }
        return def;
    }

    bool get_bool(const std::map<std::string, std::string>& m,
                  const std::string& key, bool def = false) const {
        auto it = m.find(key);
        if (it != m.end()) return (it->second == "true" || it->second == "1");
        return def;
    }

    // ========================================================================
    // Validate required fields are present
    // ========================================================================
    TestResult validate_required(const std::map<std::string, std::string>& config,
                                 const std::vector<std::string>& fields,
                                 const std::string& label) {
        for (auto& f : fields) {
            auto it = config.find(f);
            if (it == config.end() || it->second.empty()) {
                return {false, label + ": Missing required field '" + f + "'", 0};
            }
        }
        return {true, "", 0};
    }

    // ========================================================================
    // TCP socket connect test (databases, streaming brokers)
    // ========================================================================
    TestResult test_tcp(const std::string& host, int port, const std::string& label) {
        auto start = Clock::now();
        TestResult result;

        if (host.empty() || port <= 0) {
            result.message = label + ": Invalid host or port";
            return result;
        }

#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

        struct addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        std::string port_str = std::to_string(port);
        int gai_err = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
        if (gai_err != 0) {
            result.elapsed_ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            result.message = label + ": DNS resolution failed for " + host;
            return result;
        }

#ifdef _WIN32
        SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sock == INVALID_SOCKET) {
            freeaddrinfo(res);
            result.elapsed_ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            result.message = label + ": Failed to create socket";
            return result;
        }

        u_long mode = 1;
        ioctlsocket(sock, FIONBIO, &mode);

        int conn_result = connect(sock, res->ai_addr, (int)res->ai_addrlen);

        if (conn_result == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                fd_set write_fds;
                FD_ZERO(&write_fds);
                FD_SET(sock, &write_fds);
                struct timeval tv;
                tv.tv_sec = 5;
                tv.tv_usec = 0;

                int sel = select(0, nullptr, &write_fds, nullptr, &tv);
                if (sel > 0) {
                    int so_error = 0;
                    int len = sizeof(so_error);
                    getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&so_error, &len);
                    result.success = (so_error == 0);
                }
            }
        } else {
            result.success = true;
        }

        closesocket(sock);
#else
        int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sock < 0) {
            freeaddrinfo(res);
            result.elapsed_ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            result.message = label + ": Failed to create socket";
            return result;
        }

        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        int conn_result = connect(sock, res->ai_addr, res->ai_addrlen);

        if (conn_result < 0 && errno == EINPROGRESS) {
            fd_set write_fds;
            FD_ZERO(&write_fds);
            FD_SET(sock, &write_fds);
            struct timeval tv;
            tv.tv_sec = 5;
            tv.tv_usec = 0;

            int sel = select(sock + 1, nullptr, &write_fds, nullptr, &tv);
            if (sel > 0) {
                int so_error = 0;
                socklen_t len = sizeof(so_error);
                getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
                result.success = (so_error == 0);
            }
        } else {
            result.success = (conn_result == 0);
        }

        close(sock);
#endif

        freeaddrinfo(res);
        result.elapsed_ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();

        if (result.success) {
            result.message = label + ": Connected to " + host + ":" + port_str +
                             " (" + std::to_string((int)result.elapsed_ms) + "ms)";
        } else {
            result.message = label + ": Connection refused — " + host + ":" + port_str +
                             " (is the service running?)";
        }

        LOG_INFO("Adapter", "%s", result.message.c_str());
        return result;
    }

    // ========================================================================
    // HTTP probe test (APIs, search engines, cloud endpoints)
    // ========================================================================
    TestResult test_http(const std::string& url, const std::string& label,
                         const std::map<std::string, std::string>& headers = {}) {
        auto start = Clock::now();
        TestResult result;

        if (url.empty()) {
            result.message = label + ": No URL configured";
            return result;
        }

        try {
            auto& client = http::HttpClient::instance();
            http::Headers hdrs;
            for (auto& [k, v] : headers) hdrs[k] = v;

            auto resp = client.get(url, hdrs);
            result.elapsed_ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();

            if (resp.status_code > 0) {
                result.success = true;
                result.message = label + ": Reachable (HTTP " + std::to_string(resp.status_code) +
                                 ", " + std::to_string((int)result.elapsed_ms) + "ms)";
            } else {
                result.message = label + ": " + (resp.error.empty() ? "Connection failed" : resp.error);
            }
        } catch (const std::exception& e) {
            result.elapsed_ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            result.message = label + ": " + std::string(e.what());
        }

        LOG_INFO("Adapter", "%s", result.message.c_str());
        return result;
    }

    // ========================================================================
    // HTTP with API key header
    // ========================================================================
    TestResult test_http_api_key(const std::string& url, const std::string& key_header,
                                 const std::string& api_key, const std::string& label) {
        if (api_key.empty()) {
            return {false, label + ": API key is required", 0};
        }
        return test_http(url, label, {{key_header, api_key}});
    }

    // ========================================================================
    // File existence test
    // ========================================================================
    TestResult test_file(const std::string& filepath, const std::string& label) {
        auto start = Clock::now();
        TestResult result;

        if (filepath.empty()) {
            result.message = label + ": No file path configured";
            return result;
        }

        std::ifstream f(filepath);
        result.elapsed_ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();

        if (f.good()) {
            f.seekg(0, std::ios::end);
            auto size = f.tellg();
            result.success = true;
            result.message = label + ": File found (" + std::to_string(size) + " bytes)";
        } else {
            result.message = label + ": File not found — " + filepath;
        }

        LOG_INFO("Adapter", "%s", result.message.c_str());
        return result;
    }

    // ========================================================================
    // Connection string parser (mongodb:// etc.)
    // ========================================================================
    TestResult test_connection_string(const std::string& conn_str, const std::string& label,
                                       int default_port = 27017) {
        std::string host = "localhost";
        int port = default_port;

        auto pos = conn_str.find("://");
        if (pos != std::string::npos) {
            std::string rest = conn_str.substr(pos + 3);
            auto at = rest.find('@');
            if (at != std::string::npos) rest = rest.substr(at + 1);
            auto slash = rest.find('/');
            if (slash != std::string::npos) rest = rest.substr(0, slash);
            auto q = rest.find('?');
            if (q != std::string::npos) rest = rest.substr(0, q);
            auto colon = rest.find(':');
            if (colon != std::string::npos) {
                host = rest.substr(0, colon);
                try { port = std::stoi(rest.substr(colon + 1)); } catch (...) {}
            } else {
                host = rest;
            }
        }

        return test_tcp(host, port, label);
    }

    // ========================================================================
    // Parse host:port from URL (for WebSocket, NATS, etc.)
    // ========================================================================
    std::pair<std::string, int> parse_url_host_port(const std::string& url, int default_port) {
        std::string host = "localhost";
        int port = default_port;
        auto pos = url.find("://");
        if (pos != std::string::npos) {
            std::string rest = url.substr(pos + 3);
            auto slash = rest.find('/');
            if (slash != std::string::npos) rest = rest.substr(0, slash);
            auto colon = rest.find(':');
            if (colon != std::string::npos) {
                host = rest.substr(0, colon);
                try { port = std::stoi(rest.substr(colon + 1)); } catch (...) {}
            } else {
                host = rest;
            }
        }
        return {host, port};
    }

    // ========================================================================
    // Parse first host:port from comma-separated list
    // ========================================================================
    std::pair<std::string, int> parse_host_port(const std::string& str, int default_port) {
        std::string first = str;
        auto comma = first.find(',');
        if (comma != std::string::npos) first = first.substr(0, comma);

        auto colon = first.find(':');
        if (colon != std::string::npos) {
            std::string host = first.substr(0, colon);
            int port = default_port;
            try { port = std::stoi(first.substr(colon + 1)); } catch (...) {}
            return {host, port};
        }
        return {first, default_port};
    }
};

} // namespace fincept::data_sources::adapters
