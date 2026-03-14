#pragma once
// Credentials Section — API key management for external services
// Mirrors CredentialsSection.tsx from the Tauri project

#include <imgui.h>
#include <string>
#include <vector>

namespace fincept::settings {

class CredentialsSection {
public:
    void render();

private:
    struct KeyBuffer {
        std::string key_name;
        char value[256] = {};
        bool dirty = false;
    };

    bool initialized_ = false;
    std::vector<KeyBuffer> buffers_;
    bool show_values_ = false;
    std::string save_status_;
    double save_status_time_ = 0;

    void init();
    void load_keys();
    void save_all_keys();
};

} // namespace fincept::settings
