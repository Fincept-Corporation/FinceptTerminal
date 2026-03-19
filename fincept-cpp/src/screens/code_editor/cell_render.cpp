#include "code_editor_screen.h"
#include "editor_internal.h"
#include "ui/theme.h"
#include "ui/yoga_helpers.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <cstdio>
#include <cstring>

namespace fincept::code_editor {

// =============================================================================
// All cells scroll area
// =============================================================================
void CodeEditorScreen::render_cells() {
    using namespace theme::colors;

    float status_h = 24.0f;
    float avail_h = ImGui::GetContentRegionAvail().y - status_h;

    ImGui::BeginChild("##ce_cells_area", ImVec2(0, avail_h), ImGuiChildFlags_None,
        ImGuiWindowFlags_HorizontalScrollbar);

    // Use full width with small padding — no artificial max-width centering
    float avail_w = ImGui::GetContentRegionAvail().x;
    float pad_x = std::min(12.0f, avail_w * 0.01f);

    ImGui::SetCursorPosX(pad_x);
    ImGui::BeginChild("##ce_cells_inner", ImVec2(avail_w - pad_x * 2, 0), ImGuiChildFlags_None);

    ImGui::Spacing();

    auto& cells = nb().cells;
    for (int i = 0; i < static_cast<int>(cells.size()); i++) {
        render_cell(i);
        render_add_cell_divider(i);
    }

    if (cells.empty()) {
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "No cells. Click + CODE or + MARKDOWN to add one.");
        render_add_cell_divider(-1);
    }

    ImGui::Spacing();
    ImGui::EndChild();
    ImGui::EndChild();
}

// =============================================================================
// Single cell
// =============================================================================
void CodeEditorScreen::render_cell(int idx) {
    using namespace theme::colors;
    try {

    if (idx < 0 || idx >= static_cast<int>(nb().cells.size())) {
        file_log("render_cell: idx " + std::to_string(idx) + " out of range, cells=" +
                  std::to_string(nb().cells.size()));
        return;
    }

    auto& cell = nb().cells[idx];
    bool is_selected = (idx == tab().selected_cell);
    bool is_running = (idx == running_cell_.load());
    bool has_error = false;
    bool has_output = !cell.outputs.empty();
    for (auto& o : cell.outputs) {
        if (o.output_type == "error") { has_error = true; break; }
    }
    bool has_success = has_output && !has_error;

    // Execution order warning (point 15)
    bool order_warn = has_execution_order_warning(idx);

    ImVec4 accent = BORDER_DIM;
    if (is_running)       accent = WARNING;
    else if (has_error)   accent = ERROR_RED;
    else if (has_success) accent = SUCCESS;
    else if (is_selected) accent = ACCENT;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);

    char cid[32];
    std::snprintf(cid, sizeof(cid), "##cell_%d", idx);
    ImGui::BeginChild(cid, ImVec2(0, 0),
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

    // Left colored border
    ImVec2 p = ImGui::GetWindowPos();
    ImVec2 s = ImGui::GetWindowSize();
    ImGui::GetWindowDrawList()->AddRectFilled(
        p, ImVec2(p.x + 4, p.y + s.y),
        ImGui::ColorConvertFloat4ToU32(accent));

    // Drag reorder (point 7) — source
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        drag_source_cell_ = idx;
        ImGui::SetDragDropPayload("CELL_REORDER", &idx, sizeof(int));
        ImGui::Text("Move cell %d", idx + 1);
        ImGui::EndDragDropSource();
    }
    // Drag target
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CELL_REORDER")) {
            int src = *static_cast<const int*>(payload->Data);
            if (src != idx && src >= 0 && src < static_cast<int>(nb().cells.size())) {
                push_undo("reorder cell");
                auto cell_copy = std::move(nb().cells[src]);
                nb().cells.erase(nb().cells.begin() + src);
                int target = (src < idx) ? idx - 1 : idx;
                nb().cells.insert(nb().cells.begin() + target, std::move(cell_copy));
                tab().selected_cell = target;
                tab().unsaved = true;
                active_edit_cell_ = -1;
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Cell header
    render_cell_header(idx);

    // Cell editor / preview
    render_cell_editor(idx);

    // Click to select
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseClicked(0)) {
        if (tab().selected_cell != idx) {
            flush_edit_buffer();
            tab().selected_cell = idx;
            active_edit_cell_ = -1;
        }
    }

    // Cell output (point 9: scroll-capped)
    if (has_output && cell.cell_type == "code") {
        ImGui::Spacing();
        render_cell_output(idx);
    }

    // Execution order warning badge (point 15)
    if (order_warn) {
        ImGui::SetCursorPosX(10);
        ImGui::TextColored(WARNING, "[!] Cells executed out of order");
    }

    ImGui::Spacing();
    ImGui::EndChild();
    ImGui::PopStyleColor(); // ChildBg

    ImGui::Spacing();

    } catch (const std::exception& e) {
        file_log("render_cell EXCEPTION idx=" + std::to_string(idx) + ": " + e.what());
    } catch (...) {
        file_log("render_cell UNKNOWN EXCEPTION idx=" + std::to_string(idx));
    }
}

