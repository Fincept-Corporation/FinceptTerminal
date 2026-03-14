// Apache Solr Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class SolrAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"solr", "Apache Solr", "solr", Category::Database, "SL",
            "Enterprise search platform", true, true, {
                {"host", "Solr URL", FieldType::Url, "http://localhost:8983/solr", true},
                {"core", "Core / Collection", FieldType::Text, "", false},
                {"username", "Username", FieldType::Text, "", false},
                {"password", "Password", FieldType::Password, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_http(get_or(config, "host", "http://localhost:8983/solr") + "/admin/info/system", "Solr");
    }
};

} // namespace fincept::data_sources::adapters
