// etcd Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class EtcdAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"etcd", "etcd", "etcd", Category::Database, "ET",
            "Distributed key-value store", true, true, {
                {"hosts", "Hosts", FieldType::Text, "localhost:2379", true},
                {"username", "Username", FieldType::Text, "", false},
                {"password", "Password", FieldType::Password, "", false},
                {"ssl", "Use SSL", FieldType::Checkbox, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto [host, port] = parse_host_port(get_or(config, "hosts", "localhost:2379"), 2379);
        return test_http("http://" + host + ":" + std::to_string(port) + "/health", "etcd");
    }
};

} // namespace fincept::data_sources::adapters
