// QuantLib Helpers — Bloomberg-style endpoint cards, inputs, result rendering
#include "quantlib_screen.h"
#include <imgui.h>
#include <cstring>
#include <cstdio>
#include <sstream>
#include <chrono>

namespace fincept::quantlib {

// ============================================================================
// Endpoint Card — Bloomberg panel with amber header, expand/collapse
// ============================================================================

bool QuantLibScreen::begin_endpoint_card(const std::string& id, const char* title,
                                           const char* description) {
    auto& state = ep(id);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, bb::BG_CARD);
    ImGui::PushStyleColor(ImGuiCol_Border, bb::BORDER_DIM);

    char child_id[128];
    std::snprintf(child_id, sizeof(child_id), "##epc_%s", id.c_str());

    float card_h = state.expanded ? 0 : 28;
    if (!state.expanded) {
        ImGui::BeginChild(child_id, ImVec2(-1, card_h), true);
    } else {
        ImGui::BeginChild(child_id, ImVec2(-1, 0), true,
            ImGuiWindowFlags_AlwaysAutoResize);
    }

    // Header — amber text, full-width button
    ImVec2 header_pos = ImGui::GetCursorScreenPos();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bb::BG_HOVER);
    ImGui::PushStyleColor(ImGuiCol_Text, bb::AMBER);

    char btn_label[256];
    std::snprintf(btn_label, sizeof(btn_label), "%s %s##hdr_%s",
        state.expanded ? "v" : ">", title, id.c_str());
    if (ImGui::Button(btn_label, ImVec2(-1, 20))) {
        state.expanded = !state.expanded;
    }
    ImGui::PopStyleColor(3);

    // Tooltip for description
    if (description && ImGui::IsItemHovered()) {
        ImGui::PushStyleColor(ImGuiCol_PopupBg, bb::BG_PANEL);
        ImGui::SetTooltip("%s", description);
        ImGui::PopStyleColor();
    }

    // Running indicator
    if (state.loading) {
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 70);
        static const char* spinner[] = {"|", "/", "-", "\\"};
        int frame = (int)(ImGui::GetTime() / 0.12f) % 4;
        char run_buf[32];
        std::snprintf(run_buf, sizeof(run_buf), "%s RUNNING", spinner[frame]);
        ImGui::TextColored(bb::RUNNING, "%s", run_buf);
    }

    // Left amber accent bar on expanded cards
    if (state.expanded) {
        dl->AddRectFilled(
            ImVec2(header_pos.x - 8, header_pos.y - 4),
            ImVec2(header_pos.x - 5, header_pos.y + ImGui::GetCursorScreenPos().y - header_pos.y + 200),
            IM_COL32(255, 166, 0, 60));
    }

    ImGui::PopStyleColor(2); // ChildBg, Border

    if (!state.expanded) {
        ImGui::EndChild();
        ImGui::Spacing();
        return false;
    }

    // Thin separator after header
    ImVec2 sep_p = ImGui::GetCursorScreenPos();
    dl->AddLine(ImVec2(sep_p.x, sep_p.y), ImVec2(sep_p.x + ImGui::GetContentRegionAvail().x, sep_p.y),
        IM_COL32(255, 166, 0, 40), 1.0f);
    ImGui::Dummy(ImVec2(0, 4));

    return true;
}

void QuantLibScreen::end_endpoint_card() {
    ImGui::EndChild();
    ImGui::Spacing();
}

// ============================================================================
// Input Components — Bloomberg-style labeled inputs
// ============================================================================

void QuantLibScreen::input_float(const std::string& ep_id, const char* label,
                                   const char* field, const char* default_val, float width) {
    auto& state = ep(ep_id);
    auto& val = state.buf(field, default_val);

    ImGui::BeginGroup();
    ImGui::TextColored(bb::GRAY, "%s", label);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, bb::BG_INPUT);
    ImGui::PushStyleColor(ImGuiCol_Border, bb::BORDER);
    ImGui::PushStyleColor(ImGuiCol_Text, bb::GREEN);
    ImGui::SetNextItemWidth(width);

    char input_id[128];
    std::snprintf(input_id, sizeof(input_id), "##%s_%s", ep_id.c_str(), field);

    char buf[64];
    std::strncpy(buf, val.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    if (ImGui::InputText(input_id, buf, sizeof(buf))) {
        val = buf;
    }
    ImGui::PopStyleColor(3);
    ImGui::EndGroup();
    ImGui::SameLine();
}

