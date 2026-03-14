#include "llm_config_section.h"
#include "screens/settings/settings_types.h"
#include "ui/theme.h"
#include <imgui.h>
#include <cstring>
#include <cstdio>

namespace fincept::settings {

void LLMConfigSection::init() {
    if (initialized_) return;
    load_configs();
    load_global_settings();
    initialized_ = true;
}

void LLMConfigSection::load_configs() {
    configs_ = db::ops::get_llm_configs();
}

void LLMConfigSection::load_global_settings() {
    global_settings_ = db::ops::get_llm_global_settings();
    std::strncpy(edit_system_prompt_, global_settings_.system_prompt.c_str(),
                 sizeof(edit_system_prompt_) - 1);
}

void LLMConfigSection::populate_edit_buffers() {
    std::memset(edit_api_key_, 0, sizeof(edit_api_key_));
    std::memset(edit_base_url_, 0, sizeof(edit_base_url_));
    std::memset(edit_model_, 0, sizeof(edit_model_));

    const auto& providers = llm_providers();
    if (selected_provider_ < 0 || selected_provider_ >= (int)providers.size())
        return;

    std::string prov_name = providers[selected_provider_];

    for (auto& cfg : configs_) {
        if (cfg.provider == prov_name) {
            if (cfg.api_key)
                std::strncpy(edit_api_key_, cfg.api_key->c_str(), sizeof(edit_api_key_) - 1);
            if (cfg.base_url)
                std::strncpy(edit_base_url_, cfg.base_url->c_str(), sizeof(edit_base_url_) - 1);
            std::strncpy(edit_model_, cfg.model.c_str(), sizeof(edit_model_) - 1);
            break;
        }
    }
}

void LLMConfigSection::save_provider_config() {
    const auto& providers = llm_providers();
    if (selected_provider_ < 0 || selected_provider_ >= (int)providers.size())
        return;

    db::LLMConfig cfg;
    cfg.provider = providers[selected_provider_];
    cfg.api_key = std::string(edit_api_key_);
    cfg.base_url = std::string(edit_base_url_);
    cfg.model = std::string(edit_model_);
    cfg.is_active = false;

    db::ops::save_llm_config(cfg);
    load_configs();

    status_ = "Provider config saved";
    status_time_ = ImGui::GetTime();
}

void LLMConfigSection::save_global_settings() {
    global_settings_.system_prompt = std::string(edit_system_prompt_);
    db::ops::save_llm_global_settings(global_settings_);

    status_ = "Global settings saved";
    status_time_ = ImGui::GetTime();
}

// ============================================================================
// Sub-tab navigation
// ============================================================================
void LLMConfigSection::render_sub_tabs() {
    using namespace theme::colors;

    struct Tab { SubPanel panel; const char* label; };
    Tab tabs[] = {
        {SubPanel::Providers, "PROVIDERS"},
        {SubPanel::GlobalSettings, "GLOBAL SETTINGS"},
        {SubPanel::ModelLibrary, "MODEL LIBRARY"},
    };

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));

    for (int i = 0; i < 3; i++) {
        bool active = (active_panel_ == tabs[i].panel);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ACCENT_HOVER);
            ImGui::PushStyleColor(ImGuiCol_Text, BG_DARKEST);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
        }

        if (ImGui::Button(tabs[i].label)) {
            active_panel_ = tabs[i].panel;
        }
        ImGui::PopStyleColor(3);
        if (i < 2) ImGui::SameLine();
    }

    ImGui::PopStyleVar(2);
    ImGui::Separator();
    ImGui::Spacing();
}

// ============================================================================
// Providers panel
// ============================================================================
void LLMConfigSection::render_providers() {
    using namespace theme::colors;
    const auto& providers = llm_providers();

    // Provider list (left) + config (right)
    float avail_w = ImGui::GetContentRegionAvail().x;
    float list_w = 200.0f;
    float config_w = avail_w - list_w - 8;

    // Provider list
    ImGui::BeginChild("##prov_list", ImVec2(list_w, 0), ImGuiChildFlags_Borders);

    ImGui::TextColored(ACCENT, "PROVIDERS");
    ImGui::Separator();

    for (int i = 0; i < (int)providers.size(); i++) {
        bool is_selected = (i == selected_provider_);

        // Check if this provider is configured
        bool is_configured = false;
        bool is_active = false;
        for (auto& cfg : configs_) {
            if (cfg.provider == providers[i]) {
                is_configured = true;
                is_active = cfg.is_active;
                break;
            }
        }

        // Status indicator
        if (is_active) {
            ImGui::PushStyleColor(ImGuiCol_Text, SUCCESS);
            ImGui::Bullet();
            ImGui::PopStyleColor();
            ImGui::SameLine();
        } else if (is_configured) {
            ImGui::PushStyleColor(ImGuiCol_Text, INFO);
            ImGui::Bullet();
            ImGui::PopStyleColor();
            ImGui::SameLine();
        }

        if (ImGui::Selectable(providers[i], is_selected)) {
            selected_provider_ = i;
            populate_edit_buffers();
        }
    }

    ImGui::EndChild();
    ImGui::SameLine(0, 8);

    // Provider config panel
    ImGui::BeginChild("##prov_config", ImVec2(config_w, 0), ImGuiChildFlags_Borders);

    if (selected_provider_ >= 0 && selected_provider_ < (int)providers.size()) {
        char header[128];
        std::snprintf(header, sizeof(header), "CONFIGURE: %s", providers[selected_provider_]);
        ImGui::TextColored(ACCENT, "%s", header);
        ImGui::Separator();
        ImGui::Spacing();

        float label_w = 100.0f;
        float input_w = ImGui::GetContentRegionAvail().x - label_w - 8;

        // API Key
        ImGui::TextColored(TEXT_SECONDARY, "API Key");
        ImGui::SameLine(label_w);
        ImGui::SetNextItemWidth(input_w);
        ImGui::InputText("##llm_api_key", edit_api_key_, sizeof(edit_api_key_),
                         ImGuiInputTextFlags_Password);

        // Base URL
        ImGui::TextColored(TEXT_SECONDARY, "Base URL");
        ImGui::SameLine(label_w);
        ImGui::SetNextItemWidth(input_w);
        ImGui::InputTextWithHint("##llm_base_url", "https://api.openai.com/v1",
                                 edit_base_url_, sizeof(edit_base_url_));

        // Model
        ImGui::TextColored(TEXT_SECONDARY, "Model");
        ImGui::SameLine(label_w);
        ImGui::SetNextItemWidth(input_w);
        ImGui::InputTextWithHint("##llm_model", "gpt-4o",
                                 edit_model_, sizeof(edit_model_));

        ImGui::Spacing();

        // Save & Set Active buttons
        if (theme::AccentButton("Save Config")) {
            save_provider_config();
        }
        ImGui::SameLine();
        if (theme::SecondaryButton("Set as Active")) {
            db::ops::set_active_llm_provider(providers[selected_provider_]);
            load_configs();
            status_ = std::string(providers[selected_provider_]) + " set as active";
            status_time_ = ImGui::GetTime();
        }
    }

    ImGui::EndChild();
}

