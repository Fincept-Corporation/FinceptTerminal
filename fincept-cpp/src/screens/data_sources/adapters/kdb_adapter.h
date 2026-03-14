// kdb+ Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class KDBAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"kdb", "kdb+", "kdb", Category::TimeSeries, "KD",
            "High-frequency time series database", true, true, {
                {"host", "Host", FieldType::Text, "localhost", true},
                {"port", "Port", FieldType::Number, "5000", true, "5000"},
                {"username", "Username", FieldType::Text, "", false},
                {"password", "Password", FieldType::Password, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_tcp(get_or(config, "host", "localhost"),
                        get_int(config, "port", 5000), "kdb+");
    }
};

} // namespace fincept::data_sources::adapters
