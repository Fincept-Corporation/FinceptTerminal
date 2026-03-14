#pragma once
// Notifications Section — notification provider configuration
// Mirrors NotificationsSection.tsx from the Tauri project

#include <imgui.h>
#include <string>
#include <vector>

namespace fincept::settings {

class NotificationsSection {
public:
    void render();

private:
    struct ProviderConfig {
        std::string id;
        bool enabled = false;
        char value[256] = {};  // API key or webhook URL
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

    void init();
    void load_config();
    void save_config();
    void test_notification(int provider_idx);
};

} // namespace fincept::settings
