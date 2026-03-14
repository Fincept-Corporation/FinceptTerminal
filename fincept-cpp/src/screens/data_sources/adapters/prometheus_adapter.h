// Prometheus Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class PrometheusAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"prometheus", "Prometheus", "prometheus", Category::TimeSeries, "PM",
            "Monitoring and time series DB", true, true, {
                {"url", "URL", FieldType::Url, "http://localhost:9090", true},
                {"username", "Username", FieldType::Text, "", false},
                {"password", "Password", FieldType::Password, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_http(get_or(config, "url", "http://localhost:9090") + "/-/healthy", "Prometheus");
    }
};

} // namespace fincept::data_sources::adapters
