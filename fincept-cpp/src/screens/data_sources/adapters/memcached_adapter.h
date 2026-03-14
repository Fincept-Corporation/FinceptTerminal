// Memcached Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class MemcachedAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"memcached", "Memcached", "memcached", Category::Database, "MC",
            "Distributed memory caching system", false, true, {
                {"servers", "Servers (host:port)", FieldType::Text, "localhost:11211", true},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto [host, port] = parse_host_port(get_or(config, "servers", "localhost:11211"), 11211);
        return test_tcp(host, port, "Memcached");
    }
};

} // namespace fincept::data_sources::adapters
