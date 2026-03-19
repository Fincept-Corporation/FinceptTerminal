#include "code_editor_screen.h"
#include "editor_internal.h"
#include "python/python_runner.h"
#include <imgui.h>
#include <string>
#include <chrono>
#include <mutex>
#include <thread>
#include <filesystem>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace fincept::code_editor {

// =============================================================================
// debug_log — defined here because it needs the full CodeEditorScreen type
// =============================================================================
void debug_log(const std::string& msg) {
    file_log(msg);
    try {
        if (g_debug_instance) g_debug_instance->add_debug(msg);
    } catch (...) {}
}

// =============================================================================
// Keyboard navigation (point 17)
// =============================================================================
void CodeEditorScreen::handle_keyboard_navigation() {
    if (tabs_.empty()) return;

    auto& io = ImGui::GetIO();

    // Only handle when no text input is active
    bool text_active = ImGui::IsAnyItemActive();

    // Ctrl+Z = Undo, Ctrl+Y = Redo
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && !io.KeyShift) { undo(); return; }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) { redo(); return; }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && io.KeyShift) { redo(); return; }

    // Ctrl+F = Find
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F)) { show_search_ = !show_search_; return; }

    // Ctrl+S = Save
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) { save_current(); return; }

    // When text input is active, don't intercept other keys
    if (text_active) return;

    auto& cells = nb().cells;
    int sel = tab().selected_cell;

    // Arrow up/down — move between cells
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_K)) {
        if (sel > 0) {
            flush_edit_buffer();
            tab().selected_cell = sel - 1;
            active_edit_cell_ = -1;
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_J)) {
        if (sel < static_cast<int>(cells.size()) - 1) {
            flush_edit_buffer();
            tab().selected_cell = sel + 1;
            active_edit_cell_ = -1;
        }
    }

    // 'a' — add cell above
    if (ImGui::IsKeyPressed(ImGuiKey_A)) {
        push_undo("add cell above");
        add_cell(sel - 1, "code");
    }

    // 'b' — add cell below
    if (ImGui::IsKeyPressed(ImGuiKey_B)) {
        push_undo("add cell below");
        add_cell(sel, "code");
    }

    // 'x' — delete cell
    if (ImGui::IsKeyPressed(ImGuiKey_X) && cells.size() > 1) {
        push_undo("delete cell");
        delete_cell(sel);
    }

    // 'm' — toggle to markdown
    if (ImGui::IsKeyPressed(ImGuiKey_M)) {
        if (sel >= 0 && sel < static_cast<int>(cells.size()) && cells[sel].cell_type == "code") {
            push_undo("to markdown");
            toggle_cell_type(sel);
        }
    }

    // 'y' — toggle to code
    if (ImGui::IsKeyPressed(ImGuiKey_Y)) {
        if (sel >= 0 && sel < static_cast<int>(cells.size()) && cells[sel].cell_type == "markdown") {
            push_undo("to code");
            toggle_cell_type(sel);
        }
    }

    // Enter — start editing selected cell
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) && !io.KeyCtrl && !io.KeyShift) {
        active_edit_cell_ = -1; // will be re-initialized in render_cell_editor
    }

    // Escape — deselect edit mode
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        flush_edit_buffer();
        active_edit_cell_ = -1;
        show_search_ = false;
    }
}

// =============================================================================
// Execution order warning (point 15)
// =============================================================================
bool CodeEditorScreen::has_execution_order_warning(int idx) {
    auto& cells = nb().cells;
    if (idx < 0 || idx >= static_cast<int>(cells.size())) return false;
    auto& cell = cells[idx];
    if (cell.cell_type != "code" || cell.execution_count < 0) return false;

    // Check if any earlier code cell has a higher execution count
    for (int i = 0; i < idx; i++) {
        if (cells[i].cell_type == "code" && cells[i].execution_count > cell.execution_count) {
            return true;
        }
    }
    return false;
}

// =============================================================================
// Utility
// =============================================================================
void CodeEditorScreen::detect_python_version() {
    std::thread([this]() {
        try {
            fs::path py = python::resolve_python_path("");
            if (py.empty() || !fs::exists(py)) {
#ifdef _WIN32
                py = "python";
#else
                py = "python3";
#endif
            }

#ifdef _WIN32
            std::string cmd = "\"\"" + py.string() + "\" -c \"import sys; print(f'Python {sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}')\" 2>&1\"";
#else
            std::string cmd = "\"" + py.string() + "\" -c \"import sys; print(f'Python {sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}')\" 2>&1";
#endif

#ifdef _WIN32
            FILE* pipe = _popen(cmd.c_str(), "r");
#else
            FILE* pipe = popen(cmd.c_str(), "r");
#endif
            if (!pipe) { python_version_ = "Python N/A"; return; }

            std::string output;
            char buf[256];
            while (std::fgets(buf, sizeof(buf), pipe)) output += buf;

#ifdef _WIN32
            _pclose(pipe);
#else
            pclose(pipe);
#endif

            while (!output.empty() && (output.back() == '\n' || output.back() == '\r' || output.back() == ' '))
                output.pop_back();

            python_version_ = output.empty() ? "Python N/A" : output;
        } catch (...) {
            python_version_ = "Python N/A";
        }
    }).detach();
}

void CodeEditorScreen::set_status(const std::string& msg) {
    status_message_ = msg;
    status_message_time_ = ImGui::GetTime();
}

void CodeEditorScreen::flush_edit_buffer() {
    if (active_edit_cell_ >= 0 && !tabs_.empty() &&
        active_edit_cell_ < static_cast<int>(nb().cells.size())) {
        nb().cells[active_edit_cell_].source = edit_buf_.data();
    }
}

void CodeEditorScreen::add_debug(const std::string& msg) {
    // File logging is handled by file_log() — just keep in-memory log here
    try {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count() % 100000;
        char ts[16];
        std::snprintf(ts, sizeof(ts), "%05d", static_cast<int>(ms));

        std::lock_guard<std::mutex> lock(debug_mutex_);
        debug_log_.push_back({std::string(ts), msg});
        if (debug_log_.size() > 200)
            debug_log_.erase(debug_log_.begin());
    } catch (...) {}
}

} // namespace fincept::code_editor
