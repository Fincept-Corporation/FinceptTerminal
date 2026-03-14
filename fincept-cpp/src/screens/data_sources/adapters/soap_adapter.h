// SOAP Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class SOAPAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"soap", "SOAP API", "soap", Category::Api, "SO",
            "SOAP/XML web service", true, true, {
                {"wsdlUrl", "WSDL URL", FieldType::Url, "", true},
                {"username", "Username", FieldType::Text, "", false},
                {"password", "Password", FieldType::Password, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"wsdlUrl"}, "SOAP");
        if (!v.success) return v;
        return test_http(get_or(config, "wsdlUrl"), "SOAP");
    }
};

} // namespace fincept::data_sources::adapters
