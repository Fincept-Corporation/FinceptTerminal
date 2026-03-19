#include "code_editor_screen.h"
#include "editor_internal.h"
#include "python/python_runner.h"
#include <string>
#include <vector>
#include <algorithm>
#include <thread>
#include <filesystem>
#include <cstdio>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace fincept::code_editor {

// =============================================================================
// Search (point 13)
// =============================================================================
void CodeEditorScreen::perform_search() {
    search_matches_.clear();
    current_match_ = -1;

    std::string query(search_buf_);
    if (query.empty()) return;

    auto& cells = nb().cells;
    for (int i = 0; i < static_cast<int>(cells.size()); i++) {
        const auto& src = cells[i].source;
        std::string hay = src;
        std::string needle = query;

        if (!search_case_sensitive_) {
            std::transform(hay.begin(), hay.end(), hay.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            std::transform(needle.begin(), needle.end(), needle.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        }

        size_t pos = 0;
        while ((pos = hay.find(needle, pos)) != std::string::npos) {
            search_matches_.push_back({i, static_cast<int>(pos), static_cast<int>(query.size())});
            pos += query.size();
        }
    }

    if (!search_matches_.empty()) {
        current_match_ = 0;
        tab().selected_cell = search_matches_[0].cell_idx;
        active_edit_cell_ = -1;
    }
    set_status(std::to_string(search_matches_.size()) + " matches found");
}

void CodeEditorScreen::navigate_match(int direction) {
    if (search_matches_.empty()) return;
    current_match_ = (current_match_ + direction + static_cast<int>(search_matches_.size())) %
                      static_cast<int>(search_matches_.size());
    tab().selected_cell = search_matches_[current_match_].cell_idx;
    active_edit_cell_ = -1;
}

void CodeEditorScreen::replace_current() {
    if (current_match_ < 0 || current_match_ >= static_cast<int>(search_matches_.size())) return;
    auto& m = search_matches_[current_match_];
    if (m.cell_idx < 0 || m.cell_idx >= static_cast<int>(nb().cells.size())) return;

    push_undo("replace");
    auto& src = nb().cells[m.cell_idx].source;
    std::string repl(replace_buf_);
    src = src.substr(0, m.char_offset) + repl + src.substr(m.char_offset + m.length);
    tab().unsaved = true;
    active_edit_cell_ = -1;
    perform_search(); // refresh matches
}

void CodeEditorScreen::replace_all() {
    if (search_matches_.empty()) return;
    push_undo("replace all");

    std::string query(search_buf_);
    std::string repl(replace_buf_);
    if (query.empty()) return;

    for (auto& cell : nb().cells) {
        if (search_case_sensitive_) {
            size_t pos = 0;
            while ((pos = cell.source.find(query, pos)) != std::string::npos) {
                cell.source.replace(pos, query.size(), repl);
                pos += repl.size();
            }
        } else {
            std::string lower_src = cell.source;
            std::string lower_q = query;
            std::transform(lower_q.begin(), lower_q.end(), lower_q.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            // Replace from end to start to preserve offsets
            std::vector<size_t> positions;
            std::transform(lower_src.begin(), lower_src.end(), lower_src.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            size_t pos = 0;
            while ((pos = lower_src.find(lower_q, pos)) != std::string::npos) {
                positions.push_back(pos);
                pos += lower_q.size();
            }
            for (auto it = positions.rbegin(); it != positions.rend(); ++it) {
                cell.source.replace(*it, query.size(), repl);
            }
        }
    }

    tab().unsaved = true;
    active_edit_cell_ = -1;
    perform_search();
    set_status("Replaced all");
}

// =============================================================================
// Pip install (point 14)
// =============================================================================
void CodeEditorScreen::pip_install(const std::string& package) {
    set_status("Installing " + package + "...");

    std::thread([this, package]() {
        fs::path py = python::resolve_python_path("");
        if (py.empty() || !fs::exists(py)) {
#ifdef _WIN32
            py = "python";
#else
            py = "python3";
#endif
        }

#ifdef _WIN32
        std::string cmd = "\"\"" + py.string() + "\" -m pip install " + package + " 2>&1\"";
#else
        std::string cmd = "\"" + py.string() + "\" -m pip install " + package + " 2>&1";
#endif

#ifdef _WIN32
        FILE* pipe = _popen(cmd.c_str(), "r");
#else
        FILE* pipe = popen(cmd.c_str(), "r");
#endif
        if (!pipe) { set_status("pip: failed to start"); return; }

        std::string output;
        char buf[4096];
        while (std::fgets(buf, sizeof(buf), pipe)) output += buf;

#ifdef _WIN32
        int status = _pclose(pipe);
#else
        int status = pclose(pipe);
        status = WEXITSTATUS(status);
#endif

        if (status == 0) {
            set_status("Installed: " + package);
        } else {
            set_status("pip install failed: " + package);
        }
    }).detach();
}

} // namespace fincept::code_editor
