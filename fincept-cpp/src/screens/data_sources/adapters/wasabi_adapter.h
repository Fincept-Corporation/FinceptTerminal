// Wasabi Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class WasabiAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"wasabi", "Wasabi", "wasabi", Category::Cloud, "WA",
            "Hot cloud storage", true, true, {
                {"accessKeyId", "Access Key", FieldType::Text, "", true},
                {"secretAccessKey", "Secret Key", FieldType::Password, "", true},
                {"region", "Region", FieldType::Text, "us-east-1", true, "us-east-1"},
                {"bucket", "Bucket", FieldType::Text, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"accessKeyId", "secretAccessKey"}, "Wasabi");
        if (!v.success) return v;
        return test_http("https://s3." + get_or(config, "region", "us-east-1") + ".wasabisys.com/", "Wasabi");
    }
};

} // namespace fincept::data_sources::adapters
