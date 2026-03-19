#include "code_editor_screen.h"
#include "editor_internal.h"
#include "notebook_io.h"
#include "core/logger.h"
#include <string>
#include <filesystem>
#include <fstream>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

namespace fs = std::filesystem;

namespace fincept::code_editor {

// =============================================================================
// Native File Dialogs (point 11)
// =============================================================================
#ifdef _WIN32
std::string CodeEditorScreen::native_open_dialog(const char* filter, const char* title) {
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn)) return std::string(filename);
    return "";
}

std::string CodeEditorScreen::native_save_dialog(const char* filter, const char* title) {
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileNameA(&ofn)) return std::string(filename);
    return "";
}
#else
std::string CodeEditorScreen::native_open_dialog(const char*, const char*) { return ""; }
std::string CodeEditorScreen::native_save_dialog(const char*, const char*) { return ""; }
#endif

// =============================================================================
// File operations (point 3: multi-tab)
// =============================================================================
void CodeEditorScreen::new_tab() {
    NotebookTab t;
    t.notebook = io::create_new_notebook();
    t.name = "fincept" + std::to_string(tab_counter_++);
    t.selected_cell = 1;
    tabs_.push_back(std::move(t));
    active_tab_idx_ = static_cast<int>(tabs_.size()) - 1;
    active_edit_cell_ = -1;
    set_status("New notebook created");
}

void CodeEditorScreen::close_tab(int idx) {
    if (idx < 0 || idx >= static_cast<int>(tabs_.size())) return;
    tabs_.erase(tabs_.begin() + idx);
    if (tabs_.empty()) new_tab();
    if (active_tab_idx_ >= static_cast<int>(tabs_.size()))
        active_tab_idx_ = static_cast<int>(tabs_.size()) - 1;
    active_edit_cell_ = -1;
}

void CodeEditorScreen::open_notebook_dialog() {
    std::string path = native_open_dialog(
        "Jupyter Notebooks (*.ipynb)\0*.ipynb\0All Files\0*.*\0",
        "Open Notebook");
    if (!path.empty()) {
        open_notebook(path);
    } else {
        show_open_dialog_ = true;
        std::memset(path_buf_, 0, sizeof(path_buf_));
    }
}

void CodeEditorScreen::open_notebook(const std::string& path) {
    if (!fs::exists(path)) {
        set_status("File not found: " + path);
        return;
    }

    Notebook loaded = io::load_notebook(path);
    if (loaded.cells.empty()) {
        set_status("Failed to load notebook");
        return;
    }

    NotebookTab t;
    t.notebook = std::move(loaded);
    t.file_path = path;
    t.name = fs::path(path).stem().string();
    t.selected_cell = 0;
    tabs_.push_back(std::move(t));
    active_tab_idx_ = static_cast<int>(tabs_.size()) - 1;
    active_edit_cell_ = -1;
    set_status("Opened: " + fs::path(path).filename().string());
}

void CodeEditorScreen::save_current() {
    flush_edit_buffer();
    if (tab().file_path.empty()) {
        std::string path = native_save_dialog(
            "Jupyter Notebooks (*.ipynb)\0*.ipynb\0All Files\0*.*\0",
            "Save Notebook");
        if (!path.empty()) {
            save_notebook_to(path);
        } else {
            show_save_dialog_ = true;
            std::memset(path_buf_, 0, sizeof(path_buf_));
        }
    } else {
        save_notebook_to(tab().file_path);
    }
}

void CodeEditorScreen::save_notebook_to(const std::string& path) {
    if (io::save_notebook(path, nb())) {
        tab().file_path = path;
        tab().name = fs::path(path).stem().string();
        tab().unsaved = false;
        set_status("Saved: " + fs::path(path).filename().string());
    } else {
        set_status("Failed to save");
    }
}

void CodeEditorScreen::export_to_py(const std::string& path) {
    flush_edit_buffer();
    if (io::export_to_python(path, nb())) {
        set_status("Exported: " + fs::path(path).filename().string());
    } else {
        set_status("Export failed");
    }
}

// Auto-save (point 10)
void CodeEditorScreen::auto_save() {
    for (auto& t : tabs_) {
        if (t.unsaved && !t.file_path.empty()) {
            std::string autosave_path = t.file_path + ".autosave";
            io::save_notebook(autosave_path, t.notebook);
        }
    }
}

} // namespace fincept::code_editor
