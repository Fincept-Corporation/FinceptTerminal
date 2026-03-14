// Azure Blob Storage Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class AzureBlobAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"azure-blob", "Azure Blob Storage", "azure-blob", Category::Cloud, "AZ",
            "Microsoft Azure object storage", true, true, {
                {"accountName", "Account Name", FieldType::Text, "", true},
                {"accountKey", "Account Key", FieldType::Password, "", true},
                {"container", "Container Name", FieldType::Text, "", false},
                {"connectionString", "Connection String", FieldType::Textarea, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto account = get_or(config, "accountName");
        if (account.empty()) return {false, "Azure Blob: Account name required", 0};
        return test_http("https://" + account + ".blob.core.windows.net/", "Azure Blob");
    }
};

} // namespace fincept::data_sources::adapters
