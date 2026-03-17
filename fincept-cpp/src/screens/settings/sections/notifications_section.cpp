#include "notifications_section.h"
#include "screens/settings/settings_types.h"
#include "storage/database.h"
#include "core/notification.h"
#include "ui/theme.h"
#include <imgui.h>
#include <cstring>
#include <cstdio>
#include <nlohmann/json.hpp>
#include <thread>

namespace fincept::settings {

using json = nlohmann::json;

void NotificationsSection::init() {
    if (initialized_) return;

    const auto& defs = notification_providers();
    providers_.resize(defs.size());
    for (size_t i = 0; i < defs.size(); i++) {
        providers_[i].id = defs[i].id;
        providers_[i].enabled = false;
        // Initialize field buffers
        for (const auto& f : defs[i].fields) {
            providers_[i].fields[f.key] = "";
            providers_[i].field_bufs[f.key] = {};
        }
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
            filter_error_   = f.value("error", true);
            filter_warning_ = f.value("warning", true);
            filter_info_    = f.value("info", true);
        }

        if (j.contains("providers")) {
            for (auto& [key, val] : j["providers"].items()) {
                for (auto& p : providers_) {
                    if (p.id != key) continue;
                    p.enabled = val.value("enabled", false);

                    // Multi-field config
                    if (val.contains("config") && val["config"].is_object()) {
                        for (auto& [fk, fv] : val["config"].items()) {
                            std::string v = fv.get<std::string>();
                            p.fields[fk] = v;
                            if (p.field_bufs.count(fk)) {
                                std::strncpy(p.field_bufs[fk].data(), v.c_str(), 255);
                            }
                        }
                    }
                    // Legacy single "value" field
                    if (val.contains("value") && val["value"].is_string()) {
                        std::string v = val["value"].get<std::string>();
                        // Map to first field
                        if (!p.field_bufs.empty()) {
                            auto& first = p.field_bufs.begin()->second;
                            std::strncpy(first.data(), v.c_str(), 255);
                            p.fields[p.field_bufs.begin()->first] = v;
                        }
                    }
                    break;
                }
            }
        }
    } catch (...) {}
}

void NotificationsSection::save_config() {
    json j;

    j["filters"] = {
        {"success", filter_success_},
        {"error",   filter_error_},
        {"warning", filter_warning_},
        {"info",    filter_info_}
    };

    json providers_json;
    for (auto& p : providers_) {
        json cfg;
        for (auto& [fk, buf] : p.field_bufs) {
            p.fields[fk] = std::string(buf.data());
            cfg[fk] = p.fields[fk];
        }
        providers_json[p.id] = {
            {"enabled", p.enabled},
            {"config", cfg}
        };
    }
    j["providers"] = providers_json;

    db::ops::save_setting("notifications_config", j.dump(), "notifications");

    // Reload into the notification system so send() picks up changes
    core::notify::load_provider_config();

    status_ = "Notification settings saved";
    status_is_error_ = false;
    status_time_ = ImGui::GetTime();
}

void NotificationsSection::test_notification(int provider_idx) {
    if (provider_idx < 0 || provider_idx >= (int)providers_.size()) return;

    auto& prov = providers_[provider_idx];
    testing_provider_ = provider_idx;

    // Save first so provider config is current
    save_config();

    // Test on background thread
    std::string pid = prov.id;
    std::thread([this, pid, provider_idx]() {
        bool ok = core::notify::test_provider(pid);
        status_ = ok ? ("Test sent to " + pid)
                      : ("Test failed for " + pid);
        status_is_error_ = !ok;
        status_time_ = ImGui::GetTime();
        testing_provider_ = -1;
    }).detach();
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
            ImGui::TextColored(status_is_error_ ? MARKET_RED : SUCCESS, "%s", status_.c_str());
        } else {
            status_.clear();
        }
    }

    ImGui::Spacing();
    ImGui::BeginChild("##notif_content", ImVec2(0, 0));

    // ---- Event Filters ----
    theme::SectionHeader("EVENT FILTERS");
    ImGui::TextColored(TEXT_DIM, "Select which event types trigger external notifications.");
    ImGui::Spacing();

    // Colored filter toggles
    {
        auto color_toggle = [](const char* label, bool* val, ImVec4 color) {
            ImGui::PushStyleColor(ImGuiCol_CheckMark, color);
            ImGui::Checkbox(label, val);
            ImGui::PopStyleColor();
        };

        color_toggle("Success", &filter_success_, MARKET_GREEN);
        ImGui::SameLine(0, 20);
        color_toggle("Error", &filter_error_, MARKET_RED);
        ImGui::SameLine(0, 20);
        color_toggle("Warning", &filter_warning_, WARNING);
        ImGui::SameLine(0, 20);
        color_toggle("Info", &filter_info_, ACCENT);
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // ---- Providers ----
    theme::SectionHeader("PROVIDERS");
    ImGui::TextColored(TEXT_DIM, "Configure notification delivery channels. "
                                  "15 providers available.");
    ImGui::Spacing();

    const auto& defs = notification_providers();

    for (int i = 0; i < (int)defs.size() && i < (int)providers_.size(); i++) {
        ImGui::PushID(i);
        auto& prov = providers_[i];
        auto& def = defs[i];
        bool is_testing = (testing_provider_ == i);

        // Provider card
        ImVec4 border_col = prov.enabled ? ACCENT : BORDER_DIM;
        ImGui::PushStyleColor(ImGuiCol_Border, border_col);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
        ImGui::BeginChild(def.id, ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

        // Header: name + toggle + buttons
        {
            ImGui::TextColored(TEXT_PRIMARY, "%s", def.label);
            ImGui::SameLine(0, 8);

            if (prov.enabled) {
                ImGui::TextColored(MARKET_GREEN, "[ON]");
            } else {
                ImGui::TextColored(TEXT_DIM, "[OFF]");
            }

            // Right-side buttons
            float btn_x = ImGui::GetContentRegionAvail().x;
            if (btn_x > 160) {
                ImGui::SameLine(ImGui::GetCursorPosX() + btn_x - 160);

                ImGui::Checkbox("##enabled", &prov.enabled);
                ImGui::SameLine(0, 8);

                ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
                ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
                if (is_testing) {
                    ImGui::SmallButton("Testing...");
                } else if (ImGui::SmallButton("Test")) {
                    test_notification(i);
                }
                ImGui::PopStyleColor(2);
            }
        }

        ImGui::Spacing();

        // Config fields
        float field_w = ImGui::GetContentRegionAvail().x - 8;
        for (const auto& field : def.fields) {
            ImGui::TextColored(TEXT_DIM, "%s", field.label);
            if (field.required) {
                ImGui::SameLine(0, 4);
                ImGui::TextColored(MARKET_RED, "*");
            }

            ImGui::PushItemWidth(field_w);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);

            char input_id[64];
            std::snprintf(input_id, sizeof(input_id), "##%s_%s", def.id, field.key);

            auto& buf = prov.field_bufs[field.key];
            ImGuiInputTextFlags flags = 0;
            if (field.is_password) flags |= ImGuiInputTextFlags_Password;

            ImGui::InputTextWithHint(input_id, field.placeholder, buf.data(), buf.size(), flags);

            ImGui::PopStyleColor();
            ImGui::PopItemWidth();
            ImGui::Spacing();
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::Spacing();
        ImGui::PopID();
    }

    ImGui::EndChild();
}

} // namespace fincept::settings
