// MeiliSearch Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class MeiliSearchAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"meilisearch", "MeiliSearch", "meilisearch", Category::Database, "ME",
            "Lightning-fast search engine", true, true, {
                {"host", "Host URL", FieldType::Url, "http://localhost:7700", true},
                {"apiKey", "Master Key", FieldType::Password, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_http(get_or(config, "host", "http://localhost:7700") + "/health", "MeiliSearch");
    }
};

} // namespace fincept::data_sources::adapters
