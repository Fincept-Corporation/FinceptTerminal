#include "general_section.h"
#include "storage/database.h"
#include "ui/theme.h"
#include "core/config.h"
#include <imgui.h>
#include <cstdio>

namespace fincept::settings {

// Supported languages — mirrors i18n config
static const struct { const char* label; const char* code; } LANGUAGES[] = {
    {"English",    "en"},
    {"Spanish",    "es"},
    {"French",     "fr"},
    {"German",     "de"},
    {"Chinese",    "zh-CN"},
    {"Japanese",   "ja"},
    {"Korean",     "ko"},
    {"Hindi",      "hi"},
};
static constexpr int NUM_LANGUAGES = sizeof(LANGUAGES) / sizeof(LANGUAGES[0]);

void GeneralSection::init() {
    if (initialized_) return;

    auto lang = db::ops::get_setting("language");
    if (lang) {
        for (int i = 0; i < NUM_LANGUAGES; i++) {
            if (*lang == LANGUAGES[i].code) {
                selected_language_ = i;
                break;
            }
        }
    }

    initialized_ = true;
}

void GeneralSection::render() {
    init();

    using namespace theme::colors;

    ImGui::TextColored(ACCENT, "GENERAL SETTINGS");
    ImGui::Separator();
    ImGui::Spacing();

    // Status
    if (!status_.empty()) {
        double elapsed = ImGui::GetTime() - status_time_;
        if (elapsed < 5.0) {
            ImGui::TextColored(SUCCESS, "%s", status_.c_str());
        } else {
            status_.clear();
        }
    }

    ImGui::BeginChild("##general_content", ImVec2(0, 0));

    // Language
    theme::SectionHeader("LANGUAGE");
    ImGui::TextColored(TEXT_DIM,
        "Select the interface language. Changes apply on restart.");
    ImGui::Spacing();

    ImGui::TextColored(TEXT_SECONDARY, "Language");
    ImGui::SameLine(120);
    ImGui::SetNextItemWidth(250);

    if (ImGui::BeginCombo("##language", LANGUAGES[selected_language_].label)) {
        for (int i = 0; i < NUM_LANGUAGES; i++) {
            bool selected = (i == selected_language_);
            if (ImGui::Selectable(LANGUAGES[i].label, selected)) {
                selected_language_ = i;
                db::ops::save_setting("language", LANGUAGES[i].code, "general");
                status_ = std::string("Language set to ") + LANGUAGES[i].label;
                status_time_ = ImGui::GetTime();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Spacing();

    // About
    theme::SectionHeader("ABOUT");

    ImGui::TextColored(TEXT_SECONDARY, "Application");
    ImGui::SameLine(120);
    ImGui::TextColored(TEXT_PRIMARY, "%s", config::APP_NAME);

    ImGui::TextColored(TEXT_SECONDARY, "Version");
    ImGui::SameLine(120);
    ImGui::TextColored(SUCCESS, "v%s", config::APP_VERSION);

    ImGui::TextColored(TEXT_SECONDARY, "Build");
    ImGui::SameLine(120);
    ImGui::TextColored(TEXT_DIM, "C++ / ImGui / OpenGL");

    ImGui::TextColored(TEXT_SECONDARY, "Database");
    ImGui::SameLine(120);
    ImGui::TextColored(TEXT_DIM, "SQLite (WAL mode)");

    ImGui::Spacing();

    // Data management
    theme::SectionHeader("DATA MANAGEMENT");

    ImGui::TextColored(TEXT_DIM,
        "Export or reset application data.");
    ImGui::Spacing();

    if (theme::SecondaryButton("Export Settings")) {
        status_ = "Export not yet implemented";
        status_time_ = ImGui::GetTime();
    }
    ImGui::SameLine();
    if (theme::SecondaryButton("Import Settings")) {
        status_ = "Import not yet implemented";
        status_time_ = ImGui::GetTime();
    }
    ImGui::SameLine(0, 40);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ERROR_RED.x, ERROR_RED.y, ERROR_RED.z, 0.2f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ERROR_RED);
    ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
    if (ImGui::Button("Reset All Settings")) {
        // TODO: confirmation dialog
        status_ = "Reset requires confirmation (not yet implemented)";
        status_time_ = ImGui::GetTime();
    }
    ImGui::PopStyleColor(3);

    ImGui::EndChild();
}

} // namespace fincept::settings
