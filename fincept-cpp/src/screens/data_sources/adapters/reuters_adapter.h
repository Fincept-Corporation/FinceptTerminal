// Reuters / Refinitiv Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class ReutersAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"reuters", "Reuters / Refinitiv", "reuters", Category::MarketData, "RT",
            "Professional financial data", true, true, {
                {"apiKey", "API Key", FieldType::Password, "", true},
                {"appKey", "App Key", FieldType::Password, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto key = get_or(config, "apiKey");
        if (key.empty()) return {false, "Reuters: API key required", 0};
        return test_http("https://api.refinitiv.com/", "Reuters/Refinitiv");
    }
};

} // namespace fincept::data_sources::adapters
