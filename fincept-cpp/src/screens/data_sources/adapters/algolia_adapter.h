// Algolia Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class AlgoliaAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"algolia", "Algolia", "algolia", Category::Database, "AL",
            "Hosted search-as-a-service", true, true, {
                {"appId", "Application ID", FieldType::Text, "", true},
                {"apiKey", "API Key", FieldType::Password, "", true},
                {"indexName", "Index Name", FieldType::Text, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"appId", "apiKey"}, "Algolia");
        if (!v.success) return v;
        return test_http_api_key("https://" + get_or(config, "appId") + "-dsn.algolia.net/1/indexes",
                                 "X-Algolia-API-Key", get_or(config, "apiKey"), "Algolia");
    }
};

} // namespace fincept::data_sources::adapters