void QuantLibScreen::input_text(const std::string& ep_id, const char* label,
                                  const char* field, const char* default_val, float width) {
    input_float(ep_id, label, field, default_val, width);
}

void QuantLibScreen::input_int(const std::string& ep_id, const char* label,
                                 const char* field, const char* default_val, float width) {
    input_float(ep_id, label, field, default_val, width);
}

void QuantLibScreen::input_array(const std::string& ep_id, const char* label,
                                   const char* field, const char* default_val, float width) {
    auto& state = ep(ep_id);
    auto& val = state.buf(field, default_val);

    ImGui::BeginGroup();
    ImGui::TextColored(bb::GRAY, "%s", label);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, bb::BG_INPUT);
    ImGui::PushStyleColor(ImGuiCol_Border, bb::BORDER);
    ImGui::PushStyleColor(ImGuiCol_Text, bb::CYAN);
    ImGui::SetNextItemWidth(width);

    char input_id[128];
    std::snprintf(input_id, sizeof(input_id), "##%s_%s", ep_id.c_str(), field);

    char buf[256];
    std::strncpy(buf, val.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    if (ImGui::InputText(input_id, buf, sizeof(buf))) {
        val = buf;
    }
    ImGui::PopStyleColor(3);
    ImGui::EndGroup();
    ImGui::SameLine();
}

void QuantLibScreen::input_select(const std::string& ep_id, const char* label,
                                    const char* field, const std::vector<const char*>& options,
                                    float width) {
    auto& state = ep(ep_id);
    auto& val = state.buf(field, options.empty() ? "" : options[0]);

    ImGui::BeginGroup();
    ImGui::TextColored(bb::GRAY, "%s", label);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, bb::BG_INPUT);
    ImGui::PushStyleColor(ImGuiCol_Border, bb::BORDER);
    ImGui::PushStyleColor(ImGuiCol_Text, bb::AMBER);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, bb::BG_PANEL);
    ImGui::SetNextItemWidth(width);

    char combo_id[128];
    std::snprintf(combo_id, sizeof(combo_id), "##%s_%s", ep_id.c_str(), field);

    if (ImGui::BeginCombo(combo_id, val.c_str())) {
        for (auto* opt : options) {
            bool selected = (val == opt);
            ImGui::PushStyleColor(ImGuiCol_Text, selected ? bb::GREEN : bb::WHITE);
            if (ImGui::Selectable(opt, selected)) {
                val = opt;
            }
            ImGui::PopStyleColor();
        }
        ImGui::EndCombo();
    }
    ImGui::PopStyleColor(4);
    ImGui::EndGroup();
    ImGui::SameLine();
}

// ============================================================================
// Run Button — Bloomberg green action button
// ============================================================================

bool QuantLibScreen::run_button(const std::string& ep_id) {
    auto& state = ep(ep_id);

    char btn_id[128];
    std::snprintf(btn_id, sizeof(btn_id), "GO##run_%s", ep_id.c_str());

    ImGui::NewLine();
    if (state.loading) {
        ImGui::PushStyleColor(ImGuiCol_Button, bb::BG_CARD);
        ImGui::PushStyleColor(ImGuiCol_Text, bb::GRAY_DIM);
        static const char* spinner[] = {"|", "/", "-", "\\"};
        int frame = (int)(ImGui::GetTime() / 0.12f) % 4;
        char lbl[32];
        std::snprintf(lbl, sizeof(lbl), "%s RUN##dis_%s", spinner[frame], ep_id.c_str());
        ImGui::Button(lbl, ImVec2(80, 0));
        ImGui::PopStyleColor(2);
        return false;
    }

    ImGui::PushStyleColor(ImGuiCol_Button, bb::BTN_BG);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bb::BTN_HOVER);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, bb::BTN_ACTIVE);
    ImGui::PushStyleColor(ImGuiCol_Text, bb::WHITE);
    bool clicked = ImGui::Button(btn_id, ImVec2(80, 0));
    ImGui::PopStyleColor(4);
    return clicked;
}

// ============================================================================
// Result Display — Smart JSON parsing with table/kv views + raw toggle
// ============================================================================

