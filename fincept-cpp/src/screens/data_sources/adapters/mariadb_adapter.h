// MariaDB Database Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class MariaDBAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"mariadb", "MariaDB", "mariadb", Category::Database, "MA",
            "MySQL-compatible fork", true, true, {
                {"host", "Host", FieldType::Text, "localhost", true},
                {"port", "Port", FieldType::Number, "3306", true, "3306"},
                {"database", "Database", FieldType::Text, "mydb", true},
                {"username", "Username", FieldType::Text, "root", true},
                {"password", "Password", FieldType::Password, "", true},
                {"ssl", "Use SSL", FieldType::Checkbox, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"host", "database", "username"}, "MariaDB");
        if (!v.success) return v;
        return test_tcp(get_or(config, "host", "localhost"),
                        get_int(config, "port", 3306), "MariaDB");
    }
};

} // namespace fincept::data_sources::adapters
