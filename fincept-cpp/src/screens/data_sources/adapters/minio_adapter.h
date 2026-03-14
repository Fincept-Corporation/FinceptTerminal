// MinIO Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class MinIOAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"minio", "MinIO", "minio", Category::Cloud, "MI",
            "Self-hosted S3-compatible storage", true, true, {
                {"endpoint", "Endpoint", FieldType::Url, "http://localhost:9000", true},
                {"accessKey", "Access Key", FieldType::Text, "minioadmin", true},
                {"secretKey", "Secret Key", FieldType::Password, "minioadmin", true},
                {"bucket", "Default Bucket", FieldType::Text, "", false},
                {"ssl", "Use SSL", FieldType::Checkbox, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"endpoint", "accessKey", "secretKey"}, "MinIO");
        if (!v.success) return v;
        return test_http(get_or(config, "endpoint", "http://localhost:9000") + "/minio/health/live", "MinIO");
    }
};

} // namespace fincept::data_sources::adapters
