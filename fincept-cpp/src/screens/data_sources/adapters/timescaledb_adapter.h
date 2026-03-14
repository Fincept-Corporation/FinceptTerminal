// TimescaleDB Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class TimescaleDBAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"timescaledb", "TimescaleDB", "timescaledb", Category::TimeSeries, "TS",
            "PostgreSQL extension for time series", true, true, {
                {"host", "Host", FieldType::Text, "localhost", true},
                {"port", "Port", FieldType::Number, "5432", true, "5432"},
                {"database", "Database", FieldType::Text, "tsdb", true},
                {"username", "Username", FieldType::Text, "postgres", true},
                {"password", "Password", FieldType::Password, "", true},
                {"ssl", "Use SSL", FieldType::Checkbox, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"host", "database", "username", "password"}, "TimescaleDB");
        if (!v.success) return v;
        return test_tcp(get_or(config, "host", "localhost"),
                        get_int(config, "port", 5432), "TimescaleDB");
    }
};

} // namespace fincept::data_sources::adapters
