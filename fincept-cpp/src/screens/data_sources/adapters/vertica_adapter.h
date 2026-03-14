// Vertica Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class VerticaAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"vertica", "Vertica", "vertica", Category::Database, "VT",
            "Columnar analytics database", true, true, {
                {"host", "Host", FieldType::Text, "localhost", true},
                {"port", "Port", FieldType::Number, "5433", true, "5433"},
                {"database", "Database", FieldType::Text, "VMart", true},
                {"username", "Username", FieldType::Text, "dbadmin", true},
                {"password", "Password", FieldType::Password, "", true},
                {"ssl", "Use SSL", FieldType::Checkbox, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"host", "database", "username"}, "Vertica");
        if (!v.success) return v;
        return test_tcp(get_or(config, "host", "localhost"),
                        get_int(config, "port", 5433), "Vertica");
    }
};

} // namespace fincept::data_sources::adapters