// =============================================================================
// Cell header
// =============================================================================
void CodeEditorScreen::render_cell_header(int idx) {
    using namespace theme::colors;

    auto& cell = nb().cells[idx];
    bool is_selected = (idx == tab().selected_cell);
    bool is_running = (idx == running_cell_.load());
    bool has_error = false;
    bool has_output = !cell.outputs.empty();
    for (auto& o : cell.outputs) {
        if (o.output_type == "error") { has_error = true; break; }
    }
    bool has_success = has_output && !has_error;

    ImGui::SetCursorPos(ImVec2(10, 4));

    // Execution count badge with timer (point 8)
    if (cell.cell_type == "code") {
        ImVec4 badge_col = is_running ? WARNING :
                           has_error ? ERROR_RED :
                           has_success ? SUCCESS : ACCENT;

        char badge[48];
        if (is_running)
            std::snprintf(badge, sizeof(badge), "In [*]");
        else if (cell.execution_count >= 0) {
            if (cell.exec_time_ms >= 0) {
                if (cell.exec_time_ms >= 1000)
                    std::snprintf(badge, sizeof(badge), "In [%d] %.1fs", cell.execution_count, cell.exec_time_ms / 1000.0);
                else
                    std::snprintf(badge, sizeof(badge), "In [%d] %dms", cell.execution_count, static_cast<int>(cell.exec_time_ms));
            } else {
                std::snprintf(badge, sizeof(badge), "In [%d]", cell.execution_count);
            }
        }
        else
            std::snprintf(badge, sizeof(badge), "In [ ]");

        ImGui::TextColored(badge_col, "%s", badge);
    } else {
        ImGui::TextColored(ImVec4(0.62f, 0.31f, 0.87f, 1.0f), "MD");
    }

    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "%s",
        cell.cell_type == "code" ? "PYTHON" : "MARKDOWN");

    // Action buttons (when selected)
    if (is_selected) {
        float btn_x = ImGui::GetContentRegionAvail().x - 220;
        if (btn_x > 100) ImGui::SameLine(btn_x);
        else ImGui::SameLine();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));

        if (cell.cell_type == "code") {
            if (is_running) {
                ImGui::TextColored(WARNING, "RUNNING...");
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.15f, 0.05f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, SUCCESS);
                char run_id[32];
                std::snprintf(run_id, sizeof(run_id), "RUN##run_%d", idx);
                if (ImGui::SmallButton(run_id)) { run_cell(idx); }
                ImGui::PopStyleColor(2);
            }
            ImGui::SameLine(0, 4);
        }

        ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);

        // Toggle type
        char toggle_id[32];
        std::snprintf(toggle_id, sizeof(toggle_id), "%s##tog_%d",
            cell.cell_type == "code" ? "->MD" : "->PY", idx);
        if (ImGui::SmallButton(toggle_id)) {
            push_undo("toggle type");
            toggle_cell_type(idx);
        }
        ImGui::SameLine(0, 2);

        // Move up
        char up_id[32];
        std::snprintf(up_id, sizeof(up_id), "^##up_%d", idx);
        if (idx > 0) {
            if (ImGui::SmallButton(up_id)) {
                push_undo("move cell up");
                move_cell(idx, -1);
            }
        }
        ImGui::SameLine(0, 2);

        // Move down
        char dn_id[32];
        std::snprintf(dn_id, sizeof(dn_id), "v##dn_%d", idx);
        if (idx < static_cast<int>(nb().cells.size()) - 1) {
            if (ImGui::SmallButton(dn_id)) {
                push_undo("move cell down");
                move_cell(idx, 1);
            }
        }
        ImGui::SameLine(0, 2);

        // Clear output
        if (has_output) {
            char clr_id[32];
            std::snprintf(clr_id, sizeof(clr_id), "CLR##clr_%d", idx);
            if (ImGui::SmallButton(clr_id)) clear_cell_output(idx);
            ImGui::SameLine(0, 2);
        }

        ImGui::PopStyleColor(2);

        // Delete
        if (nb().cells.size() > 1) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.02f, 0.02f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
            char del_id[32];
            std::snprintf(del_id, sizeof(del_id), "X##del_%d", idx);
            if (ImGui::SmallButton(del_id)) {
                push_undo("delete cell");
                delete_cell(idx);
                ImGui::PopStyleColor(2);
                ImGui::PopStyleVar();
                // Early exit — cell no longer valid
                return;
            }
            ImGui::PopStyleColor(2);
        }

        ImGui::PopStyleVar();
    }
}

