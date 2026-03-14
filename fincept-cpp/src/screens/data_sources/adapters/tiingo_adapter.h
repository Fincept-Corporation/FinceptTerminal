// Tiingo Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class TiingoAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"tiingo", "Tiingo", "tiingo", Category::MarketData, "TG",
            "Financial data and news", true, true, {
                {"apiToken", "API Token", FieldType::Password, "", true},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto token = get_or(config, "apiToken");
        if (token.empty()) return {false, "Tiingo: API token required", 0};
        return test_http_api_key("https://api.tiingo.com/api/test",
                                 "Authorization", "Token " + token, "Tiingo");
    }
};

} // namespace fincept::data_sources::adapters
