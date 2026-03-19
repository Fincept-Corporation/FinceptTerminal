#include "code_editor_screen.h"
#include "editor_internal.h"
#include "ui/theme.h"
#include "ui/yoga_helpers.h"
#include "notebook_io.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace fs = std::filesystem;

namespace fincept::code_editor {

// =============================================================================
// Tab bar
// =============================================================================
void CodeEditorScreen::render_tab_bar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##ce_tabbar", ImVec2(0, 26), ImGuiChildFlags_Borders);

    ImGui::SetCursorPos(ImVec2(4, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));

    int close_idx = -1;
    for (int i = 0; i < static_cast<int>(tabs_.size()); i++) {
        bool is_active = (i == active_tab_idx_);
        auto& t = tabs_[i];

        std::string label = t.name;
        if (t.unsaved) label += " *";
        label += "##ntab_" + std::to_string(i);

        if (is_active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT_BG);
            ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
        }

        if (ImGui::SmallButton(label.c_str())) {
            flush_edit_buffer();
            active_tab_idx_ = i;
            active_edit_cell_ = -1;
        }
        ImGui::PopStyleColor(2);

        // Close button
        ImGui::SameLine(0, 0);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        char close_id[32];
        std::snprintf(close_id, sizeof(close_id), "x##close_%d", i);
        if (ImGui::SmallButton(close_id)) {
            close_idx = i;
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine(0, 4);
    }

    // "+" button for new tab
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
    if (ImGui::SmallButton("+##new_tab")) {
        new_tab();
    }
    ImGui::PopStyleColor(2);

    ImGui::PopStyleVar(2);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    if (close_idx >= 0) close_tab(close_idx);
}

// =============================================================================
// Toolbar
// =============================================================================
void CodeEditorScreen::render_toolbar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##ce_toolbar", ImVec2(0, 34), ImGuiChildFlags_Borders);
    ImGui::SetCursorPos(ImVec2(8, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 3));

    // File actions
    if (theme::SecondaryButton("NEW")) { new_tab(); }
    ImGui::SameLine(0, 4);

    if (theme::SecondaryButton("OPEN")) { open_notebook_dialog(); }
    ImGui::SameLine(0, 4);

    // Save button — yellow when unsaved
    if (tab().unsaved) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.17f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.25f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, WARNING);
    }
    if (ImGui::Button("SAVE")) { save_current(); }
    if (tab().unsaved) { ImGui::PopStyleColor(3); }

    ImGui::SameLine(0, 4);

    // Export to .py (point 16)
    if (theme::SecondaryButton("EXPORT .PY")) {
        std::string path = native_save_dialog(
            "Python Files (*.py)\0*.py\0All Files\0*.*\0",
            "Export as Python");
        if (path.empty()) {
            show_export_dialog_ = true;
            std::memset(path_buf_, 0, sizeof(path_buf_));
        } else {
            export_to_py(path);
        }
    }

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    // Run All
    if (running_all_) {
        ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DISABLED);
        ImGui::Button("RUNNING...");
        ImGui::PopStyleColor(2);
    } else {
        if (theme::AccentButton("RUN ALL")) { run_all_cells(); }
    }
    ImGui::SameLine(0, 4);

    if (running_all_ || exec_pending_) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.05f, 0.05f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
        if (ImGui::Button("STOP")) { interrupt_execution(); }
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0, 4);
    }

    // Clear all outputs
    if (theme::SecondaryButton("CLEAR ALL")) { clear_all_outputs(); }

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    // Undo/Redo (point 5)
    bool can_undo = !tab().undo_stack.empty();
    bool can_redo = !tab().redo_stack.empty();

    if (!can_undo) ImGui::BeginDisabled();
    if (theme::SecondaryButton("UNDO")) { undo(); }
    if (!can_undo) ImGui::EndDisabled();
    ImGui::SameLine(0, 4);

    if (!can_redo) ImGui::BeginDisabled();
    if (theme::SecondaryButton("REDO")) { redo(); }
    if (!can_redo) ImGui::EndDisabled();

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 12);

    // Find (point 13)
    if (theme::SecondaryButton("FIND")) { show_search_ = !show_search_; }
    ImGui::SameLine(0, 4);

    // Pip install (point 14)
    if (theme::SecondaryButton("PIP")) {
        show_pip_dialog_ = true;
        std::memset(pip_pkg_buf_, 0, sizeof(pip_pkg_buf_));
    }

    // Right side: filename
    float right_w = 300.0f;
    float avail = ImGui::GetContentRegionAvail().x;
    if (avail > right_w + 50) {
        ImGui::SameLine(ImGui::GetWindowWidth() - right_w);
        ImGui::TextColored(ACCENT, "[>]");
        ImGui::SameLine(0, 4);
        std::string display_name = tab().file_path.empty() ?
            (tab().name + ".ipynb") :
            fs::path(tab().file_path).filename().string();
        ImGui::TextColored(TEXT_PRIMARY, "%s", display_name.c_str());
        if (tab().unsaved) {
            ImGui::SameLine(0, 8);
            ImGui::TextColored(WARNING, "MODIFIED");
        }
    }

    ImGui::PopStyleVar(); // FramePadding
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Search bar (point 13)
// =============================================================================
void CodeEditorScreen::render_search_bar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##ce_search", ImVec2(0, 28), ImGuiChildFlags_Borders);
    ImGui::SetCursorPos(ImVec2(8, 4));

    ImGui::TextColored(TEXT_DIM, "Find:");
    ImGui::SameLine(0, 4);
    ImGui::PushItemWidth(200);
    if (ImGui::InputText("##search_input", search_buf_, sizeof(search_buf_),
            ImGuiInputTextFlags_EnterReturnsTrue)) {
        perform_search();
    }
    ImGui::PopItemWidth();

    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "Replace:");
    ImGui::SameLine(0, 4);
    ImGui::PushItemWidth(200);
    ImGui::InputText("##replace_input", replace_buf_, sizeof(replace_buf_));
    ImGui::PopItemWidth();

    ImGui::SameLine(0, 8);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));

    if (ImGui::SmallButton("Find##do_find")) perform_search();
    ImGui::SameLine(0, 2);
    if (ImGui::SmallButton("Prev##find_prev")) navigate_match(-1);
    ImGui::SameLine(0, 2);
    if (ImGui::SmallButton("Next##find_next")) navigate_match(1);
    ImGui::SameLine(0, 4);
    if (ImGui::SmallButton("Replace##do_repl")) replace_current();
    ImGui::SameLine(0, 2);
    if (ImGui::SmallButton("All##repl_all")) replace_all();
    ImGui::SameLine(0, 8);

    ImGui::Checkbox("Case##search_case", &search_case_sensitive_);

    ImGui::SameLine(0, 8);
    if (!search_matches_.empty()) {
        ImGui::TextColored(INFO, "%d/%d",
            current_match_ >= 0 ? current_match_ + 1 : 0,
            static_cast<int>(search_matches_.size()));
    }

    ImGui::SameLine(0, 8);
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
    if (ImGui::SmallButton("X##close_search")) {
        show_search_ = false;
        search_matches_.clear();
    }
    ImGui::PopStyleColor();

    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Status bar
