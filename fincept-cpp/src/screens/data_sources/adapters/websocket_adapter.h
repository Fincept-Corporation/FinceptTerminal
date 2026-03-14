// WebSocket Adapter
#pragma once
#include "base_adapter.h"

namespace fincept::data_sources::adapters {

class WebSocketAdapter : public BaseAdapter {
public:
    DataSourceDef get_definition() const override {
        return {"websocket", "WebSocket", "websocket", Category::Streaming, "WS",
            "WebSocket real-time stream", true, true, {
                {"url", "WebSocket URL", FieldType::Url, "wss://stream.example.com", true},
                {"protocols", "Subprotocols", FieldType::Text, "", false},
                {"headers", "Custom Headers (JSON)", FieldType::Textarea, "{}", false},
            }};
    }

    TestResult test_connection(const std::map<std::string, std::string>& config) override {
        auto v = validate_required(config, {"url"}, "WebSocket");
        if (!v.success) return v;
        std::string url = get_or(config, "url", "wss://localhost:443");
        int default_port = (url.substr(0, 3) == "wss") ? 443 : 80;
        auto [host, port] = parse_url_host_port(url, default_port);
        return test_tcp(host, port, "WebSocket");
    }
};

} // namespace fincept::data_sources::adapters
