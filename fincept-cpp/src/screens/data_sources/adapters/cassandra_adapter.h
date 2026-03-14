// Apache Cassandra Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class CassandraAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"cassandra", "Cassandra", "cassandra", Category::Database, "CS",
            "Wide-column NoSQL database", true, true, {
                {"contactPoints", "Contact Points", FieldType::Text, "localhost", true},
                {"port", "Port", FieldType::Number, "9042", true, "9042"},
                {"datacenter", "Local Datacenter", FieldType::Text, "datacenter1", true},
                {"keyspace", "Keyspace", FieldType::Text, "", false},
                {"username", "Username", FieldType::Text, "", false},
                {"password", "Password", FieldType::Password, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"contactPoints", "datacenter"}, "Cassandra");
        if (!v.success) return v;
        return test_tcp(get_or(config, "contactPoints", "localhost"),
                        get_int(config, "port", 9042), "Cassandra");
    }
};

} // namespace fincept::data_sources::adapters
