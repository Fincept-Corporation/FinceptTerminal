// RethinkDB Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class RethinkDBAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"rethinkdb", "RethinkDB", "rethinkdb", Category::Database, "RT",
            "Real-time document database", true, true, {
                {"host", "Host", FieldType::Text, "localhost", true},
                {"port", "Port", FieldType::Number, "28015", true, "28015"},
                {"database", "Database", FieldType::Text, "test", false, "test"},
                {"username", "Username", FieldType::Text, "admin", false},
                {"password", "Password", FieldType::Password, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_tcp(get_or(config, "host", "localhost"),
                        get_int(config, "port", 28015), "RethinkDB");
    }
};

} // namespace fincept::data_sources::adapters
