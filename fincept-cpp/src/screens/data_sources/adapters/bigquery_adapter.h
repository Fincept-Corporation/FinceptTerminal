// Google BigQuery Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class BigQueryAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"bigquery", "Google BigQuery", "bigquery", Category::Database, "BQ",
            "Serverless data warehouse", true, true, {
                {"projectId", "Project ID", FieldType::Text, "my-project", true},
                {"dataset", "Dataset", FieldType::Text, "my_dataset", false},
                {"credentials", "Service Account JSON", FieldType::Textarea, "Paste JSON credentials", true},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"projectId", "credentials"}, "BigQuery");
        if (!v.success) return v;
        return test_http("https://bigquery.googleapis.com/bigquery/v2/projects/" +
                         get_or(config, "projectId") + "/datasets", "BigQuery");
    }
};

} // namespace fincept::data_sources::adapters
