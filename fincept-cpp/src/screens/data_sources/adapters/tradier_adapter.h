// Tradier Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class TradierAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"tradier", "Tradier", "tradier", Category::MarketData, "TR",
            "Brokerage and market data API", true, true, {
                {"accessToken", "Access Token", FieldType::Password, "", true},
                {"sandbox", "Use Sandbox", FieldType::Checkbox, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto token = get_or(config, "accessToken");
        if (token.empty()) return {false, "Tradier: Access token required", 0};
        std::string base = get_bool(config, "sandbox") ?
            "https://sandbox.tradier.com" : "https://api.tradier.com";
        return test_http_api_key(base + "/v1/markets/clock",
                                 "Authorization", "Bearer " + token, "Tradier");
    }
};

} // namespace fincept::data_sources::adapters
