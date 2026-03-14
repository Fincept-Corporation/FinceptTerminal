// ArangoDB Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class ArangoDBAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"arangodb", "ArangoDB", "arangodb", Category::Database, "AR",
            "Multi-model database (graph/doc/kv)", true, true, {
                {"url", "URL", FieldType::Url, "http://localhost:8529", true},
                {"database", "Database", FieldType::Text, "_system", true, "_system"},
                {"username", "Username", FieldType::Text, "root", true},
                {"password", "Password", FieldType::Password, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"url", "username"}, "ArangoDB");
        if (!v.success) return v;
        return test_http(get_or(config, "url", "http://localhost:8529") + "/_api/version", "ArangoDB");
    }
};

} // namespace fincept::data_sources::adapters
