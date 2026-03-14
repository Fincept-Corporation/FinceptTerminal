// JSON File Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class JSONAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"json", "JSON File", "json", Category::File, "JS",
            "JavaScript Object Notation file", false, true, {
                {"filepath", "File Path", FieldType::Text, "/path/to/data.json", true},
                {"jsonPath", "JSON Path Expression", FieldType::Text, "$", false},
                {"format", "Format", FieldType::Select, "", false, "standard", {
                    {"standard", "Standard JSON"}, {"ndjson", "Newline-delimited JSON"},
                    {"jsonl", "JSON Lines"},
                }},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_file(get_or(config, "filepath"), "JSON");
    }
};

} // namespace fincept::data_sources::adapters
