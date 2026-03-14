// Finnhub Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class FinnhubAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"finnhub", "Finnhub", "finnhub", Category::MarketData, "FH",
            "Real-time stock & financial data", true, true, {
                {"apiKey", "API Key", FieldType::Password, "", true},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto key = get_or(config, "apiKey");
        if (key.empty()) return {false, "Finnhub: API key required", 0};
        return test_http("https://finnhub.io/api/v1/quote?symbol=AAPL&token=" + key, "Finnhub");
    }
};

} // namespace fincept::data_sources::adapters
