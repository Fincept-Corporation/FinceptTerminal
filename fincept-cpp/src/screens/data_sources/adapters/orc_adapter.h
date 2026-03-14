// Apache ORC Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class ORCAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"orc", "ORC File", "orc", Category::File, "OC",
            "Optimized Row Columnar format", false, true, {
                {"filepath", "File Path", FieldType::Text, "/path/to/data.orc", true},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_file(get_or(config, "filepath"), "ORC");
    }
};

} // namespace fincept::data_sources::adapters
