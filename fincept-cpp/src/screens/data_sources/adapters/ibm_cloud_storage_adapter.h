// IBM Cloud Object Storage Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class IBMCloudStorageAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"ibm-cloud-storage", "IBM Cloud Storage", "ibm-cloud-storage", Category::Cloud, "IB",
            "IBM Cloud Object Storage", true, true, {
                {"endpoint", "Endpoint", FieldType::Url, "https://s3.us.cloud-object-storage.appdomain.cloud", true},
                {"apiKey", "API Key", FieldType::Password, "", true},
                {"serviceInstanceId", "Service Instance ID", FieldType::Text, "", true},
                {"bucket", "Bucket", FieldType::Text, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"endpoint", "apiKey"}, "IBM Cloud Storage");
        if (!v.success) return v;
        return test_http(get_or(config, "endpoint", "https://s3.us.cloud-object-storage.appdomain.cloud"), "IBM Cloud");
    }
};

} // namespace fincept::data_sources::adapters