// =============================================================================
void CodeEditorScreen::render_status_bar() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##ce_status", ImVec2(0, 24), ImGuiChildFlags_Borders);
    ImGui::SetCursorPos(ImVec2(8, 4));

    // Python version badge
    if (!python_version_.empty()) {
        ImGui::TextColored(SUCCESS, "%s", python_version_.c_str());
    } else {
        ImGui::TextColored(TEXT_DIM, "PYTHON");
    }

    // Cell counts
    int code_count = 0, md_count = 0, executed = 0;
    for (auto& c : nb().cells) {
        if (c.cell_type == "code") code_count++;
        else md_count++;
        if (c.execution_count >= 0) executed++;
    }

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(INFO, "%d", code_count);
    ImGui::SameLine(0, 2);
    ImGui::TextColored(TEXT_DIM, "CODE");

    ImGui::SameLine(0, 8);
    ImGui::TextColored(ImVec4(0.62f, 0.31f, 0.87f, 1.0f), "%d", md_count);
    ImGui::SameLine(0, 2);
    ImGui::TextColored(TEXT_DIM, "MD");

    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "EXEC:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(INFO, "%d/%d", executed, code_count);

    // Tab count
    ImGui::SameLine(0, 12);
    ImGui::TextColored(BORDER_BRIGHT, "|");
    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "TABS:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(INFO, "%d", static_cast<int>(tabs_.size()));

    // Undo depth
    ImGui::SameLine(0, 8);
    ImGui::TextColored(TEXT_DIM, "UNDO:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(INFO, "%d", static_cast<int>(tab().undo_stack.size()));

    if (tab().unsaved) {
        ImGui::SameLine(0, 12);
        ImGui::TextColored(BORDER_BRIGHT, "|");
        ImGui::SameLine(0, 8);
        ImGui::TextColored(WARNING, "UNSAVED");
    }

    if (!status_message_.empty()) {
        ImGui::SameLine(0, 12);
        ImGui::TextColored(BORDER_BRIGHT, "|");
        ImGui::SameLine(0, 8);
        ImGui::TextColored(INFO, "%s", status_message_.c_str());
    }

    // Right side: keyboard hints (point 17)
    const char* hints = "Ctrl+Enter:RUN | Shift+Enter:RUN+NEXT | Ctrl+Z:UNDO | Ctrl+F:FIND | a/b:ADD | dd:DEL";
    float hint_w = ImGui::CalcTextSize(hints).x + 20;
    float win_w = ImGui::GetWindowWidth();
    if (win_w > hint_w + 100) {
        ImGui::SameLine(win_w - hint_w);
        ImGui::TextColored(TEXT_DISABLED, "%s", hints);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// File dialogs
// =============================================================================
void CodeEditorScreen::render_file_dialog() {
    using namespace theme::colors;

    if (show_open_dialog_) {
        ImGui::OpenPopup("Open Notebook##ce_open");
        show_open_dialog_ = false;
    }
    if (show_save_dialog_) {
        ImGui::OpenPopup("Save Notebook##ce_save");
        show_save_dialog_ = false;
    }
    if (show_export_dialog_) {
        ImGui::OpenPopup("Export Python##ce_export");
        show_export_dialog_ = false;
    }

    // Open dialog (fallback if native failed)
    ImGui::SetNextWindowSize(ImVec2(500, 120));
    if (ImGui::BeginPopupModal("Open Notebook##ce_open", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::Text("Enter path to .ipynb file:");
        ImGui::PushItemWidth(-1);
        bool enter = ImGui::InputText("##open_path", path_buf_, sizeof(path_buf_),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();
        ImGui::Spacing();
        if (theme::AccentButton("Open") || enter) {
            if (std::strlen(path_buf_) > 0) {
                open_notebook(path_buf_);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine(0, 8);
        if (theme::SecondaryButton("Cancel")) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    // Save dialog
    ImGui::SetNextWindowSize(ImVec2(500, 120));
    if (ImGui::BeginPopupModal("Save Notebook##ce_save", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::Text("Enter save path (.ipynb):");
        ImGui::PushItemWidth(-1);
        bool enter = ImGui::InputText("##save_path", path_buf_, sizeof(path_buf_),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();
        ImGui::Spacing();
        if (theme::AccentButton("Save") || enter) {
            if (std::strlen(path_buf_) > 0) {
                save_notebook_to(path_buf_);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine(0, 8);
        if (theme::SecondaryButton("Cancel")) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    // Export dialog
    ImGui::SetNextWindowSize(ImVec2(500, 120));
    if (ImGui::BeginPopupModal("Export Python##ce_export", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::Text("Enter export path (.py):");
        ImGui::PushItemWidth(-1);
        bool enter = ImGui::InputText("##export_path", path_buf_, sizeof(path_buf_),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();
        ImGui::Spacing();
        if (theme::AccentButton("Export") || enter) {
            if (std::strlen(path_buf_) > 0) {
                export_to_py(path_buf_);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine(0, 8);
        if (theme::SecondaryButton("Cancel")) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}

// =============================================================================
// Pip install dialog (point 14)
// =============================================================================
void CodeEditorScreen::render_pip_dialog() {
    using namespace theme::colors;

    if (show_pip_dialog_) {
        ImGui::OpenPopup("Install Package##pip");
        show_pip_dialog_ = false;
    }

    ImGui::SetNextWindowSize(ImVec2(400, 130));
    if (ImGui::BeginPopupModal("Install Package##pip", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::Text("Package name (e.g. pandas, numpy==1.24):");
        ImGui::PushItemWidth(-1);
        bool enter = ImGui::InputText("##pip_pkg", pip_pkg_buf_, sizeof(pip_pkg_buf_),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();
        ImGui::Spacing();
        if (theme::AccentButton("Install") || enter) {
            if (std::strlen(pip_pkg_buf_) > 0) {
                pip_install(pip_pkg_buf_);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine(0, 8);
        if (theme::SecondaryButton("Cancel")) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}

// =============================================================================
// Debug panel
// =============================================================================
void CodeEditorScreen::render_debug_panel() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));
    ImGui::BeginChild("##ce_debug", ImVec2(0, 200), ImGuiChildFlags_Borders,
        ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::TextColored(ACCENT, "DEBUG LOG");
    ImGui::SameLine(0, 16);
    if (ImGui::SmallButton("Clear##dbg_clear")) {
        std::lock_guard<std::mutex> lock(debug_mutex_);
        debug_log_.clear();
    }
    ImGui::SameLine(0, 8);
    if (ImGui::SmallButton("Hide##dbg_hide")) {
        show_debug_log_ = false;
    }
    ImGui::Separator();

    {
        std::lock_guard<std::mutex> lock(debug_mutex_);
        for (auto& entry : debug_log_) {
            ImGui::TextColored(TEXT_DIM, "[%s]", entry.timestamp.c_str());
            ImGui::SameLine(0, 4);

            // Color based on content
            ImVec4 col = TEXT_PRIMARY;
            if (entry.message.find("ERROR") != std::string::npos ||
                entry.message.find("FAIL") != std::string::npos) col = ERROR_RED;
            else if (entry.message.find("OK") != std::string::npos ||
                     entry.message.find("SUCCESS") != std::string::npos) col = SUCCESS;
            else if (entry.message.find(">>>") != std::string::npos) col = WARNING;

            ImGui::TextColored(col, "%s", entry.message.c_str());
        }
    }

    // Auto-scroll
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20)
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace fincept::code_editor