// =============================================================================
// Cell line numbers (point 12)
// =============================================================================
void CodeEditorScreen::render_cell_line_numbers(const std::string& source, float height) {
    using namespace theme::colors;

    int line_count = 1;
    for (char c : source) if (c == '\n') line_count++;

    float line_h = ImGui::GetTextLineHeight();
    float gutter_w = 35.0f;

    ImGui::BeginChild("##gutter", ImVec2(gutter_w, height), ImGuiChildFlags_None);
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DISABLED);
    for (int i = 1; i <= line_count; i++) {
        ImGui::Text("%3d", i);
    }
    ImGui::PopStyleColor();
    ImGui::EndChild();
    (void)line_h; // suppress unused warning
}

// =============================================================================
// Cell editor
// =============================================================================
void CodeEditorScreen::render_cell_editor(int idx) {
    using namespace theme::colors;

    auto& cell = nb().cells[idx];
    bool is_selected = (idx == tab().selected_cell);

    ImGui::SetCursorPosX(10);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_DARKEST);

    char editor_id[32];
    std::snprintf(editor_id, sizeof(editor_id), "##celledit_%d", idx);

    int line_count = 1;
    for (char c : cell.source) if (c == '\n') line_count++;
    float line_h = ImGui::GetTextLineHeight();
    float editor_h = std::max(3, std::min(line_count + 1, 30)) * line_h + 8;

    if (is_selected || active_edit_cell_ == idx) {
        if (active_edit_cell_ != idx) {
            active_edit_cell_ = idx;
            edit_buf_.resize(cell.source.size() + 16384);
            std::memcpy(edit_buf_.data(), cell.source.c_str(), cell.source.size() + 1);
        }

        // Line numbers (point 12)
        char gutter_id[32];
        std::snprintf(gutter_id, sizeof(gutter_id), "##gutter_%d", idx);
        ImGui::BeginChild(gutter_id, ImVec2(32, editor_h), ImGuiChildFlags_None);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DISABLED);
        for (int i = 1; i <= line_count; i++) {
            ImGui::Text("%3d", i);
        }
        ImGui::PopStyleColor();
        ImGui::EndChild();

        ImGui::SameLine(0, 0);

        float edit_w = ImGui::GetContentRegionAvail().x - 4;
        if (ImGui::InputTextMultiline(editor_id, edit_buf_.data(), edit_buf_.size(),
                ImVec2(edit_w, editor_h),
                ImGuiInputTextFlags_AllowTabInput)) {
            cell.source = edit_buf_.data();
            tab().unsaved = true;
        }

        // Ctrl+Enter / Shift+Enter
        if (ImGui::IsItemFocused()) {
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) &&
                (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift)) {
                cell.source = edit_buf_.data();
                if (cell.cell_type == "code") {
                    run_cell(idx);
                    if (ImGui::GetIO().KeyShift) {
                        if (idx < static_cast<int>(nb().cells.size()) - 1) {
                            tab().selected_cell = idx + 1;
                            active_edit_cell_ = -1;
                        } else {
                            add_cell(idx, "code");
                        }
                    }
                }
            }
        }
    } else {
        // Read-only preview with syntax highlighting
        ImGui::SetCursorPosX(10);

        // Line numbers for preview too
        char gutter_id[32];
        std::snprintf(gutter_id, sizeof(gutter_id), "##gutter_p_%d", idx);
        ImGui::BeginChild(gutter_id, ImVec2(32, editor_h), ImGuiChildFlags_None);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DISABLED);
        for (int i = 1; i <= line_count; i++) {
            ImGui::Text("%3d", i);
        }
        ImGui::PopStyleColor();
        ImGui::EndChild();

        ImGui::SameLine(0, 0);

        float preview_w = ImGui::GetContentRegionAvail().x - 4;
        char prev_id[32];
        std::snprintf(prev_id, sizeof(prev_id), "##preview_%d", idx);
        ImGui::BeginChild(prev_id, ImVec2(preview_w, editor_h), ImGuiChildFlags_None);

        if (cell.source.empty()) {
            ImGui::TextColored(TEXT_DISABLED,
                cell.cell_type == "code" ? "# Enter Python code..." : "Enter markdown text...");
        } else if (cell.cell_type == "code") {
            render_highlighted_text(cell.source, preview_w - 8);
        } else {
            render_markdown(cell.source);
        }

        ImGui::EndChild();
    }

    ImGui::PopStyleColor(); // FrameBg
}

