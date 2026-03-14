// Amazon S3 Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class S3Adapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"aws-s3", "Amazon S3", "aws-s3", Category::Cloud, "S3",
            "AWS object storage", true, true, {
                {"accessKeyId", "Access Key ID", FieldType::Text, "", true},
                {"secretAccessKey", "Secret Access Key", FieldType::Password, "", true},
                {"region", "Region", FieldType::Text, "us-east-1", true, "us-east-1"},
                {"bucket", "Bucket Name", FieldType::Text, "", false},
                {"endpoint", "Custom Endpoint", FieldType::Url, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"accessKeyId", "secretAccessKey", "region"}, "AWS S3");
        if (!v.success) return v;
        auto endpoint = get_or(config, "endpoint");
        if (!endpoint.empty()) return test_http(endpoint, "S3 (custom endpoint)");
        return test_http("https://s3." + get_or(config, "region", "us-east-1") + ".amazonaws.com/", "AWS S3");
    }
};

} // namespace fincept::data_sources::adapters
