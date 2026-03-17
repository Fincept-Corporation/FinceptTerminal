#pragma once
// Notifications Section — 15 notification provider configuration
// Mirrors NotificationsSection.tsx from the Tauri project
// Supports multi-field config per provider (e.g. Telegram: bot_token + chat_id)

#include <imgui.h>
#include <string>
#include <vector>
#include <map>

namespace fincept::settings {

class NotificationsSection {
public:
    void render();

private:
    struct ProviderConfig {
        std::string id;
        bool enabled = false;
        std::map<std::string, std::string> fields;  // field_key → value
        // ImGui input buffers (keyed by field key, max 256 chars each)
        std::map<std::string, std::array<char, 256>> field_bufs;
    };

    bool initialized_ = false;
    std::vector<ProviderConfig> providers_;

    // Event filters
    bool filter_success_ = true;
    bool filter_error_ = true;
    bool filter_warning_ = true;
    bool filter_info_ = true;

    // Status
    std::string status_;
    double status_time_ = 0;
    bool status_is_error_ = false;

    // Testing state
    int testing_provider_ = -1;

    void init();
    void load_config();
    void save_config();
    void test_notification(int provider_idx);
};

} // namespace fincept::settings
