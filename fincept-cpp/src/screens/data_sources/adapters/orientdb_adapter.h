// OrientDB Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class OrientDBAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"orientdb", "OrientDB", "orientdb", Category::Database, "OD",
            "Multi-model database (graph/document)", true, true, {
                {"host", "Host", FieldType::Text, "localhost", true},
                {"port", "Port", FieldType::Number, "2424", true, "2424"},
                {"database", "Database", FieldType::Text, "", true},
                {"username", "Username", FieldType::Text, "root", true},
                {"password", "Password", FieldType::Password, "", true},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"host", "database", "username", "password"}, "OrientDB");
        if (!v.success) return v;
        return test_tcp(get_or(config, "host", "localhost"),
                        get_int(config, "port", 2424), "OrientDB");
    }
};

} // namespace fincept::data_sources::adapters
