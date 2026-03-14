// RabbitMQ Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class RabbitMQAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"rabbitmq", "RabbitMQ", "rabbitmq", Category::Streaming, "RQ",
            "Message broker with AMQP", true, true, {
                {"host", "Host", FieldType::Text, "localhost", true},
                {"port", "AMQP Port", FieldType::Number, "5672", true, "5672"},
                {"managementPort", "Management Port", FieldType::Number, "15672", false, "15672"},
                {"vhost", "Virtual Host", FieldType::Text, "/", false, "/"},
                {"username", "Username", FieldType::Text, "guest", true, "guest"},
                {"password", "Password", FieldType::Password, "guest", true},
                {"tls", "Use TLS", FieldType::Checkbox, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"host", "username", "password"}, "RabbitMQ");
        if (!v.success) return v;
        return test_tcp(get_or(config, "host", "localhost"),
                        get_int(config, "port", 5672), "RabbitMQ");
    }
};

} // namespace fincept::data_sources::adapters
