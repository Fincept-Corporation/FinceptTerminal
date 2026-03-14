// CoinMarketCap Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class CoinMarketCapAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"coinmarketcap", "CoinMarketCap", "coinmarketcap", Category::MarketData, "CM",
            "Crypto market data and rankings", true, true, {
                {"apiKey", "API Key", FieldType::Password, "", true},
                {"sandbox", "Use Sandbox", FieldType::Checkbox, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto key = get_or(config, "apiKey");
        if (key.empty()) return {false, "CoinMarketCap: API key required", 0};
        return test_http_api_key("https://pro-api.coinmarketcap.com/v1/cryptocurrency/map?limit=1",
                                 "X-CMC_PRO_API_KEY", key, "CoinMarketCap");
    }
};

} // namespace fincept::data_sources::adapters
