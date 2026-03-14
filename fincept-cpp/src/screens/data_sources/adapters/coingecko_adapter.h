// CoinGecko Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class CoinGeckoAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"coingecko", "CoinGecko", "coingecko", Category::MarketData, "CG",
            "Free crypto market data", false, true, {
                {"apiKey", "API Key (Pro)", FieldType::Password, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_http("https://api.coingecko.com/api/v3/ping", "CoinGecko");
    }
};

} // namespace fincept::data_sources::adapters
