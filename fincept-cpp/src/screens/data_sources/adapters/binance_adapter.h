// Binance Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class BinanceAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"binance", "Binance", "binance", Category::MarketData, "BN",
            "Crypto exchange API", true, true, {
                {"apiKey", "API Key", FieldType::Password, "", false},
                {"apiSecret", "API Secret", FieldType::Password, "", false},
                {"testnet", "Use Testnet", FieldType::Checkbox, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        std::string base = get_bool(config, "testnet") ?
            "https://testnet.binance.vision" : "https://api.binance.com";
        return test_http(base + "/api/v3/ping", "Binance");
    }
};

} // namespace fincept::data_sources::adapters
