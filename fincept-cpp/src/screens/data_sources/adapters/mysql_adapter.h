// MySQL Database Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class MySQLAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"mysql", "MySQL", "mysql", Category::Database, "MY",
            "Popular open-source RDBMS", true, true, {
                {"host", "Host", FieldType::Text, "localhost", true},
                {"port", "Port", FieldType::Number, "3306", true, "3306"},
                {"database", "Database", FieldType::Text, "mydb", true},
                {"username", "Username", FieldType::Text, "root", true},
                {"password", "Password", FieldType::Password, "", true},
                {"ssl", "Use SSL", FieldType::Checkbox, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"host", "database", "username"}, "MySQL");
        if (!v.success) return v;
        return test_tcp(get_or(config, "host", "localhost"),
                        get_int(config, "port", 3306), "MySQL");
    }

    AdapterMetadata get_metadata(const std::map<std::string, std::string>& config) override {
        return {"mysql", "MySQL", "database", {
            {"host", get_or(config, "host")}, {"port", get_or(config, "port", "3306")},
            {"database", get_or(config, "database")},
        }};
    }
};

} // namespace fincept::data_sources::adapters
