#pragma once
// LLM Configuration Section — provider selection, model management, global settings
// Mirrors LLMConfigSection.tsx and sub-components from the Tauri project

#include <imgui.h>
#include <string>
#include <vector>
#include "storage/database.h"

namespace fincept::settings {

class LLMConfigSection {
public:
    void render();

private:
    enum class SubPanel { Providers, GlobalSettings, ModelLibrary };

    bool initialized_ = false;
    SubPanel active_panel_ = SubPanel::Providers;

    // Provider configs
    std::vector<db::LLMConfig> configs_;
    int selected_provider_ = 0;

    // Edit buffers for selected provider
    char edit_api_key_[256] = {};
    char edit_base_url_[256] = {};
    char edit_model_[128] = {};

    // Global settings
    db::LLMGlobalSettings global_settings_;
    char edit_system_prompt_[4096] = {};

    // Status
    std::string status_;
    double status_time_ = 0;

    void init();
    void load_configs();
    void load_global_settings();
    void populate_edit_buffers();

    void render_sub_tabs();
    void render_providers();
    void render_fincept_panel();
    void render_global_settings();
    void render_model_library();

    void save_provider_config();
    void save_global_settings();
    void ensure_fincept_config();
};

} // namespace fincept::settings
