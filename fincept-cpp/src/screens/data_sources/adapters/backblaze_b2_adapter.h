// Backblaze B2 Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class BackblazeB2Adapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"backblaze-b2", "Backblaze B2", "backblaze-b2", Category::Cloud, "B2",
            "Affordable cloud storage", true, true, {
                {"applicationKeyId", "Application Key ID", FieldType::Text, "", true},
                {"applicationKey", "Application Key", FieldType::Password, "", true},
                {"bucket", "Bucket Name", FieldType::Text, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"applicationKeyId", "applicationKey"}, "Backblaze B2");
        if (!v.success) return v;
        return test_http("https://api.backblazeb2.com/b2api/v2/b2_authorize_account", "Backblaze B2");
    }
};

} // namespace fincept::data_sources::adapters
