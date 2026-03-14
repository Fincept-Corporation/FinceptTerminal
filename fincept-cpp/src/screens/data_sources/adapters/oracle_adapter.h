// Oracle Database Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class OracleAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"oracle", "Oracle Database", "oracle", Category::Database, "OR",
            "Enterprise relational database", true, true, {
                {"host", "Host", FieldType::Text, "localhost", true},
                {"port", "Port", FieldType::Number, "1521", true, "1521"},
                {"sid", "SID / Service Name", FieldType::Text, "ORCL", true},
                {"username", "Username", FieldType::Text, "", true},
                {"password", "Password", FieldType::Password, "", true},
                {"connectString", "TNS Connect String", FieldType::Textarea, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"host", "username", "password"}, "Oracle");
        if (!v.success) return v;
        return test_tcp(get_or(config, "host", "localhost"),
                        get_int(config, "port", 1521), "Oracle");
    }
};

} // namespace fincept::data_sources::adapters
