// XML File Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class XMLAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"xml", "XML File", "xml", Category::File, "XM",
            "Extensible Markup Language file", false, true, {
                {"filepath", "File Path", FieldType::Text, "/path/to/data.xml", true},
                {"xpath", "XPath Expression", FieldType::Text, "/", false},
                {"rootElement", "Root Element", FieldType::Text, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_file(get_or(config, "filepath"), "XML");
    }
};

} // namespace fincept::data_sources::adapters
