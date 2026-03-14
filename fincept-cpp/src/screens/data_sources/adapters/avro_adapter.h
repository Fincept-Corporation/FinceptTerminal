// Apache Avro Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class AvroAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"avro", "Avro File", "avro", Category::File, "AV",
            "Apache Avro binary format", false, true, {
                {"filepath", "File Path", FieldType::Text, "/path/to/data.avro", true},
                {"schemaRegistry", "Schema Registry URL", FieldType::Url, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_file(get_or(config, "filepath"), "Avro");
    }
};

} // namespace fincept::data_sources::adapters
