// Neo4j Graph Database Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class Neo4jAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"neo4j", "Neo4j", "neo4j", Category::Database, "N4",
            "Graph database", true, true, {
                {"uri", "URI", FieldType::Text, "bolt://localhost:7687", true},
                {"username", "Username", FieldType::Text, "neo4j", true},
                {"password", "Password", FieldType::Password, "", true},
                {"database", "Database", FieldType::Text, "neo4j", false, "neo4j"},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"uri", "username", "password"}, "Neo4j");
        if (!v.success) return v;
        auto [host, port] = parse_url_host_port(get_or(config, "uri", "bolt://localhost:7687"), 7687);
        return test_tcp(host, port, "Neo4j");
    }
};

} // namespace fincept::data_sources::adapters