void QuantLibScreen::render_result(const std::string& ep_id) {
    auto& state = ep(ep_id);
    if (!state.has_result && state.error.empty()) return;

    ImGui::Spacing();

    // Error display
    if (!state.error.empty()) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        dl->AddRectFilled(p, ImVec2(p.x + ImGui::GetContentRegionAvail().x, p.y + 30),
            IM_COL32(255, 40, 40, 20));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::BeginChild(("##err_" + ep_id).c_str(), ImVec2(-1, 30), false);
        ImGui::TextColored(bb::ERROR_COL, "ERROR: %s", state.error.c_str());
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    if (!state.has_result) return;

    // Latency + copy button
    ImGui::TextColored(bb::GRAY_DIM, "%.0fms", state.elapsed_ms);
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, bb::BTN_SEC_BG);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bb::BTN_SEC_HOVER);
    ImGui::PushStyleColor(ImGuiCol_Text, bb::GRAY);
    if (ImGui::SmallButton(("COPY##cp_" + ep_id).c_str())) {
        std::string text = state.result_data.dump(2);
        ImGui::SetClipboardText(text.c_str());
    }
    ImGui::PopStyleColor(3);

    // Result content
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bb::BG_BLACK);
    float max_h = 250.0f;

    if (show_raw_json_) {
        // Raw JSON view
        std::string json_text = state.result_data.dump(2);
        float text_h = std::min(max_h, ImGui::CalcTextSize(json_text.c_str()).y + 16);
        ImGui::BeginChild(("##res_" + ep_id).c_str(), ImVec2(-1, text_h), true);
        ImGui::PushStyleColor(ImGuiCol_Text, bb::GREEN);
        ImGui::TextWrapped("%s", json_text.c_str());
        ImGui::PopStyleColor();
        ImGui::EndChild();
    } else {
        // Smart parsed view
        ImGui::BeginChild(("##res_" + ep_id).c_str(), ImVec2(-1, 0), true,
            ImGuiWindowFlags_AlwaysAutoResize);
        render_result_table(state.result_data);
        ImGui::EndChild();
    }

    ImGui::PopStyleColor();
}

// ============================================================================
// Smart Result Renderers
// ============================================================================

void QuantLibScreen::render_result_table(const json& data) {
    if (data.is_object()) {
        render_result_kv(data);
    } else if (data.is_array()) {
        render_result_array_table(data);
    } else {
        // Primitive value
        ImGui::TextColored(bb::GREEN, "%s", data.dump().c_str());
    }
}

void QuantLibScreen::render_result_kv(const json& data, int depth) {
    if (!data.is_object()) {
        ImGui::TextColored(bb::GREEN, "%s", data.dump().c_str());
        return;
    }

    // Key-value table
    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                            ImGuiTableFlags_SizingStretchProp;

    char table_id[32];
    std::snprintf(table_id, sizeof(table_id), "##kv_%d_%p", depth, (void*)&data);

    if (ImGui::BeginTable(table_id, 2, flags)) {
        ImGui::TableSetupColumn("Key", 0, 1.2f);
        ImGui::TableSetupColumn("Value", 0, 2.0f);

        for (auto& [key, val] : data.items()) {
            ImGui::TableNextRow();

            // Key column
            ImGui::TableNextColumn();
            ImGui::TextColored(bb::AMBER_DIM, "%s", key.c_str());

            // Value column
            ImGui::TableNextColumn();
            if (val.is_number_float()) {
                double v = val.get<double>();
                ImVec4 col = bb::WHITE;
                if (v > 0) col = bb::POSITIVE;
                else if (v < 0) col = bb::NEGATIVE;
                ImGui::TextColored(col, "%.6g", v);
            } else if (val.is_number_integer()) {
                ImGui::TextColored(bb::WHITE, "%lld", (long long)val.get<int64_t>());
            } else if (val.is_boolean()) {
                ImGui::TextColored(val.get<bool>() ? bb::GREEN : bb::RED,
                    "%s", val.get<bool>() ? "true" : "false");
            } else if (val.is_string()) {
                ImGui::TextColored(bb::CYAN, "%s", val.get<std::string>().c_str());
            } else if (val.is_null()) {
                ImGui::TextColored(bb::GRAY_DIM, "null");
            } else if (val.is_object()) {
                if (val.size() <= 6 && depth < 2) {
                    render_result_kv(val, depth + 1);
                } else {
                    ImGui::TextColored(bb::GREEN, "%s", val.dump().c_str());
                }
            } else if (val.is_array()) {
                if (val.size() <= 20) {
                    // Compact array display
                    std::string arr_str;
                    for (size_t i = 0; i < val.size(); i++) {
                        if (i > 0) arr_str += ", ";
                        if (val[i].is_number()) {
                            char num_buf[32];
                            std::snprintf(num_buf, sizeof(num_buf), "%.6g",
                                val[i].get<double>());
                            arr_str += num_buf;
                        } else {
                            arr_str += val[i].dump();
                        }
                    }
                    ImGui::TextColored(bb::CYAN, "[%s]", arr_str.c_str());
                } else {
                    ImGui::TextColored(bb::GRAY, "[%d items]", (int)val.size());
                }
            }
        }

        ImGui::EndTable();
    }
}

