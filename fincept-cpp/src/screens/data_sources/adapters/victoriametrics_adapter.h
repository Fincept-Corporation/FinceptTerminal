// VictoriaMetrics Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class VictoriaMetricsAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"victoriametrics", "VictoriaMetrics", "victoriametrics", Category::TimeSeries, "VM",
            "Fast, cost-effective monitoring", true, true, {
                {"url", "URL", FieldType::Url, "http://localhost:8428", true},
                {"username", "Username", FieldType::Text, "", false},
                {"password", "Password", FieldType::Password, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_http(get_or(config, "url", "http://localhost:8428") + "/health", "VictoriaMetrics");
    }
};

} // namespace fincept::data_sources::adapters
