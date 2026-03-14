// Alpha Vantage Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class AlphaVantageAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"alpha-vantage", "Alpha Vantage", "alpha-vantage", Category::MarketData, "AV",
            "Stock, forex, crypto market data", true, true, {
                {"apiKey", "API Key", FieldType::Password, "", true},
                {"premium", "Premium Plan", FieldType::Checkbox, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto key = get_or(config, "apiKey");
        if (key.empty()) return {false, "Alpha Vantage: API key required", 0};
        return test_http("https://www.alphavantage.co/query?function=TIME_SERIES_INTRADAY&symbol=IBM&interval=1min&apikey=" + key,
                         "Alpha Vantage");
    }
};

} // namespace fincept::data_sources::adapters
