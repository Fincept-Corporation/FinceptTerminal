// Azure Synapse Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class AzureSynapseAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"synapse", "Azure Synapse", "synapse", Category::Database, "SY",
            "Azure analytics service", true, true, {
                {"server", "Server", FieldType::Text, "workspace.sql.azuresynapse.net", true},
                {"database", "Database", FieldType::Text, "sqlpool", true},
                {"username", "Username", FieldType::Text, "sqladmin", true},
                {"password", "Password", FieldType::Password, "", true},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"server", "database", "username", "password"}, "Azure Synapse");
        if (!v.success) return v;
        return test_tcp(get_or(config, "server"), 1433, "Azure Synapse");
    }
};

} // namespace fincept::data_sources::adapters