// =============================================================================
// Cell Output (point 9: scroll capped at 300px)
// =============================================================================
void CodeEditorScreen::render_cell_output(int idx) {
    using namespace theme::colors;
    try {

    auto& cell = nb().cells[idx];
    bool has_error = false;
    for (auto& o : cell.outputs) {
        if (o.output_type == "error") { has_error = true; break; }
    }

    ImGui::SetCursorPosX(10);
    ImGui::TextColored(has_error ? ERROR_RED : SUCCESS,
        has_error ? "ERROR OUTPUT" : "OUTPUT");
    if (cell.execution_count >= 0) {
        ImGui::SameLine(0, 8);
        ImGui::TextColored(BORDER_BRIGHT, "|");
        ImGui::SameLine(0, 8);
        ImGui::TextColored(INFO, "Out [%d]", cell.execution_count);
    }
    if (cell.exec_time_ms >= 0) {
        ImGui::SameLine(0, 8);
        ImGui::TextColored(BORDER_BRIGHT, "|");
        ImGui::SameLine(0, 8);
        if (cell.exec_time_ms >= 1000)
            ImGui::TextColored(TEXT_DIM, "%.2fs", cell.exec_time_ms / 1000.0);
        else
            ImGui::TextColored(TEXT_DIM, "%dms", static_cast<int>(cell.exec_time_ms));
    }

    ImGui::Spacing();

    // Scroll-capped output container (point 9)
    float max_output_h = 300.0f;
    char scroll_id[32];
    std::snprintf(scroll_id, sizeof(scroll_id), "##out_scroll_%d", idx);

    ImGui::SetCursorPosX(10);
    ImGui::BeginChild(scroll_id, ImVec2(ImGui::GetContentRegionAvail().x - 4, max_output_h),
        ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);

    for (auto& out : cell.outputs) {
        if (out.output_type == "stream") {
            ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - 8);
            ImGui::TextColored(TEXT_PRIMARY, "%s", out.text.c_str());
            ImGui::PopTextWrapPos();
        } else if (out.output_type == "error") {
            // Red left border
            ImVec2 p2 = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                p2, ImVec2(p2.x + 3, p2.y + ImGui::GetTextLineHeight() * 3),
                ImGui::ColorConvertFloat4ToU32(ERROR_RED));

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8);
            if (!out.ename.empty()) {
                ImGui::TextColored(ERROR_RED, "%s: %s", out.ename.c_str(), out.evalue.c_str());
            }
            if (!out.traceback.empty()) {
                ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - 8);
                ImGui::TextColored(ImVec4(0.96f, 0.30f, 0.30f, 0.7f), "%s", out.traceback.c_str());
                ImGui::PopTextWrapPos();
            }
        } else if (out.output_type == "execute_result" || out.output_type == "display_data") {
            ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - 8);
            ImGui::TextColored(INFO, "%s", out.text.c_str());
            ImGui::PopTextWrapPos();
        }
    }

    ImGui::EndChild();

    } catch (const std::exception& e) {
        file_log("render_cell_output EXCEPTION idx=" + std::to_string(idx) + ": " + e.what());
    } catch (...) {
        file_log("render_cell_output UNKNOWN EXCEPTION idx=" + std::to_string(idx));
    }
}

// =============================================================================
// Add cell divider
// =============================================================================
void CodeEditorScreen::render_add_cell_divider(int after_idx) {
    using namespace theme::colors;

    float avail_w = ImGui::GetContentRegionAvail().x;

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 2));

    float btn_w = ImGui::CalcTextSize("+ CODE").x + ImGui::CalcTextSize("+ MARKDOWN").x + 60;
    float center_x = (avail_w - btn_w) * 0.5f;
    if (center_x > 0) ImGui::SetCursorPosX(center_x);

    char code_id[32], md_id[32];
    std::snprintf(code_id, sizeof(code_id), "+ CODE##add_c_%d", after_idx);
    std::snprintf(md_id, sizeof(md_id), "+ MARKDOWN##add_m_%d", after_idx);

    if (ImGui::SmallButton(code_id)) {
        push_undo("add code cell");
        add_cell(after_idx, "code");
    }
    ImGui::SameLine(0, 4);
    if (ImGui::SmallButton(md_id)) {
        push_undo("add markdown cell");
        add_cell(after_idx, "markdown");
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
}

} // namespace fincept::code_editor
