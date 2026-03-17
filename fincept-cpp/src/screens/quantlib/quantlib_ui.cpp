// QuantLib UI — Bloomberg-style header, sidebar, status bar
#include "quantlib_screen.h"
#include <imgui.h>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <cctype>

namespace fincept::quantlib {

// ============================================================================
// Header Bar — Bloomberg-style top bar with title, search, stats
// ============================================================================
void QuantLibScreen::render_header(float width) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bb::BG_HEADER);
    ImGui::PushStyleColor(ImGuiCol_Border, bb::BORDER_DIM);
    ImGui::BeginChild("##ql_header", ImVec2(width, 36), true);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // Amber accent line at top
    dl->AddRectFilled(ImVec2(p.x - 8, p.y - 8), ImVec2(p.x + width, p.y - 6),
        IM_COL32(255, 166, 0, 200));

    // Title block
    ImGui::TextColored(bb::AMBER_BRIGHT, "QUANTLIB");
    ImGui::SameLine();
    ImGui::TextColored(bb::GRAY_DIM, "|");
    ImGui::SameLine();

    // Endpoint count
    int total_eps = 0;
    for (auto& m : modules_) total_eps += m.endpoint_count;
    char stats_buf[64];
    std::snprintf(stats_buf, sizeof(stats_buf), "%d ENDPOINTS", total_eps);
    ImGui::TextColored(bb::GREEN, "%s", stats_buf);
    ImGui::SameLine();
    ImGui::TextColored(bb::GRAY_DIM, "|");
    ImGui::SameLine();

    // Module count
    std::snprintf(stats_buf, sizeof(stats_buf), "%d MODULES", (int)modules_.size());
    ImGui::TextColored(bb::AMBER_DIM, "%s", stats_buf);

    // Available width inside the header child for right-aligned items
    float avail = ImGui::GetContentRegionAvail().x;

    // Search box — right-aligned (only if enough room)
    if (avail > 280) {
        ImGui::SameLine(width - 260);
        ImGui::TextColored(bb::GRAY_DIM, "SEARCH:");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_FrameBg, bb::BG_INPUT);
        ImGui::PushStyleColor(ImGuiCol_Border, bb::BORDER);
        ImGui::PushStyleColor(ImGuiCol_Text, bb::AMBER);
        ImGui::SetNextItemWidth(std::min(160.0f, avail - 140.0f));
        ImGui::InputText("##ql_search", search_buf_, sizeof(search_buf_));
        ImGui::PopStyleColor(3);
    }

    // Raw JSON toggle — far right (only if enough room)
    if (avail > 100) {
        ImGui::SameLine(width - 80);
        ImGui::PushStyleColor(ImGuiCol_Button, show_raw_json_ ? bb::BTN_BG : bb::BTN_SEC_BG);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bb::BTN_HOVER);
        ImGui::PushStyleColor(ImGuiCol_Text, show_raw_json_ ? bb::GREEN : bb::GRAY);
        if (ImGui::SmallButton(show_raw_json_ ? "RAW:ON" : "RAW:OFF")) {
            show_raw_json_ = !show_raw_json_;
        }
        ImGui::PopStyleColor(3);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}

