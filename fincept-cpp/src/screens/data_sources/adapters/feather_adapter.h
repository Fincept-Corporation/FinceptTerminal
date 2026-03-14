// Apache Feather/Arrow Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class FeatherAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"feather", "Feather File", "feather", Category::File, "FT",
            "Apache Arrow IPC format", false, true, {
                {"filepath", "File Path", FieldType::Text, "/path/to/data.feather", true},
                {"columns", "Columns (comma-separated)", FieldType::Text, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_file(get_or(config, "filepath"), "Feather");
    }
};

} // namespace fincept::data_sources::adapters
