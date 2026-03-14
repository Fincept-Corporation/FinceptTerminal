// Coinbase Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class CoinbaseAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"coinbase", "Coinbase", "coinbase", Category::MarketData, "CB",
            "Crypto exchange API", true, true, {
                {"apiKey", "API Key", FieldType::Password, "", false},
                {"apiSecret", "API Secret", FieldType::Password, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_http("https://api.coinbase.com/v2/ping", "Coinbase");
    }
};

} // namespace fincept::data_sources::adapters
