// Amazon Redshift Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class RedshiftAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"redshift", "Amazon Redshift", "redshift", Category::Database, "RS",
            "AWS data warehouse", true, true, {
                {"host", "Host", FieldType::Text, "cluster.region.redshift.amazonaws.com", true},
                {"port", "Port", FieldType::Number, "5439", true, "5439"},
                {"database", "Database", FieldType::Text, "dev", true},
                {"username", "Username", FieldType::Text, "awsuser", true},
                {"password", "Password", FieldType::Password, "", true},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"host", "database", "username", "password"}, "Redshift");
        if (!v.success) return v;
        return test_tcp(get_or(config, "host"),
                        get_int(config, "port", 5439), "Redshift");
    }
};

} // namespace fincept::data_sources::adapters
