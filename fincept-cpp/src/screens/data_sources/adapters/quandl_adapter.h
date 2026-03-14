// Quandl (Nasdaq Data Link) Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class QuandlAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"quandl", "Quandl / Nasdaq", "quandl", Category::MarketData, "QL",
            "Nasdaq Data Link financial data", true, true, {
                {"apiKey", "API Key", FieldType::Password, "", true},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto key = get_or(config, "apiKey");
        if (key.empty()) return {false, "Quandl: API key required", 0};
        return test_http("https://data.nasdaq.com/api/v3/datasets/WIKI/AAPL.json?rows=1&api_key=" + key, "Quandl");
    }
};

} // namespace fincept::data_sources::adapters