// ============================================================================
// Global settings panel
// ============================================================================
void LLMConfigSection::render_global_settings() {
    using namespace theme::colors;

    ImGui::TextColored(ACCENT, "GLOBAL LLM SETTINGS");
    ImGui::Separator();
    ImGui::Spacing();

    float label_w = 120.0f;

    // Temperature
    ImGui::TextColored(TEXT_SECONDARY, "Temperature");
    ImGui::SameLine(label_w);
    float temp = static_cast<float>(global_settings_.temperature);
    ImGui::SetNextItemWidth(200);
    if (ImGui::SliderFloat("##llm_temp", &temp, 0.0f, 2.0f, "%.2f")) {
        global_settings_.temperature = temp;
    }
    ImGui::SameLine();
    ImGui::TextColored(TEXT_DIM, "(0=deterministic, 2=creative)");

    // Max tokens
    ImGui::TextColored(TEXT_SECONDARY, "Max Tokens");
    ImGui::SameLine(label_w);
    int tokens = static_cast<int>(global_settings_.max_tokens);
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputInt("##llm_tokens", &tokens, 100, 1000)) {
        if (tokens < 1) tokens = 1;
        if (tokens > 128000) tokens = 128000;
        global_settings_.max_tokens = tokens;
    }

    ImGui::Spacing();

    // System prompt
    ImGui::TextColored(TEXT_SECONDARY, "System Prompt");
    float avail_h = ImGui::GetContentRegionAvail().y - 50;
    if (avail_h < 100) avail_h = 100;
    ImGui::InputTextMultiline("##llm_sysprompt", edit_system_prompt_,
        sizeof(edit_system_prompt_), ImVec2(-1, avail_h));

    ImGui::Spacing();
    if (theme::AccentButton("Save Global Settings")) {
        save_global_settings();
    }
}

// ============================================================================
// Model library panel (placeholder for custom model management)
// ============================================================================
void LLMConfigSection::render_model_library() {
    using namespace theme::colors;

    ImGui::TextColored(ACCENT, "MODEL LIBRARY");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(TEXT_DIM, "Manage custom model configurations.");
    ImGui::Spacing();

    // Show configured models from all providers
    if (configs_.empty()) {
        ImGui::TextColored(TEXT_DIM, "No models configured yet. Add providers first.");
        return;
    }

    // Table of configured providers/models
    if (ImGui::BeginTable("##model_table", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {

        ImGui::TableSetupColumn("Provider", ImGuiTableColumnFlags_WidthFixed, 140);
        ImGui::TableSetupColumn("Model", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Base URL", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Active", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableHeadersRow();

        for (int i = 0; i < (int)configs_.size(); i++) {
            auto& cfg = configs_[i];
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_PRIMARY, "%s", cfg.provider.c_str());

            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_SECONDARY, "%s", cfg.model.c_str());

            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_DIM, "%s",
                cfg.base_url.value_or("default").c_str());

            ImGui::TableNextColumn();
            if (cfg.is_active) {
                ImGui::TextColored(SUCCESS, "YES");
            } else {
                ImGui::TextColored(TEXT_DIM, "no");
            }

            ImGui::TableNextColumn();
            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
            ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
            if (ImGui::SmallButton("Delete")) {
                // Delete by saving empty config — or just remove from DB
                // For now, re-save with empty values
            }
            ImGui::PopStyleColor(3);
            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

// ============================================================================
// Main render
// ============================================================================
void LLMConfigSection::render() {
    init();

    using namespace theme::colors;

    ImGui::TextColored(ACCENT, "LLM CONFIGURATION");
    ImGui::Spacing();

    // Status message
    if (!status_.empty()) {
        double elapsed = ImGui::GetTime() - status_time_;
        if (elapsed < 5.0) {
            ImGui::TextColored(SUCCESS, "%s", status_.c_str());
        } else {
            status_.clear();
        }
    }

    render_sub_tabs();

    switch (active_panel_) {
        case SubPanel::Providers:      render_providers(); break;
        case SubPanel::GlobalSettings: render_global_settings(); break;
        case SubPanel::ModelLibrary:   render_model_library(); break;
    }
}

} // namespace fincept::settings
