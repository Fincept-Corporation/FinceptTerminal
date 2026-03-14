// Oracle Cloud Object Storage Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class OracleCloudStorageAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"oracle-cloud-storage", "Oracle Cloud Storage", "oracle-cloud-storage", Category::Cloud, "OC",
            "Oracle Cloud Infrastructure storage", true, true, {
                {"namespace", "Namespace", FieldType::Text, "", true},
                {"region", "Region", FieldType::Text, "us-phoenix-1", true, "us-phoenix-1"},
                {"tenancyOCID", "Tenancy OCID", FieldType::Text, "", true},
                {"userOCID", "User OCID", FieldType::Text, "", true},
                {"fingerprint", "API Key Fingerprint", FieldType::Text, "", true},
                {"privateKey", "Private Key (PEM)", FieldType::Textarea, "", true},
                {"bucket", "Bucket", FieldType::Text, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"namespace", "region", "tenancyOCID"}, "Oracle Cloud");
        if (!v.success) return v;
        return test_http("https://objectstorage." + get_or(config, "region", "us-phoenix-1") + ".oraclecloud.com/", "Oracle Cloud");
    }
};

} // namespace fincept::data_sources::adapters
