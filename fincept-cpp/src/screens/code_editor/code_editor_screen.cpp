#include "code_editor_screen.h"
#include "editor_internal.h"
#include "ui/theme.h"
#include "ui/yoga_helpers.h"
#include "core/config.h"
#include <imgui.h>
#include <chrono>
#include <mutex>
#include <future>
#include <string>

namespace fincept::code_editor {

// =============================================================================
// Convenience accessors for active tab
// =============================================================================
NotebookTab& CodeEditorScreen::tab() { return tabs_[active_tab_idx_]; }
Notebook& CodeEditorScreen::nb() { return tabs_[active_tab_idx_].notebook; }

// =============================================================================
// Main render — orchestrates all sub-panels
// =============================================================================
void CodeEditorScreen::render() {
    using namespace theme::colors;

    g_debug_instance = this;

    try {

    if (!initialized_) {
        file_log("render() initializing...");
        new_tab();
        detect_python_version();
        last_autosave_time_ = ImGui::GetTime();
        initialized_ = true;
        file_log("render() initialized OK");
    }

    if (tabs_.empty()) { new_tab(); }

    // Check async execution result — MUST catch exceptions from async thread
    if (exec_future_.valid()) {
        auto status = exec_future_.wait_for(std::chrono::milliseconds(0));
        if (status == std::future_status::ready) {
            file_log("render() exec_future_ ready, calling .get()...");
            try {
                exec_future_.get();
                file_log("render() exec_future_.get() OK");
            } catch (const std::exception& e) {
                file_log("render() EXCEPTION from exec_future_.get(): " + std::string(e.what()));
            } catch (...) {
                file_log("render() UNKNOWN EXCEPTION from exec_future_.get()");
            }
            exec_pending_ = false;
            running_cell_ = -1;
        }
    }

    // Apply pending results from background thread (thread-safe)
    {
        std::lock_guard<std::mutex> lock(result_mutex_);
        for (auto& pr : pending_results_) {
            file_log("Applying result: tab=" + std::to_string(pr.tab_idx) +
                      " cell=" + std::to_string(pr.cell_idx) +
                      " success=" + (pr.success ? "true" : "false") +
                      " elapsed=" + std::to_string(static_cast<int>(pr.elapsed_ms)) + "ms");
            try {
                if (pr.tab_idx >= 0 && pr.tab_idx < static_cast<int>(tabs_.size())) {
                    auto& t = tabs_[pr.tab_idx];
                    if (pr.cell_idx >= 0 && pr.cell_idx < static_cast<int>(t.notebook.cells.size())) {
                        auto& c = t.notebook.cells[pr.cell_idx];
                        c.outputs.clear();
                        c.execution_count = pr.exec_num;
                        c.exec_time_ms = pr.elapsed_ms;
                        if (pr.success) {
                            if (!pr.output.empty()) {
                                CellOutput out;
                                out.output_type = "stream";
                                out.text = pr.output;
                                c.outputs.push_back(std::move(out));
                            }
                        } else {
                            CellOutput out;
                            out.output_type = "error";
                            out.ename = "ExecutionError";
                            out.evalue = pr.error.empty() ? "Unknown error" : pr.error;
                            out.traceback = pr.error;
                            c.outputs.push_back(std::move(out));
                        }
                        file_log("Result applied OK for cell " + std::to_string(pr.cell_idx));
                    } else {
                        file_log("WARN: cell_idx out of range: " + std::to_string(pr.cell_idx));
                    }
                } else {
                    file_log("WARN: tab_idx out of range: " + std::to_string(pr.tab_idx));
                }
            } catch (const std::exception& e) {
                file_log("EXCEPTION applying result: " + std::string(e.what()));
            }
        }
        pending_results_.clear();
    }

    // Auto-save (point 10)
    double now = ImGui::GetTime();
    if (now - last_autosave_time_ > AUTOSAVE_INTERVAL) {
        auto_save();
        last_autosave_time_ = now;
    }

    // Clear stale status messages
    if (!status_message_.empty() && now - status_message_time_ > 5.0) {
        status_message_.clear();
    }

    // Handle keyboard navigation (point 17)
    handle_keyboard_navigation();

    ui::ScreenFrame frame("##code_editor_screen", ImVec2(0, 0), BG_DARK);
    if (!frame.begin()) { frame.end(); return; }

    render_tab_bar();
    render_toolbar();
    if (show_search_) render_search_bar();
    render_cells();
    render_status_bar();
    render_file_dialog();
    render_pip_dialog();

    frame.end();

    } catch (const std::exception& e) {
        file_log("FATAL EXCEPTION in render(): " + std::string(e.what()));
    } catch (...) {
        file_log("FATAL UNKNOWN EXCEPTION in render()");
    }
}

} // namespace fincept::code_editor
