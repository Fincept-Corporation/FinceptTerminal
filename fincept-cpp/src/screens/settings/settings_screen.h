#pragma once
// Settings Screen — main container with sidebar navigation and section content
// Mirrors SettingsTab.tsx from the Tauri project
// Owns all section instances as members (following App pattern)

#include <imgui.h>
#include "settings_types.h"
#include "sections/credentials_section.h"
#include "sections/llm_config_section.h"
#include "sections/terminal_appearance_section.h"
#include "sections/storage_cache_section.h"
#include "sections/notifications_section.h"
#include "sections/general_section.h"

namespace fincept::settings {

class SettingsScreen {
public:
    void render();

private:
    bool initialized_ = false;
    Section active_section_ = Section::Credentials;

    // Section instances (owned by this screen)
    CredentialsSection credentials_;
    LLMConfigSection llm_config_;
    TerminalAppearanceSection appearance_;
    StorageCacheSection storage_;
    NotificationsSection notifications_;
    GeneralSection general_;

    // Status bar message
    StatusMessage status_;

    void init();
    void render_sidebar(float width, float height);
    void render_content(float width, float height);
    void render_status_bar();
};

} // namespace fincept::settings
