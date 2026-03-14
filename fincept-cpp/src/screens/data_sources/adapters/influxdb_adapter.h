// InfluxDB Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class InfluxDBAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"influxdb", "InfluxDB", "influxdb", Category::TimeSeries, "IX",
            "Time series database", true, true, {
                {"url", "URL", FieldType::Url, "http://localhost:8086", true},
                {"token", "API Token", FieldType::Password, "", true},
                {"org", "Organization", FieldType::Text, "", true},
                {"bucket", "Bucket", FieldType::Text, "", false},
                {"version", "Version", FieldType::Select, "", false, "2", {
                    {"1", "v1.x"}, {"2", "v2.x"},
                }},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"url", "token", "org"}, "InfluxDB");
        if (!v.success) return v;
        return test_http(get_or(config, "url", "http://localhost:8086") + "/ping", "InfluxDB");
    }
};

} // namespace fincept::data_sources::adapters
