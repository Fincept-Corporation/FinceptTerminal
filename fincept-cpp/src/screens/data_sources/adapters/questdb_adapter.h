// QuestDB Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class QuestDBAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"questdb", "QuestDB", "questdb", Category::TimeSeries, "QD",
            "High-performance time series DB", true, true, {
                {"host", "Host", FieldType::Text, "localhost", true},
                {"port", "HTTP Port", FieldType::Number, "9000", true, "9000"},
                {"pgPort", "PostgreSQL Wire Port", FieldType::Number, "8812", false, "8812"},
                {"username", "Username", FieldType::Text, "admin", false},
                {"password", "Password", FieldType::Password, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_tcp(get_or(config, "host", "localhost"),
                        get_int(config, "port", 9000), "QuestDB");
    }
};

} // namespace fincept::data_sources::adapters
