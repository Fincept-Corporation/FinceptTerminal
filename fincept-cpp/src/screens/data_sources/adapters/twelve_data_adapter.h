// Twelve Data Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class TwelveDataAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"twelve-data", "Twelve Data", "twelve-data", Category::MarketData, "12",
            "Financial data APIs", true, true, {
                {"apiKey", "API Key", FieldType::Password, "", true},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto key = get_or(config, "apiKey");
        if (key.empty()) return {false, "Twelve Data: API key required", 0};
        return test_http("https://api.twelvedata.com/time_series?symbol=AAPL&interval=1min&apikey=" + key + "&outputsize=1",
                         "Twelve Data");
    }
};

} // namespace fincept::data_sources::adapters
