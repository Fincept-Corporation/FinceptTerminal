// OData Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class ODataAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"odata", "OData", "odata", Category::Api, "OD",
            "Open Data Protocol service", true, true, {
                {"serviceRoot", "Service Root URL", FieldType::Url, "", true},
                {"version", "OData Version", FieldType::Select, "", false, "4.0", {
                    {"2.0", "v2.0"}, {"3.0", "v3.0"}, {"4.0", "v4.0"},
                }},
                {"token", "Bearer Token", FieldType::Password, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"serviceRoot"}, "OData");
        if (!v.success) return v;
        return test_http(get_or(config, "serviceRoot"), "OData");
    }
};

} // namespace fincept::data_sources::adapters
