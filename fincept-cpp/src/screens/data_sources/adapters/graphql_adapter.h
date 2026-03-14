// GraphQL Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class GraphQLAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"graphql", "GraphQL", "graphql", Category::Api, "GQ",
            "GraphQL API endpoint", true, true, {
                {"endpoint", "Endpoint URL", FieldType::Url, "https://api.example.com/graphql", true},
                {"token", "Bearer Token", FieldType::Password, "", false},
                {"headers", "Custom Headers (JSON)", FieldType::Textarea, "{}", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"endpoint"}, "GraphQL");
        if (!v.success) return v;
        std::map<std::string, std::string> headers;
        auto token = get_or(config, "token");
        if (!token.empty()) headers["Authorization"] = "Bearer " + token;
        return test_http(get_or(config, "endpoint"), "GraphQL", headers);
    }
};

} // namespace fincept::data_sources::adapters
