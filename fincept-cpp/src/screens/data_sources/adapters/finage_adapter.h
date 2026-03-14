// Finage Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class FinageAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"finage", "Finage", "finage", Category::MarketData, "FG",
            "Real-time financial data", true, true, {
                {"apiKey", "API Key", FieldType::Password, "", true},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto key = get_or(config, "apiKey");
        if (key.empty()) return {false, "Finage: API key required", 0};
        return test_http("https://api.finage.co.uk/last/stock/AAPL?apikey=" + key, "Finage");
    }
};

} // namespace fincept::data_sources::adapters
