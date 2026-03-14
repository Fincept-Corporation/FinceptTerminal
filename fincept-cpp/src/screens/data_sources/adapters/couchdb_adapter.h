// CouchDB Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class CouchDBAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"couchdb", "CouchDB", "couchdb", Category::Database, "CD",
            "Document-oriented NoSQL with HTTP API", true, true, {
                {"url", "URL", FieldType::Url, "http://localhost:5984", true},
                {"username", "Username", FieldType::Text, "admin", false},
                {"password", "Password", FieldType::Password, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_http(get_or(config, "url", "http://localhost:5984"), "CouchDB");
    }
};

} // namespace fincept::data_sources::adapters