// ============================================================================
// Sidebar — collapsible module tree with search filtering
// ============================================================================
void QuantLibScreen::render_sidebar(float /*height*/) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));

    std::string filter;
    if (search_buf_[0] != '\0') {
        filter = search_buf_;
        std::transform(filter.begin(), filter.end(), filter.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });
    }

    for (auto& mod : modules_) {
        // Filter: skip module if no sections match
        bool any_match = filter.empty();
        if (!any_match) {
            std::string mod_lower = mod.label;
            std::transform(mod_lower.begin(), mod_lower.end(), mod_lower.begin(),
                [](unsigned char c) { return (char)std::tolower(c); });
            if (mod_lower.find(filter) != std::string::npos) {
                any_match = true;
            } else {
                for (auto& sec : mod.sections) {
                    std::string sec_lower = sec.label;
                    std::transform(sec_lower.begin(), sec_lower.end(), sec_lower.begin(),
                        [](unsigned char c) { return (char)std::tolower(c); });
                    if (sec_lower.find(filter) != std::string::npos) {
                        any_match = true;
                        break;
                    }
                }
            }
        }
        if (!any_match) continue;

        bool is_expanded = expanded_modules_.count(mod.id) > 0;

        // Module header button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bb::BG_HOVER);

        char mod_btn[128];
        std::snprintf(mod_btn, sizeof(mod_btn), "%s %s  %d##mod_%s",
            is_expanded ? "v" : ">", mod.label.c_str(), mod.endpoint_count, mod.id.c_str());

        // Amber text for module headers
        ImGui::PushStyleColor(ImGuiCol_Text, bb::AMBER);
        if (ImGui::Button(mod_btn, ImVec2(-1, 22))) {
            if (is_expanded) expanded_modules_.erase(mod.id);
            else expanded_modules_.insert(mod.id);
        }
        ImGui::PopStyleColor(3);

        // Section items
        if (is_expanded) {
            for (auto& sec : mod.sections) {
                // Filter sections
                if (!filter.empty()) {
                    std::string sec_lower = sec.label;
                    std::transform(sec_lower.begin(), sec_lower.end(), sec_lower.begin(),
                        [](unsigned char c) { return (char)std::tolower(c); });
                    std::string mod_lower = mod.label;
                    std::transform(mod_lower.begin(), mod_lower.end(), mod_lower.begin(),
                        [](unsigned char c) { return (char)std::tolower(c); });
                    if (sec_lower.find(filter) == std::string::npos &&
                        mod_lower.find(filter) == std::string::npos) continue;
                }

                bool active = (sec.id == active_section_);
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 cursor = ImGui::GetCursorScreenPos();

                // Active indicator: amber left bar
                if (active) {
                    dl->AddRectFilled(
                        ImVec2(cursor.x, cursor.y),
                        ImVec2(cursor.x + 3, cursor.y + 20),
                        IM_COL32(255, 166, 0, 255));
                }

                ImGui::PushStyleColor(ImGuiCol_Button,
                    active ? bb::BG_CARD : ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bb::BG_HOVER);
                ImGui::PushStyleColor(ImGuiCol_Text,
                    active ? bb::GREEN : bb::WHITE);

                char sec_btn[128];
                std::snprintf(sec_btn, sizeof(sec_btn), "   %s##sec_%s",
                    sec.label.c_str(), sec.id.c_str());

                if (ImGui::Button(sec_btn, ImVec2(-1, 20))) {
                    active_section_ = sec.id;
                }
                ImGui::PopStyleColor(3);
            }

            // Dim separator between modules
            ImDrawList* dl2 = ImGui::GetWindowDrawList();
            ImVec2 sep_p = ImGui::GetCursorScreenPos();
            dl2->AddLine(
                ImVec2(sep_p.x + 8, sep_p.y),
                ImVec2(sep_p.x + 210, sep_p.y),
                IM_COL32(50, 50, 60, 150), 1.0f);
            ImGui::Dummy(ImVec2(0, 2));
        }
    }

    ImGui::PopStyleVar();
}

// ============================================================================
// Status Bar — Bloomberg-style bottom bar with stats
// ============================================================================
void QuantLibScreen::render_status_bar(float width) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bb::BG_HEADER);
    ImGui::PushStyleColor(ImGuiCol_Border, bb::BORDER_DIM);
    ImGui::BeginChild("##ql_status", ImVec2(width, 22), true);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // Amber line at bottom
    dl->AddRectFilled(ImVec2(p.x - 8, p.y + 12), ImVec2(p.x + width, p.y + 14),
        IM_COL32(255, 166, 0, 80));

    // API status
    ImGui::TextColored(bb::GREEN_DIM, "API");
    ImGui::SameLine();
    ImGui::TextColored(bb::GREEN, "ONLINE");
    ImGui::SameLine();
    ImGui::TextColored(bb::GRAY_DIM, " | ");
    ImGui::SameLine();

    // Call count
    char buf[64];
    std::snprintf(buf, sizeof(buf), "CALLS: %d", total_calls_);
    ImGui::TextColored(bb::AMBER_DIM, "%s", buf);
    ImGui::SameLine();
    ImGui::TextColored(bb::GRAY_DIM, " | ");
    ImGui::SameLine();

    // Average latency
    double avg = total_calls_ > 0 ? total_latency_ / total_calls_ : 0;
    std::snprintf(buf, sizeof(buf), "AVG: %.0fms", avg);
    ImGui::TextColored(avg < 500 ? bb::GREEN_DIM : bb::AMBER_DIM, "%s", buf);
    ImGui::SameLine();
    ImGui::TextColored(bb::GRAY_DIM, " | ");
    ImGui::SameLine();

    // Active section
    ImGui::TextColored(bb::GRAY, "SECTION:");
    ImGui::SameLine();
    ImGui::TextColored(bb::AMBER, "%s", active_section_.c_str());

    // Loading indicator — right-aligned using available content width
    int loading_count = 0;
    for (auto& [id, state] : endpoints_) {
        if (state.loading) loading_count++;
    }
    if (loading_count > 0) {
        static const char* spinner[] = {"|", "/", "-", "\\"};
        int frame = (int)(ImGui::GetTime() / 0.15f) % 4;
        std::snprintf(buf, sizeof(buf), "%s %d RUNNING", spinner[frame], loading_count);
        float label_w = ImGui::CalcTextSize(buf).x + 8;
        float avail = ImGui::GetContentRegionAvail().x;
        if (avail > label_w) {
            ImGui::SameLine(ImGui::GetCursorPosX() + avail - label_w);
        } else {
            ImGui::SameLine();
        }
        ImGui::TextColored(bb::RUNNING, "%s", buf);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}

} // namespace fincept::quantlib
