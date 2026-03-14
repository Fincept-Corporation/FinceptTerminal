// ClickHouse Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class ClickHouseAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"clickhouse", "ClickHouse", "clickhouse", Category::TimeSeries, "CH",
            "Column-oriented OLAP database", true, true, {
                {"host", "Host", FieldType::Text, "localhost", true},
                {"port", "HTTP Port", FieldType::Number, "8123", true, "8123"},
                {"database", "Database", FieldType::Text, "default", false, "default"},
                {"username", "Username", FieldType::Text, "default", false, "default"},
                {"password", "Password", FieldType::Password, "", false},
                {"nativePort", "Native Port", FieldType::Number, "9000", false, "9000"},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_http("http://" + get_or(config, "host", "localhost") + ":" +
                         std::to_string(get_int(config, "port", 8123)) + "/ping", "ClickHouse");
    }
};

} // namespace fincept::data_sources::adapters
