// SFTP Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class SFTPAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"sftp", "SFTP Server", "sftp", Category::File, "SF",
            "SSH File Transfer Protocol", true, true, {
                {"host", "Host", FieldType::Text, "", true},
                {"port", "Port", FieldType::Number, "22", true, "22"},
                {"username", "Username", FieldType::Text, "", true},
                {"password", "Password", FieldType::Password, "", false},
                {"privateKey", "Private Key Path", FieldType::Text, "", false},
                {"path", "Remote Path", FieldType::Text, "/", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"host", "username"}, "SFTP");
        if (!v.success) return v;
        return test_tcp(get_or(config, "host"),
                        get_int(config, "port", 22), "SFTP");
    }
};

} // namespace fincept::data_sources::adapters
