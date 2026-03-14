// Microsoft SQL Server Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class MSSQLAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"mssql", "SQL Server", "mssql", Category::Database, "MS",
            "Microsoft SQL Server", true, true, {
                {"host", "Host", FieldType::Text, "localhost", true},
                {"port", "Port", FieldType::Number, "1433", true, "1433"},
                {"database", "Database", FieldType::Text, "master", true},
                {"username", "Username", FieldType::Text, "sa", true},
                {"password", "Password", FieldType::Password, "", true},
                {"encrypt", "Encrypt Connection", FieldType::Checkbox, "", false},
                {"trustServerCertificate", "Trust Server Certificate", FieldType::Checkbox, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"host", "database", "username", "password"}, "SQL Server");
        if (!v.success) return v;
        return test_tcp(get_or(config, "host", "localhost"),
                        get_int(config, "port", 1433), "SQL Server");
    }
};

} // namespace fincept::data_sources::adapters
