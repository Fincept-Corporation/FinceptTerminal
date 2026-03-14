// REST API Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class RESTAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"rest-api", "REST API", "rest-api", Category::Api, "RE",
            "RESTful HTTP API endpoint", true, true, {
                {"baseUrl", "Base URL", FieldType::Url, "https://api.example.com", true},
                {"authType", "Auth Type", FieldType::Select, "", false, "none", {
                    {"none", "None"}, {"bearer", "Bearer Token"}, {"basic", "Basic Auth"}, {"apikey", "API Key"},
                }},
                {"token", "Token / API Key", FieldType::Password, "", false},
                {"username", "Username (Basic)", FieldType::Text, "", false},
                {"password", "Password (Basic)", FieldType::Password, "", false},
                {"headers", "Custom Headers (JSON)", FieldType::Textarea, "{}", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"baseUrl"}, "REST API");
        if (!v.success) return v;
        std::map<std::string, std::string> headers;
        auto auth = get_or(config, "authType", "none");
        if (auth == "bearer")
            headers["Authorization"] = "Bearer " + get_or(config, "token");
        else if (auth == "apikey")
            headers["X-API-Key"] = get_or(config, "token");
        return test_http(get_or(config, "baseUrl"), "REST API", headers);
    }
};

} // namespace fincept::data_sources::adapters
