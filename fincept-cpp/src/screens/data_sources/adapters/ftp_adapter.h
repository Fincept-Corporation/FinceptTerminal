// FTP Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class FTPAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"ftp", "FTP Server", "ftp", Category::File, "FP",
            "File Transfer Protocol server", true, true, {
                {"host", "Host", FieldType::Text, "", true},
                {"port", "Port", FieldType::Number, "21", true, "21"},
                {"username", "Username", FieldType::Text, "anonymous", false, "anonymous"},
                {"password", "Password", FieldType::Password, "", false},
                {"path", "Remote Path", FieldType::Text, "/", false},
                {"passive", "Passive Mode", FieldType::Checkbox, "", false, "true"},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"host"}, "FTP");
        if (!v.success) return v;
        return test_tcp(get_or(config, "host"),
                        get_int(config, "port", 21), "FTP");
    }
};

} // namespace fincept::data_sources::adapters
