// OpenSearch Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class OpenSearchAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"opensearch", "OpenSearch", "opensearch", Category::Database, "OS",
            "AWS-backed search engine", true, true, {
                {"node", "Node URL", FieldType::Url, "https://localhost:9200", true},
                {"username", "Username", FieldType::Text, "admin", false},
                {"password", "Password", FieldType::Password, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_http(get_or(config, "node", "https://localhost:9200"), "OpenSearch");
    }
};

} // namespace fincept::data_sources::adapters
