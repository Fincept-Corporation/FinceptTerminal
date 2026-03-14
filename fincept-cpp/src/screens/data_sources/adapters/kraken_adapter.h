// Kraken Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class KrakenAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"kraken", "Kraken", "kraken", Category::MarketData, "KR",
            "Crypto exchange API", true, true, {
                {"apiKey", "API Key", FieldType::Password, "", false},
                {"apiSecret", "Private Key", FieldType::Password, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_http("https://api.kraken.com/0/public/SystemStatus", "Kraken");
    }
};

} // namespace fincept::data_sources::adapters
