// Redis Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class RedisAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"redis", "Redis", "redis", Category::Database, "RD",
            "In-memory key-value store", true, true, {
                {"host", "Host", FieldType::Text, "localhost", true},
                {"port", "Port", FieldType::Number, "6379", true, "6379"},
                {"password", "Password", FieldType::Password, "", false},
                {"database", "Database Index", FieldType::Number, "0", false, "0"},
                {"tls", "Use TLS", FieldType::Checkbox, "", false},
                {"cluster", "Cluster Mode", FieldType::Checkbox, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_tcp(get_or(config, "host", "localhost"),
                        get_int(config, "port", 6379), "Redis");
    }
};

} // namespace fincept::data_sources::adapters
