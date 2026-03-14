#pragma once
// Code Editor screen — Python notebook interface with cell execution
// Features: syntax highlighting, persistent session, multi-tab, undo/redo,
// markdown rendering, execution timer, output scroll cap, native file dialog,
// find/replace, line numbers, keyboard navigation, export to .py, pip install,
// cell drag reorder, auto-save, execution order warnings, cell queue

#include "notebook_types.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <future>
#include <mutex>
#include <atomic>
#include <chrono>

namespace fincept::code_editor {

class CodeEditorScreen {
public:
    void render();
    void add_debug(const std::string& msg); // public so static helper can call it

private:
    // === Multi-tab notebook state ===
    std::vector<NotebookTab> tabs_;
    int active_tab_idx_ = 0;
    int tab_counter_ = 1;
    bool initialized_ = false;

    // Convenience accessors for active tab
    NotebookTab& tab();
    Notebook& nb();

    // === Cell state ===
    std::atomic<int> running_cell_{-1};
    std::atomic<bool> running_all_{false};

    // === Python persistent session ===
    std::string python_version_;
    std::future<void> exec_future_;
    bool exec_pending_ = false;
    std::string status_message_;
    double status_message_time_ = 0;

    // Thread-safe result staging
    struct PendingResult {
        int tab_idx = 0;
        int cell_idx = -1;
        int exec_num = 0;
        bool success = false;
        std::string output;
        std::string error;
        double elapsed_ms = 0;
    };
    std::mutex result_mutex_;
    std::vector<PendingResult> pending_results_;

    // === Edit buffer ===
    int active_edit_cell_ = -1;
    std::vector<char> edit_buf_;

    // === Search / Find-Replace (point 13) ===
    bool show_search_ = false;
    char search_buf_[256] = {};
    char replace_buf_[256] = {};
    std::vector<SearchMatch> search_matches_;
    int current_match_ = -1;
    bool search_case_sensitive_ = false;

    // === File dialogs ===
    char path_buf_[512] = {};
    bool show_open_dialog_ = false;
    bool show_save_dialog_ = false;
    bool show_export_dialog_ = false;
    bool show_pip_dialog_ = false;
    char pip_pkg_buf_[256] = {};

    // === Auto-save (point 10) ===
    double last_autosave_time_ = 0;
    static constexpr double AUTOSAVE_INTERVAL = 60.0; // seconds

    // === Drag reorder (point 7) ===
    int drag_source_cell_ = -1;

    // === Debug log ===
    bool show_debug_log_ = false;
    struct DebugEntry {
        std::string timestamp;
        std::string message;
    };
    std::mutex debug_mutex_;
    std::vector<DebugEntry> debug_log_;

    // === Render methods ===
    void render_debug_panel();
    void render_tab_bar();
    void render_toolbar();
    void render_cells();
    void render_cell(int idx);
    void render_cell_header(int idx);
    void render_cell_editor(int idx);
    void render_cell_line_numbers(const std::string& source, float height);
    void render_cell_preview_code(int idx);
    void render_cell_preview_markdown(int idx);
    void render_cell_output(int idx);
    void render_add_cell_divider(int after_idx);
    void render_status_bar();
    void render_file_dialog();
    void render_search_bar();
    void render_pip_dialog();

    // === Syntax highlighting (point 1) ===
    static std::vector<SyntaxToken> tokenize_python(const std::string& source);
    void render_highlighted_text(const std::string& source, float wrap_width);

    // === Markdown rendering (point 6) ===
    void render_markdown(const std::string& source);

    // === Cell operations ===
    void add_cell(int after_idx, const std::string& type = "code");
    void delete_cell(int idx);
    void move_cell(int idx, int direction);
    void toggle_cell_type(int idx);
    void clear_cell_output(int idx);
    void clear_all_outputs();

    // === Undo/Redo (point 5) ===
    void push_undo(const std::string& description);
    void undo();
    void redo();

    // === Execution ===
    void run_cell(int idx);
    void run_all_cells();
    void interrupt_execution();

    // === File operations ===
    void new_tab();
    void close_tab(int idx);
    void open_notebook_dialog();
    void open_notebook(const std::string& path);
    void save_current();
    void save_notebook_to(const std::string& path);
    void export_to_py(const std::string& path);
    void auto_save();

    // === Search (point 13) ===
    void perform_search();
    void replace_current();
    void replace_all();
    void navigate_match(int direction);

    // === Pip install (point 14) ===
    void pip_install(const std::string& package);

    // === Keyboard navigation (point 17) ===
    void handle_keyboard_navigation();

    // === Native file dialog (point 11) ===
    static std::string native_open_dialog(const char* filter, const char* title);
    static std::string native_save_dialog(const char* filter, const char* title);

    // === Utility ===
    void detect_python_version();
    void set_status(const std::string& msg);
    void flush_edit_buffer();
    bool has_execution_order_warning(int idx);
};

} // namespace fincept::code_editor
