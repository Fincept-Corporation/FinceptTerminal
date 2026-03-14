// MongoDB Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class MongoDBAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"mongodb", "MongoDB", "mongodb", Category::Database, "MG",
            "Document-oriented NoSQL database", true, true, {
                {"connectionString", "Connection String", FieldType::Text, "mongodb://localhost:27017", true},
                {"database", "Database", FieldType::Text, "mydb", false},
                {"authSource", "Auth Source", FieldType::Text, "admin", false, "admin"},
                {"replicaSet", "Replica Set", FieldType::Text, "", false},
                {"ssl", "Use SSL/TLS", FieldType::Checkbox, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"connectionString"}, "MongoDB");
        if (!v.success) return v;
        return test_connection_string(
            get_or(config, "connectionString", "mongodb://localhost:27017"), "MongoDB", 27017);
    }
};

} // namespace fincept::data_sources::adapters
