// ScyllaDB Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class ScyllaDBAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"scylladb", "ScyllaDB", "scylladb", Category::Database, "SC",
            "High-performance Cassandra-compatible DB", true, true, {
                {"contactPoints", "Contact Points", FieldType::Text, "localhost", true},
                {"port", "Port", FieldType::Number, "9042", true, "9042"},
                {"datacenter", "Local Datacenter", FieldType::Text, "datacenter1", true},
                {"keyspace", "Keyspace", FieldType::Text, "", false},
                {"username", "Username", FieldType::Text, "", false},
                {"password", "Password", FieldType::Password, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_tcp(get_or(config, "contactPoints", "localhost"),
                        get_int(config, "port", 9042), "ScyllaDB");
    }
};

} // namespace fincept::data_sources::adapters
