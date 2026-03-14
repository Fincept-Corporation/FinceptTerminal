// Microsoft Graph Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class MSGraphAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"ms-graph", "Microsoft Graph", "ms-graph", Category::Api, "MG",
            "Microsoft 365 API gateway", true, true, {
                {"tenantId", "Tenant ID", FieldType::Text, "", true},
                {"clientId", "Client ID", FieldType::Text, "", true},
                {"clientSecret", "Client Secret", FieldType::Password, "", true},
                {"scope", "Scope", FieldType::Text, "https://graph.microsoft.com/.default", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"tenantId", "clientId", "clientSecret"}, "Microsoft Graph");
        if (!v.success) return v;
        return test_http("https://graph.microsoft.com/v1.0/$metadata", "Microsoft Graph");
    }
};

} // namespace fincept::data_sources::adapters
