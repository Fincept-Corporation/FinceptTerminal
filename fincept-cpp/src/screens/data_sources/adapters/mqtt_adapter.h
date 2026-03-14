// MQTT Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class MQTTAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"mqtt", "MQTT", "mqtt", Category::Streaming, "MQ",
            "Lightweight messaging protocol", true, true, {
                {"broker", "Broker Host", FieldType::Text, "localhost", true},
                {"port", "Port", FieldType::Number, "1883", true, "1883"},
                {"clientId", "Client ID", FieldType::Text, "fincept-terminal", false},
                {"username", "Username", FieldType::Text, "", false},
                {"password", "Password", FieldType::Password, "", false},
                {"topic", "Default Topic", FieldType::Text, "#", false},
                {"tls", "Use TLS", FieldType::Checkbox, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_tcp(get_or(config, "broker", "localhost"),
                        get_int(config, "port", 1883), "MQTT");
    }
};

} // namespace fincept::data_sources::adapters
