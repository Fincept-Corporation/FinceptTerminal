// Google Cloud Storage Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class GCPStorageAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"google-cloud-storage", "Google Cloud Storage", "google-cloud-storage", Category::Cloud, "GC",
            "Google Cloud object storage", true, true, {
                {"projectId", "Project ID", FieldType::Text, "", true},
                {"credentials", "Service Account JSON", FieldType::Textarea, "Paste JSON key", true},
                {"bucket", "Default Bucket", FieldType::Text, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"projectId", "credentials"}, "GCS");
        if (!v.success) return v;
        return test_http("https://storage.googleapis.com/", "Google Cloud Storage");
    }
};

} // namespace fincept::data_sources::adapters
