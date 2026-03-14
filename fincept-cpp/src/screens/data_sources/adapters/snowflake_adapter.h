// Snowflake Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class SnowflakeAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"snowflake", "Snowflake", "snowflake", Category::Database, "SF",
            "Cloud data warehouse", true, true, {
                {"account", "Account Identifier", FieldType::Text, "xy12345.us-east-1", true},
                {"username", "Username", FieldType::Text, "", true},
                {"password", "Password", FieldType::Password, "", true},
                {"database", "Database", FieldType::Text, "", true},
                {"schema", "Schema", FieldType::Text, "PUBLIC", false, "PUBLIC"},
                {"warehouse", "Warehouse", FieldType::Text, "COMPUTE_WH", true},
                {"role", "Role", FieldType::Text, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto account = get_or(config, "account");
        if (account.empty()) return {false, "Snowflake: Account identifier required", 0};
        auto v = validate_required(config, {"username", "password", "database", "warehouse"}, "Snowflake");
        if (!v.success) return v;
        return test_http("https://" + account + ".snowflakecomputing.com/", "Snowflake");
    }
};

} // namespace fincept::data_sources::adapters
