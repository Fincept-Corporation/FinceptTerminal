// Apache HBase Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class HBaseAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"hbase", "HBase", "hbase", Category::Database, "HB",
            "Hadoop-based column store", true, true, {
                {"zkQuorum", "ZooKeeper Quorum", FieldType::Text, "localhost", true},
                {"zkPort", "ZooKeeper Port", FieldType::Number, "2181", true, "2181"},
                {"namespace", "Namespace", FieldType::Text, "default", false, "default"},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        return test_tcp(get_or(config, "zkQuorum", "localhost"),
                        get_int(config, "zkPort", 2181), "HBase (ZooKeeper)");
    }
};

} // namespace fincept::data_sources::adapters
