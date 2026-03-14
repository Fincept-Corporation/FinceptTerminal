// NATS Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class NATSAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"nats", "NATS", "nats", Category::Streaming, "NA",
            "Cloud-native messaging system", true, true, {
                {"servers", "Servers", FieldType::Text, "nats://localhost:4222", true},
                {"token", "Auth Token", FieldType::Password, "", false},
                {"username", "Username", FieldType::Text, "", false},
                {"password", "Password", FieldType::Password, "", false},
                {"tls", "Use TLS", FieldType::Checkbox, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto [host, port] = parse_url_host_port(
            get_or(config, "servers", "nats://localhost:4222"), 4222);
        return test_tcp(host, port, "NATS");
    }
};

} // namespace fincept::data_sources::adapters
