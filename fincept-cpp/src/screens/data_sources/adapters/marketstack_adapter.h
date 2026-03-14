// Marketstack Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class MarketstackAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"marketstack", "Marketstack", "marketstack", Category::MarketData, "MK",
            "Real-time and historical market data", true, true, {
                {"apiKey", "Access Key", FieldType::Password, "", true},
                {"https", "Use HTTPS (paid)", FieldType::Checkbox, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto key = get_or(config, "apiKey");
        if (key.empty()) return {false, "Marketstack: API key required", 0};
        std::string proto = get_bool(config, "https") ? "https" : "http";
        return test_http(proto + "://api.marketstack.com/v1/tickers?access_key=" + key + "&limit=1", "Marketstack");
    }
};

} // namespace fincept::data_sources::adapters
