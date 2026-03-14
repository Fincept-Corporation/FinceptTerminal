// PostgreSQL Database Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class PostgreSQLAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"postgresql", "PostgreSQL", "postgresql", Category::Database, "PG",
            "Open-source relational database", true, true, {
                {"host", "Host", FieldType::Text, "localhost", true},
                {"port", "Port", FieldType::Number, "5432", true, "5432"},
                {"database", "Database", FieldType::Text, "mydb", true},
                {"username", "Username", FieldType::Text, "postgres", true},
                {"password", "Password", FieldType::Password, "", true},
                {"ssl", "SSL Mode", FieldType::Select, "", false, "", {
                    {"disable", "Disable"}, {"require", "Require"},
                    {"verify-ca", "Verify CA"}, {"verify-full", "Verify Full"}
                }},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"host", "database", "username", "password"}, "PostgreSQL");
        if (!v.success) return v;
        return test_tcp(get_or(config, "host", "localhost"),
                        get_int(config, "port", 5432), "PostgreSQL");
    }

    AdapterMetadata get_metadata(const std::map<std::string, std::string>& config) override {
        return {"postgresql", "PostgreSQL", "database", {
            {"host", get_or(config, "host")},
            {"port", get_or(config, "port", "5432")},
            {"database", get_or(config, "database")},
            {"ssl", get_or(config, "ssl", "disable")},
        }};
    }
};

} // namespace fincept::data_sources::adapters
