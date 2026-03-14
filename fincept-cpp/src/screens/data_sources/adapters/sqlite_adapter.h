// SQLite Database Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class SQLiteAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"sqlite", "SQLite", "sqlite", Category::Database, "SL",
            "Embedded file-based database", false, true, {
                {"filepath", "Database File Path", FieldType::Text, "/path/to/database.db", true},
                {"readOnly", "Read Only", FieldType::Checkbox, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_file(get_or(config, "filepath"), "SQLite");
    }
};

} // namespace fincept::data_sources::adapters
