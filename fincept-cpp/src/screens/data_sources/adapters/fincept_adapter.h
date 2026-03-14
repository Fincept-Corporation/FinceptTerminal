// Fincept API Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class FinceptAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"fincept", "Fincept API", "fincept", Category::MarketData, "FN",
            "Fincept Corporation data API", true, true, {
                {"host", "Host", FieldType::Text, "localhost", true, "localhost"},
                {"port", "Port", FieldType::Number, "8194", true, "8194"},
                {"apiKey", "API Key", FieldType::Password, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_tcp(get_or(config, "host", "localhost"),
                        get_int(config, "port", 8194), "Fincept API");
    }
};

} // namespace fincept::data_sources::adapters
