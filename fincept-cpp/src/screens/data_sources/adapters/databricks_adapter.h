// Databricks Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class DatabricksAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"databricks", "Databricks", "databricks", Category::Database, "DB",
            "Unified analytics platform", true, true, {
                {"serverHostname", "Server Hostname", FieldType::Text, "your-workspace.cloud.databricks.com", true},
                {"httpPath", "HTTP Path", FieldType::Text, "/sql/1.0/warehouses/...", true},
                {"accessToken", "Personal Access Token", FieldType::Password, "dapi...", true},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"serverHostname", "accessToken"}, "Databricks");
        if (!v.success) return v;
        return test_http("https://" + get_or(config, "serverHostname") + "/api/2.0/clusters/list",
                         "Databricks");
    }
};

} // namespace fincept::data_sources::adapters
