// Yahoo Finance Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class YahooFinanceAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"yahoo-finance", "Yahoo Finance", "yahoo-finance", Category::MarketData, "YF",
            "Free market data from Yahoo", false, true, {
                {"symbol", "Default Symbol", FieldType::Text, "AAPL", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_http("https://query1.finance.yahoo.com/v8/finance/chart/AAPL?range=1d", "Yahoo Finance");
    }
};

} // namespace fincept::data_sources::adapters
