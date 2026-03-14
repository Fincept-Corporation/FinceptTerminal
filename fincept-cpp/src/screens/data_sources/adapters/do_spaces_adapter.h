// DigitalOcean Spaces Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class DOSpacesAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"digitalocean-spaces", "DigitalOcean Spaces", "digitalocean-spaces", Category::Cloud, "DO",
            "DigitalOcean S3-compatible storage", true, true, {
                {"accessKeyId", "Access Key", FieldType::Text, "", true},
                {"secretAccessKey", "Secret Key", FieldType::Password, "", true},
                {"region", "Region", FieldType::Text, "nyc3", true, "nyc3"},
                {"bucket", "Space Name", FieldType::Text, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"accessKeyId", "secretAccessKey", "region"}, "DO Spaces");
        if (!v.success) return v;
        return test_http("https://" + get_or(config, "region", "nyc3") + ".digitaloceanspaces.com/", "DO Spaces");
    }
};

} // namespace fincept::data_sources::adapters
