// Google Firestore Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class FirestoreAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"firestore", "Firestore", "firestore", Category::Database, "FS",
            "Google Cloud document database", true, true, {
                {"projectId", "Project ID", FieldType::Text, "my-project", true},
                {"credentials", "Service Account JSON", FieldType::Textarea, "Paste JSON key", true},
                {"collection", "Default Collection", FieldType::Text, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto project = get_or(config, "projectId");
        if (project.empty()) return {false, "Firestore: Project ID required", 0};
        return test_http("https://firestore.googleapis.com/v1/projects/" + project +
                         "/databases/(default)/documents", "Firestore");
    }
};

} // namespace fincept::data_sources::adapters