void QuantLibScreen::render_result_array_table(const json& arr) {
    if (arr.empty()) {
        ImGui::TextColored(bb::GRAY_DIM, "[]");
        return;
    }

    // If array of objects with same keys — render as table
    if (arr[0].is_object()) {
        // Collect all keys from first element
        std::vector<std::string> keys;
        for (auto& [k, v] : arr[0].items()) {
            keys.push_back(k);
        }

        int ncols = (int)keys.size();
        if (ncols > 0 && ncols <= 12) {
            ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                                    ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;

            if (ImGui::BeginTable("##arr_tbl", ncols, flags, ImVec2(0, 200))) {
                for (auto& k : keys) {
                    ImGui::TableSetupColumn(k.c_str());
                }
                // Header row in amber
                ImGui::PushStyleColor(ImGuiCol_Text, bb::AMBER);
                ImGui::TableHeadersRow();
                ImGui::PopStyleColor();

                for (auto& row : arr) {
                    ImGui::TableNextRow();
                    for (auto& k : keys) {
                        ImGui::TableNextColumn();
                        if (row.contains(k)) {
                            auto& v = row[k];
                            if (v.is_number_float()) {
                                double val = v.get<double>();
                                ImGui::TextColored(val >= 0 ? bb::POSITIVE : bb::NEGATIVE,
                                    "%.6g", val);
                            } else if (v.is_string()) {
                                ImGui::TextColored(bb::WHITE, "%s",
                                    v.get<std::string>().c_str());
                            } else {
                                ImGui::TextColored(bb::GREEN, "%s", v.dump().c_str());
                            }
                        }
                    }
                }
                ImGui::EndTable();
            }
            return;
        }
    }

    // Array of primitives — render as numbered list
    if (arr[0].is_number()) {
        int ncols = std::min((int)arr.size(), 8);
        int nrows = ((int)arr.size() + ncols - 1) / ncols;

        ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                                ImGuiTableFlags_SizingFixedFit;

        if (ImGui::BeginTable("##arr_num", ncols, flags)) {
            for (int r = 0; r < nrows; r++) {
                ImGui::TableNextRow();
                for (int c = 0; c < ncols; c++) {
                    int idx = r * ncols + c;
                    ImGui::TableNextColumn();
                    if (idx < (int)arr.size()) {
                        if (arr[idx].is_number_float()) {
                            double v = arr[idx].get<double>();
                            ImGui::TextColored(v >= 0 ? bb::POSITIVE : bb::NEGATIVE,
                                "%.6g", v);
                        } else {
                            ImGui::TextColored(bb::WHITE, "%s", arr[idx].dump().c_str());
                        }
                    }
                }
            }
            ImGui::EndTable();
        }
        return;
    }

    // Fallback: dump
    ImGui::TextColored(bb::GREEN, "%s", arr.dump(2).c_str());
}

// ============================================================================
// Fire API calls
// ============================================================================

void QuantLibScreen::fire_post(const std::string& ep_id, const std::string& api_path,
                                 const json& body) {
    auto& state = ep(ep_id);
    if (state.loading) return;

    state.loading = true;
    state.has_result = false;
    state.error.clear();
    state.future = QuantLibApi::instance().post(api_path, body);
}

void QuantLibScreen::fire_get(const std::string& ep_id, const std::string& api_path,
                                const std::map<std::string, std::string>& params) {
    auto& state = ep(ep_id);
    if (state.loading) return;

    state.loading = true;
    state.has_result = false;
    state.error.clear();
    state.future = QuantLibApi::instance().get(api_path, params);
}

void QuantLibScreen::poll_futures() {
    for (auto& [id, state] : endpoints_) {
        if (state.loading && state.future.valid() &&
            state.future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            auto result = state.future.get();
            state.loading = false;
            state.elapsed_ms = result.elapsed_ms;
            if (result.success) {
                state.result_data = result.data;
                state.has_result = true;
                state.error.clear();
            } else {
                state.error = result.error;
                state.has_result = false;
            }
            // Track stats
            total_calls_++;
            total_latency_ += result.elapsed_ms;
        }
    }
}

} // namespace fincept::quantlib
