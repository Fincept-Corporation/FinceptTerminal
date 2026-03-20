#include "general_section.h"
#include "storage/database.h"
#include "ui/theme.h"
#include "core/config.h"
#include "core/logger.h"
#include <imgui.h>
#include <cstdio>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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
        try {
            auto all = db::ops::get_all_settings();
            json j;
            for (auto& s : all) j[s.key] = s.value;
            std::ofstream f("fincept_settings_export.json");
            f << j.dump(2);
            status_ = "Settings exported to fincept_settings_export.json";
        } catch (const std::exception& e) {
            status_ = std::string("Export failed: ") + e.what();
        }
        status_time_ = ImGui::GetTime();
    }
    ImGui::SameLine();
    if (theme::SecondaryButton("Import Settings")) {
        try {
            std::ifstream f("fincept_settings_export.json");
            if (!f.is_open()) {
                status_ = "Import failed: fincept_settings_export.json not found";
            } else {
                json j = json::parse(f);
                std::vector<std::pair<std::string, std::string>> pairs;
                for (auto& [k, v] : j.items()) {
                    if (v.is_string()) pairs.emplace_back(k, v.get<std::string>());
                }
                db::ops::storage_set_many(pairs);
                initialized_ = false; // re-init to pick up imported language
                status_ = "Settings imported (" + std::to_string(pairs.size()) + " keys)";
            }
        } catch (const std::exception& e) {
            status_ = std::string("Import failed: ") + e.what();
        }
        status_time_ = ImGui::GetTime();
    }
    ImGui::SameLine(0, 40);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ERROR_RED.x, ERROR_RED.y, ERROR_RED.z, 0.2f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ERROR_RED);
    ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
    if (ImGui::Button("Reset All Settings")) {
        confirm_reset_open_ = true;
    }
    ImGui::PopStyleColor(3);

    ImGui::EndChild();

    // ── Reset confirmation modal ───────────────────────────────────────────
    if (confirm_reset_open_) {
        ImGui::OpenPopup("Reset All Settings");
        confirm_reset_open_ = false;
    }
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Reset All Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ERROR_RED, "Reset all application settings?");
        ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "This will clear all settings, credentials,");
        ImGui::TextColored(TEXT_DIM, "LLM configs, and cached data.");
        ImGui::TextColored(TEXT_DIM, "This action cannot be undone.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ERROR_RED.x, ERROR_RED.y, ERROR_RED.z, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ERROR_RED);
        if (ImGui::Button("Yes, Reset Everything")) {
            try {
                db::ops::cache_cleanup();
                db::ops::storage_set_many({});  // no-op but signals intent
                // Clear settings, LLM configs, credentials via direct DB exec
                auto& dbref = db::Database::instance();
                dbref.exec("DELETE FROM settings");
                dbref.exec("DELETE FROM key_value_storage");
                dbref.exec("DELETE FROM llm_configs");
                dbref.exec("DELETE FROM credentials");
                dbref.exec("UPDATE llm_global_settings SET temperature=0.7, max_tokens=2048, system_prompt='' WHERE id=1");
                initialized_ = false;
                selected_language_ = 0;
                status_ = "All settings have been reset";
                status_time_ = ImGui::GetTime();
                LOG_INFO("Settings", "User performed full settings reset");
            } catch (const std::exception& e) {
                status_ = std::string("Reset failed: ") + e.what();
                status_time_ = ImGui::GetTime();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
        if (theme::SecondaryButton("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace fincept::settings
