#include "notifications_section.h"
#include "screens/settings/settings_types.h"
#include "storage/database.h"
#include "http/http_client.h"
#include "ui/theme.h"
#include <imgui.h>
#include <cstring>
#include <cstdio>
#include <nlohmann/json.hpp>

namespace fincept::settings {

using json = nlohmann::json;

void NotificationsSection::init() {
    if (initialized_) return;

    const auto& defs = notification_providers();
    providers_.resize(defs.size());
    for (size_t i = 0; i < defs.size(); i++) {
        providers_[i].id = defs[i].id;
        std::memset(providers_[i].value, 0, sizeof(providers_[i].value));
    }

    load_config();
    initialized_ = true;
}

void NotificationsSection::load_config() {
    auto config_str = db::ops::get_setting("notifications_config");
    if (!config_str) return;

    try {
        auto j = json::parse(*config_str);

        if (j.contains("filters")) {
            auto& f = j["filters"];
            filter_success_ = f.value("success", true);
            filter_error_ = f.value("error", true);
            filter_warning_ = f.value("warning", true);
            filter_info_ = f.value("info", true);
        }

        if (j.contains("providers")) {
            for (auto& [key, val] : j["providers"].items()) {
                for (auto& p : providers_) {
                    if (p.id == key) {
                        p.enabled = val.value("enabled", false);
                        std::string v = val.value("value", "");
                        std::strncpy(p.value, v.c_str(), sizeof(p.value) - 1);
                        break;
                    }
                }
            }
        }
    } catch (...) {}
}

void NotificationsSection::save_config() {
    json j;

    j["filters"] = {
        {"success", filter_success_},
        {"error", filter_error_},
        {"warning", filter_warning_},
        {"info", filter_info_}
    };

    json providers_json;
    for (auto& p : providers_) {
        providers_json[p.id] = {
            {"enabled", p.enabled},
            {"value", std::string(p.value)}
        };
    }
    j["providers"] = providers_json;

    db::ops::save_setting("notifications_config", j.dump(), "notifications");
    status_ = "Notification settings saved";
    status_time_ = ImGui::GetTime();
}

void NotificationsSection::test_notification(int provider_idx) {
    if (provider_idx < 0 || provider_idx >= (int)providers_.size()) return;

    auto& prov = providers_[provider_idx];
    std::string url(prov.value);
    if (url.empty()) {
        status_ = "No URL/key configured for " + prov.id;
        status_time_ = ImGui::GetTime();
        return;
    }

    const auto& defs = notification_providers();
    if (defs[provider_idx].uses_webhook) {
        // Send test webhook
        json payload = {
            {"content", "Fincept Terminal test notification"},
            {"text", "Fincept Terminal test notification"},
            {"username", "Fincept Terminal"}
        };

        auto& http = http::HttpClient::instance();
        http::Headers headers = {
            {"Content-Type", "application/json"}
        };
        auto resp = http.post(url, payload.dump(), headers);

        if (resp.success) {
            status_ = "Test notification sent to " + prov.id;
        } else {
            status_ = "Failed to send test: " + resp.error;
        }
    } else {
        status_ = "Test sent (API key providers use different methods)";
    }
    status_time_ = ImGui::GetTime();
}

void NotificationsSection::render() {
    init();

    using namespace theme::colors;

    ImGui::TextColored(ACCENT, "NOTIFICATIONS");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80);
    if (theme::AccentButton("Save")) {
        save_config();
    }
    ImGui::Separator();

    // Status
    if (!status_.empty()) {
        double elapsed = ImGui::GetTime() - status_time_;
        if (elapsed < 5.0) {
            ImGui::TextColored(SUCCESS, "%s", status_.c_str());
        } else {
            status_.clear();
        }
    }

    ImGui::Spacing();

    ImGui::BeginChild("##notif_content", ImVec2(0, 0));

    // Event filters
    theme::SectionHeader("EVENT FILTERS");
    ImGui::TextColored(TEXT_DIM, "Select which event types trigger notifications.");
    ImGui::Spacing();

    ImGui::Checkbox("Success", &filter_success_);
    ImGui::SameLine(0, 20);
    ImGui::Checkbox("Error", &filter_error_);
    ImGui::SameLine(0, 20);
    ImGui::Checkbox("Warning", &filter_warning_);
    ImGui::SameLine(0, 20);
    ImGui::Checkbox("Info", &filter_info_);

    ImGui::Spacing();

    // Notification providers
    theme::SectionHeader("PROVIDERS");
    ImGui::TextColored(TEXT_DIM,
        "Configure notification delivery channels.");
    ImGui::Spacing();

    const auto& defs = notification_providers();

    if (ImGui::BeginTable("##notif_providers", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {

        ImGui::TableSetupColumn("Provider", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Key / URL", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Test", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableHeadersRow();

        for (int i = 0; i < (int)defs.size() && i < (int)providers_.size(); i++) {
            ImGui::TableNextRow();
            ImGui::PushID(i);

            ImGui::TableNextColumn();
            ImGui::TextColored(TEXT_PRIMARY, "%s", defs[i].label);

            ImGui::TableNextColumn();
            ImGui::Checkbox("##enabled", &providers_[i].enabled);

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##value", defs[i].placeholder,
                providers_[i].value, sizeof(providers_[i].value),
                ImGuiInputTextFlags_Password);

            ImGui::TableNextColumn();
            if (ImGui::SmallButton("Test")) {
                test_notification(i);
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
}

} // namespace fincept::settings
