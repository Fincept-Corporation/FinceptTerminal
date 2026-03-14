// Amazon DynamoDB Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class DynamoDBAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"dynamodb", "DynamoDB", "dynamodb", Category::Database, "DY",
            "AWS managed NoSQL database", true, true, {
                {"region", "AWS Region", FieldType::Text, "us-east-1", true},
                {"accessKeyId", "Access Key ID", FieldType::Text, "", true},
                {"secretAccessKey", "Secret Access Key", FieldType::Password, "", true},
                {"endpoint", "Custom Endpoint (local)", FieldType::Url, "", false},
                {"tableName", "Table Name", FieldType::Text, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"region", "accessKeyId", "secretAccessKey"}, "DynamoDB");
        if (!v.success) return v;
        std::string endpoint = get_or(config, "endpoint");
        if (!endpoint.empty())
            return test_http(endpoint, "DynamoDB (local)");
        return test_http("https://dynamodb." + get_or(config, "region", "us-east-1") + ".amazonaws.com/", "DynamoDB");
    }
};

} // namespace fincept::data_sources::adapters
