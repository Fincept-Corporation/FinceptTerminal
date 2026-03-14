// Excel File Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class ExcelAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"excel", "Excel File", "excel", Category::File, "XL",
            "Microsoft Excel spreadsheet", false, true, {
                {"filepath", "File Path", FieldType::Text, "/path/to/data.xlsx", true},
                {"sheet", "Sheet Name", FieldType::Text, "Sheet1", false},
                {"range", "Cell Range", FieldType::Text, "", false},
                {"hasHeader", "Has Header Row", FieldType::Checkbox, "", false, "true"},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_file(get_or(config, "filepath"), "Excel");
    }
};

} // namespace fincept::data_sources::adapters
