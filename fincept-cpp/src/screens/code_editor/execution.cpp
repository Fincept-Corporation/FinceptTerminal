#include "code_editor_screen.h"
#include "editor_internal.h"
#include "python/python_runner.h"
#include "core/logger.h"
#include "core/notification.h"
#include <string>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <future>
#include <filesystem>

namespace fs = std::filesystem;

namespace fincept::code_editor {

// =============================================================================
// Execution (with timer — point 8, persistent session — point 2)
// =============================================================================
void CodeEditorScreen::run_cell(int idx) {
    file_log("run_cell() ENTER idx=" + std::to_string(idx));
    try {
    auto& cells = nb().cells;
    file_log("run_cell() cells.size()=" + std::to_string(cells.size()));

    if (idx < 0 || idx >= static_cast<int>(cells.size())) {
        add_debug("ERROR: idx out of range, cells.size()=" + std::to_string(cells.size()));
        return;
    }
    auto& cell = cells[idx];
    if (cell.cell_type != "code" || cell.source.empty()) {
        add_debug("ERROR: cell not code or source empty, type=" + cell.cell_type +
                  " source_len=" + std::to_string(cell.source.size()));
        return;
    }
    if (exec_pending_) {
        add_debug("ERROR: exec_pending_ is true, another cell is still running");
        return;
    }

    flush_edit_buffer();
    running_cell_ = idx;
    exec_pending_ = true;

    int exec_num = tab().execution_counter++;
    int tab_idx = active_tab_idx_;
    std::string source = cell.source;

    add_debug("Cell source (" + std::to_string(source.size()) + " chars): " +
              source.substr(0, 80) + (source.size() > 80 ? "..." : ""));

    // Persistent session (point 2): gather all previously executed cells' source
    // and prepend to current cell so variables carry over
    std::string full_source;
    int prepended = 0;
    for (int i = 0; i < idx; i++) {
        if (cells[i].cell_type == "code" && cells[i].execution_count >= 0 && !cells[i].source.empty()) {
            full_source += cells[i].source;
            if (!cells[i].source.empty() && cells[i].source.back() != '\n') full_source += '\n';
            prepended++;
        }
    }
    full_source += source;
    add_debug("Prepended " + std::to_string(prepended) + " prior cells, total source: " +
              std::to_string(full_source.size()) + " chars");

    exec_future_ = std::async(std::launch::async, [this, idx, exec_num, tab_idx, full_source]() {
        try {
            file_log("async: ENTER for cell " + std::to_string(idx));

            auto t_start = std::chrono::steady_clock::now();

            file_log("async: writing temp script...");
            std::string tmp_path = write_temp_script(full_source);
            file_log("async: temp script written: " + tmp_path);

            // Log the resolved python path
            file_log("async: resolving python path...");
            fs::path py = python::resolve_python_path("");
            file_log("async: resolve_python_path('') = '" + py.string() + "' exists=" +
                      (py.empty() ? "empty" : (fs::exists(py) ? "YES" : "NO")));

            file_log("async: calling run_cell_subprocess...");
            python::PythonResult result = run_cell_subprocess(tmp_path);

            file_log("async: subprocess returned, success=" + std::string(result.success ? "true" : "false") +
                      " exit_code=" + std::to_string(result.exit_code));
            if (!result.output.empty()) {
                std::string preview = result.output.substr(0, 200);
                file_log("async: output (" + std::to_string(result.output.size()) + " chars): " + preview);
            }
            if (!result.error.empty()) {
                std::string preview = result.error.substr(0, 300);
                file_log("async: ERROR output: " + preview);
            }

            try { fs::remove(tmp_path); } catch (...) {}
            auto t_end = std::chrono::steady_clock::now();
            double elapsed_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
            file_log("async: execution took " + std::to_string(static_cast<int>(elapsed_ms)) + "ms");

            file_log("async: locking result_mutex_ to push result...");
            std::lock_guard<std::mutex> lock(result_mutex_);
            PendingResult pr;
            pr.tab_idx = tab_idx;
            pr.cell_idx = idx;
            pr.exec_num = exec_num;
            pr.success = result.success;
            pr.output = result.output;
            pr.error = result.error;
            pr.elapsed_ms = elapsed_ms;
            pending_results_.push_back(std::move(pr));
            file_log("async: result pushed OK, EXIT");
        } catch (const std::exception& e) {
            file_log("async: EXCEPTION: " + std::string(e.what()));
            // Still push an error result so UI doesn't hang
            try {
                std::lock_guard<std::mutex> lock(result_mutex_);
                PendingResult pr;
                pr.tab_idx = tab_idx;
                pr.cell_idx = idx;
                pr.exec_num = exec_num;
                pr.success = false;
                pr.error = std::string("Internal error: ") + e.what();
                pr.elapsed_ms = 0;
                pending_results_.push_back(std::move(pr));
            } catch (...) {}
        } catch (...) {
            file_log("async: UNKNOWN EXCEPTION");
            try {
                std::lock_guard<std::mutex> lock(result_mutex_);
                PendingResult pr;
                pr.tab_idx = tab_idx;
                pr.cell_idx = idx;
                pr.exec_num = exec_num;
                pr.success = false;
                pr.error = "Internal unknown error";
                pr.elapsed_ms = 0;
                pending_results_.push_back(std::move(pr));
            } catch (...) {}
        }
    });
    file_log("run_cell() async launched OK");
    } catch (const std::exception& e) {
        file_log("run_cell() EXCEPTION: " + std::string(e.what()));
        exec_pending_ = false;
        running_cell_ = -1;
    } catch (...) {
        file_log("run_cell() UNKNOWN EXCEPTION");
        exec_pending_ = false;
        running_cell_ = -1;
    }
}

void CodeEditorScreen::run_all_cells() {
    if (exec_pending_) return;
    flush_edit_buffer();

    running_all_ = true;

    struct CellJob { int idx; int exec_num; std::string source; };
    std::vector<CellJob> jobs;
    for (int i = 0; i < static_cast<int>(nb().cells.size()); i++) {
        auto& c = nb().cells[i];
        if (c.cell_type != "code" || c.source.empty()) continue;
        jobs.push_back({i, tab().execution_counter++, c.source});
    }
    int tab_idx = active_tab_idx_;

    exec_future_ = std::async(std::launch::async, [this, jobs = std::move(jobs), tab_idx]() {
        // Persistent session: accumulate source for sequential execution
        std::string accumulated;

        for (auto& job : jobs) {
            if (!running_all_) break;

            running_cell_ = job.idx;
            accumulated += job.source;
            if (!job.source.empty() && job.source.back() != '\n') accumulated += '\n';

            auto t_start = std::chrono::steady_clock::now();
            std::string tmp_path = write_temp_script(accumulated);
            python::PythonResult result = run_cell_subprocess(tmp_path);
            try { fs::remove(tmp_path); } catch (...) {}
            auto t_end = std::chrono::steady_clock::now();
            double elapsed_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

            std::lock_guard<std::mutex> lock(result_mutex_);
            PendingResult pr;
            pr.tab_idx = tab_idx;
            pr.cell_idx = job.idx;
            pr.exec_num = job.exec_num;
            pr.success = result.success;
            pr.output = result.output;
            pr.error = result.error;
            pr.elapsed_ms = elapsed_ms;
            pending_results_.push_back(std::move(pr));

            if (!result.success) break; // stop on error
        }
        running_cell_ = -1;
        running_all_ = false;
    });

    exec_pending_ = true;
}

void CodeEditorScreen::interrupt_execution() {
    running_all_ = false;
    set_status("Execution interrupted");
}

} // namespace fincept::code_editor
