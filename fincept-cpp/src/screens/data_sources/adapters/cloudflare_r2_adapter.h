// Cloudflare R2 Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class CloudflareR2Adapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"cloudflare-r2", "Cloudflare R2", "cloudflare-r2", Category::Cloud, "R2",
            "Cloudflare S3-compatible storage", true, true, {
                {"accountId", "Account ID", FieldType::Text, "", true},
                {"accessKeyId", "Access Key ID", FieldType::Text, "", true},
                {"secretAccessKey", "Secret Access Key", FieldType::Password, "", true},
                {"bucket", "Bucket", FieldType::Text, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"accountId", "accessKeyId", "secretAccessKey"}, "Cloudflare R2");
        if (!v.success) return v;
        return test_http("https://api.cloudflare.com/client/v4/user/tokens/verify", "Cloudflare R2");
    }
};

} // namespace fincept::data_sources::adapters
