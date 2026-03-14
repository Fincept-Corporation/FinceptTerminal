// EOD Historical Data Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class EODHistoricalAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"eod-historical", "EOD Historical Data", "eod-historical", Category::MarketData, "ED",
            "End-of-day and intraday data", true, true, {
                {"apiToken", "API Token", FieldType::Password, "", true},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto token = get_or(config, "apiToken");
        if (token.empty()) return {false, "EOD Historical: API token required", 0};
        return test_http("https://eodhistoricaldata.com/api/real-time/AAPL.US?api_token=" + token + "&fmt=json",
                         "EOD Historical");
    }
};

} // namespace fincept::data_sources::adapters
