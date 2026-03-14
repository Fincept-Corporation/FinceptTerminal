// gRPC Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class GRPCAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"grpc", "gRPC", "grpc", Category::Api, "GR",
            "gRPC remote procedure call", true, true, {
                {"host", "Host", FieldType::Text, "localhost", true},
                {"port", "Port", FieldType::Number, "50051", true, "50051"},
                {"tls", "Use TLS", FieldType::Checkbox, "", false},
                {"protoPath", "Proto File Path", FieldType::Text, "", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"host"}, "gRPC");
        if (!v.success) return v;
        return test_tcp(get_or(config, "host", "localhost"),
                        get_int(config, "port", 50051), "gRPC");
    }
};

} // namespace fincept::data_sources::adapters
