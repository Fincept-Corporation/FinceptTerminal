#include "credentials_section.h"
#include "screens/settings/settings_types.h"
#include "storage/database.h"
#include "ui/theme.h"
#include <imgui.h>
#include <cstring>

namespace fincept::settings {

void CredentialsSection::init() {
    if (initialized_) return;

    const auto& keys = predefined_api_keys();
    buffers_.resize(keys.size());
    for (size_t i = 0; i < keys.size(); i++) {
        buffers_[i].key_name = keys[i].key;
        std::memset(buffers_[i].value, 0, sizeof(buffers_[i].value));
    }

    load_keys();
    initialized_ = true;
}

void CredentialsSection::load_keys() {
    auto creds = db::ops::get_credentials();
    for (auto& cred : creds) {
        for (auto& buf : buffers_) {
            if (buf.key_name == cred.service_name && cred.api_key) {
                std::strncpy(buf.value, cred.api_key->c_str(),
                             sizeof(buf.value) - 1);
                buf.dirty = false;
            }
        }
    }
}

void CredentialsSection::save_all_keys() {
    int saved = 0;
    for (auto& buf : buffers_) {
        std::string val(buf.value);
        if (val.empty()) continue;

        db::Credential cred;
        cred.service_name = buf.key_name;
        cred.api_key = val;
        db::ops::save_credential(cred);
        buf.dirty = false;
        saved++;
    }

    if (saved > 0) {
        save_status_ = "Saved " + std::to_string(saved) + " API key(s)";
    } else {
        save_status_ = "No keys to save";
    }
    save_status_time_ = ImGui::GetTime();
}

void CredentialsSection::render() {
    init();

    using namespace theme::colors;
    const auto& keys = predefined_api_keys();

    // Header
    ImGui::TextColored(ACCENT, "API CREDENTIALS");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 200);

    // Toggle visibility
    if (ImGui::Checkbox("Show Values", &show_values_)) {}
    ImGui::SameLine();

    // Save all button
    if (theme::AccentButton("Save All")) {
        save_all_keys();
    }

    ImGui::Separator();
    ImGui::Spacing();

    // Status message
    if (!save_status_.empty()) {
        double elapsed = ImGui::GetTime() - save_status_time_;
        if (elapsed < 5.0) {
            ImGui::TextColored(SUCCESS, "%s", save_status_.c_str());
            ImGui::Spacing();
        } else {
            save_status_.clear();
        }
    }

    ImGui::TextColored(TEXT_DIM,
        "Configure API keys for external data providers and services.");
    ImGui::TextColored(TEXT_DIM,
        "Keys are stored locally in encrypted SQLite database.");
    ImGui::Spacing();

    // API key input fields
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));

    float label_width = 200.0f;
    ImGuiInputTextFlags flags = show_values_
        ? ImGuiInputTextFlags_None
        : ImGuiInputTextFlags_Password;

    for (size_t i = 0; i < keys.size() && i < buffers_.size(); i++) {
        ImGui::PushID(static_cast<int>(i));

        // Label
        ImGui::TextColored(TEXT_SECONDARY, "%s", keys[i].label);

        ImGui::SameLine(label_width);

        // Input field
        float input_w = ImGui::GetContentRegionAvail().x - 80;
        ImGui::SetNextItemWidth(input_w);

        char id[64];
        std::snprintf(id, sizeof(id), "##key_%s", keys[i].key);

        if (ImGui::InputTextWithHint(id, keys[i].placeholder,
                buffers_[i].value, sizeof(buffers_[i].value), flags)) {
            buffers_[i].dirty = true;
        }

        // Dirty indicator
        ImGui::SameLine();
        if (buffers_[i].dirty) {
            ImGui::TextColored(WARNING, " *");
        } else if (buffers_[i].value[0] != '\0') {
            ImGui::TextColored(SUCCESS, " OK");
        } else {
            ImGui::TextColored(TEXT_DIM, "   ");
        }

        // Clear button
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
        ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
        if (ImGui::SmallButton("X")) {
            std::memset(buffers_[i].value, 0, sizeof(buffers_[i].value));
            buffers_[i].dirty = true;
        }
        ImGui::PopStyleColor(3);

        ImGui::PopID();
    }

    ImGui::PopStyleVar();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Summary
    int configured = 0;
    int total = static_cast<int>(buffers_.size());
    for (auto& buf : buffers_) {
        if (buf.value[0] != '\0') configured++;
    }

    char summary[128];
    std::snprintf(summary, sizeof(summary),
        "%d / %d keys configured", configured, total);
    ImGui::TextColored(TEXT_DIM, "%s", summary);
}

} // namespace fincept::settings
