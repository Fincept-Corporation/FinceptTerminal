// Intrinio Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class IntrinioAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"intrinio", "Intrinio", "intrinio", Category::MarketData, "IN",
            "Financial data and analytics", true, true, {
                {"apiKey", "API Key", FieldType::Password, "", true},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto key = get_or(config, "apiKey");
        if (key.empty()) return {false, "Intrinio: API key required", 0};
        return test_http_api_key("https://api-v2.intrinio.com/securities/AAPL",
                                 "Authorization", "Bearer " + key, "Intrinio");
    }
};

} // namespace fincept::data_sources::adapters
