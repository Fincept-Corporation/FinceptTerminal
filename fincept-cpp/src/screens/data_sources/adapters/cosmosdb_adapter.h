// Azure Cosmos DB Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class CosmosDBAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"cosmosdb", "Cosmos DB", "cosmosdb", Category::Database, "CM",
            "Azure globally distributed database", true, true, {
                {"endpoint", "Endpoint URL", FieldType::Url, "https://myaccount.documents.azure.com", true},
                {"key", "Primary Key", FieldType::Password, "", true},
                {"database", "Database", FieldType::Text, "", true},
                {"container", "Container", FieldType::Text, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"endpoint", "key"}, "Cosmos DB");
        if (!v.success) return v;
        return test_http(get_or(config, "endpoint", "https://localhost:8081"), "Cosmos DB");
    }
};

} // namespace fincept::data_sources::adapters
