// OpenTSDB Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class OpenTSDBAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"opentsdb", "OpenTSDB", "opentsdb", Category::TimeSeries, "OT",
            "Scalable time series on HBase", true, true, {
                {"url", "URL", FieldType::Url, "http://localhost:4242", true},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_http(get_or(config, "url", "http://localhost:4242") + "/api/version", "OpenTSDB");
    }
};

} // namespace fincept::data_sources::adapters
