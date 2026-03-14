// Parquet File Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class ParquetAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"parquet", "Parquet File", "parquet", Category::File, "PQ",
            "Apache Parquet columnar format", false, true, {
                {"filepath", "File Path", FieldType::Text, "/path/to/data.parquet", true},
                {"columns", "Columns (comma-separated)", FieldType::Text, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_file(get_or(config, "filepath"), "Parquet");
    }
};

} // namespace fincept::data_sources::adapters
