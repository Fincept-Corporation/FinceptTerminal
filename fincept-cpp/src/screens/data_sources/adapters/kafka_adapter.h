// Apache Kafka Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class KafkaAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"kafka", "Kafka", "kafka", Category::Streaming, "KF",
            "Distributed event streaming platform", true, true, {
                {"brokers", "Brokers", FieldType::Text, "localhost:9092", true},
                {"clientId", "Client ID", FieldType::Text, "fincept-terminal", false},
                {"groupId", "Consumer Group ID", FieldType::Text, "", false},
                {"saslMechanism", "SASL Mechanism", FieldType::Select, "", false, "", {
                    {"", "None"}, {"PLAIN", "PLAIN"}, {"SCRAM-SHA-256", "SCRAM-SHA-256"},
                    {"SCRAM-SHA-512", "SCRAM-SHA-512"},
                }},
                {"saslUsername", "SASL Username", FieldType::Text, "", false},
                {"saslPassword", "SASL Password", FieldType::Password, "", false},
                {"ssl", "Use SSL", FieldType::Checkbox, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"brokers"}, "Kafka");
        if (!v.success) return v;
        auto [host, port] = parse_host_port(get_or(config, "brokers", "localhost:9092"), 9092);
        return test_tcp(host, port, "Kafka");
    }
};

} // namespace fincept::data_sources::adapters
