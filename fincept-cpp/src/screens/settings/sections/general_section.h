#pragma once
// General Section — language selection, misc settings
// Mirrors LanguageSelector.tsx and general settings from the Tauri project

#include <imgui.h>
#include <string>

namespace fincept::settings {

class GeneralSection {
public:
    void render();

private:
    bool initialized_ = false;
    int selected_language_ = 0;

    // Status
    std::string status_;
    double status_time_ = 0;

    // Confirmation modal
    bool confirm_reset_open_ = false;

    void init();
};

} // namespace fincept::settings
