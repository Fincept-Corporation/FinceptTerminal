// Elasticsearch Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class ElasticsearchAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"elasticsearch", "Elasticsearch", "elasticsearch", Category::Database, "ES",
            "Distributed search and analytics", true, true, {
                {"node", "Node URL", FieldType::Url, "http://localhost:9200", true},
                {"username", "Username", FieldType::Text, "", false},
                {"password", "Password", FieldType::Password, "", false},
                {"apiKey", "API Key", FieldType::Password, "", false},
                {"cloudId", "Elastic Cloud ID", FieldType::Text, "", false},
                {"index", "Default Index", FieldType::Text, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        std::string url = get_or(config, "node", "http://localhost:9200");
        std::map<std::string, std::string> headers;
        auto apiKey = get_or(config, "apiKey");
        auto user = get_or(config, "username");
        auto pass = get_or(config, "password");
        if (!apiKey.empty())
            headers["Authorization"] = "ApiKey " + apiKey;
        else if (!user.empty() && !pass.empty())
            headers["Authorization"] = "Basic " + user + ":" + pass;  // Simplified
        return test_http(url + "/_cluster/health", "Elasticsearch", headers);
    }
};

} // namespace fincept::data_sources::adapters
