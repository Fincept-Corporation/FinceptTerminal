// IEX Cloud Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class IEXCloudAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"iex-cloud", "IEX Cloud", "iex-cloud", Category::MarketData, "IX",
            "Financial data platform", true, true, {
                {"token", "API Token", FieldType::Password, "", true},
                {"sandbox", "Use Sandbox", FieldType::Checkbox, "", false},
                {"version", "API Version", FieldType::Select, "", false, "stable", {
                    {"stable", "Stable"}, {"latest", "Latest"}, {"beta", "Beta"},
                }},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto token = get_or(config, "token");
        if (token.empty()) return {false, "IEX Cloud: API token required", 0};
        std::string base = get_bool(config, "sandbox") ?
            "https://sandbox.iexapis.com" : "https://cloud.iexapis.com";
        return test_http(base + "/stable/stock/AAPL/quote?token=" + token, "IEX Cloud");
    }
};

} // namespace fincept::data_sources::adapters
