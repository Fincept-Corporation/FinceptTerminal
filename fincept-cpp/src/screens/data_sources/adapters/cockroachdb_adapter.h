// CockroachDB Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class CockroachDBAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"cockroachdb", "CockroachDB", "cockroachdb", Category::Database, "CR",
            "Distributed SQL database", true, true, {
                {"host", "Host", FieldType::Text, "localhost", true},
                {"port", "Port", FieldType::Number, "26257", true, "26257"},
                {"database", "Database", FieldType::Text, "defaultdb", true},
                {"username", "Username", FieldType::Text, "root", true},
                {"password", "Password", FieldType::Password, "", false},
                {"sslMode", "SSL Mode", FieldType::Select, "", false, "", {
                    {"disable", "Disable"}, {"require", "Require"}, {"verify-full", "Verify Full"},
                }},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"host", "database", "username"}, "CockroachDB");
        if (!v.success) return v;
        return test_tcp(get_or(config, "host", "localhost"),
                        get_int(config, "port", 26257), "CockroachDB");
    }
};

} // namespace fincept::data_sources::adapters
