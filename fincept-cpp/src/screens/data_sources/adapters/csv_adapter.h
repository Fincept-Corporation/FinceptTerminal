// CSV File Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class CSVAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"csv", "CSV File", "csv", Category::File, "CV",
            "Comma-separated values file", false, true, {
                {"filepath", "File Path", FieldType::Text, "/path/to/data.csv", true},
                {"delimiter", "Delimiter", FieldType::Select, "", false, ",", {
                    {",", "Comma (,)"}, {"\t", "Tab"}, {";", "Semicolon (;)"}, {"|", "Pipe (|)"},
                }},
                {"hasHeader", "Has Header Row", FieldType::Checkbox, "", false, "true"},
                {"encoding", "Encoding", FieldType::Select, "", false, "utf-8", {
                    {"utf-8", "UTF-8"}, {"ascii", "ASCII"}, {"latin-1", "Latin-1"}, {"utf-16", "UTF-16"},
                }},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_file(get_or(config, "filepath"), "CSV");
    }
};

} // namespace fincept::data_sources::adapters
