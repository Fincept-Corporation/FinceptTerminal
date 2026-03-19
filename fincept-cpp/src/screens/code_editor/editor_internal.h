#pragma once
// editor_internal.h — private implementation helpers shared across code_editor .cpp files.
// NOT part of the public API. Include only from code_editor .cpp files.

#include "notebook_types.h"
#include "python/python_runner.h"
#include <string>
#include <vector>
#include <mutex>
#include <set>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <cerrno>

namespace fs = std::filesystem;

namespace fincept::code_editor {

// Forward declaration so debug_log can reference the class
class CodeEditorScreen;

// =============================================================================
// Crash-safe file logger — writes directly to file, never throws (thread-safe)
// =============================================================================
inline std::mutex s_file_log_mutex;

static inline void file_log(const std::string& msg) {
    try {
        std::lock_guard<std::mutex> lock(s_file_log_mutex);
        static std::ofstream log_file;
        if (!log_file.is_open()) {
            log_file.open("C:/windowsdisk/finceptTerminal/fincept-cpp/debug_code_editor.log",
                          std::ios::trunc);
        }
        if (log_file.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count() % 100000;
            log_file << "[" << ms << "] " << msg << "\n";
            log_file.flush();
        }
    } catch (...) {}
}

// =============================================================================
// Debug log — writes to file log and forwards to the screen's debug panel.
// g_debug_instance must be set to a live CodeEditorScreen* before use.
// Files that call debug_log must #include "code_editor_screen.h" for the full type.
// =============================================================================
inline CodeEditorScreen* g_debug_instance = nullptr;

// debug_log is defined in editor_utils.cpp (needs full CodeEditorScreen type).
// Other TUs that need it must include editor_utils.h or call file_log directly.
// For use within execution.cpp / run_cell, just call file_log — it's sufficient.
void debug_log(const std::string& msg);

// =============================================================================
// Helper: Jupyter-like auto-display for last expression
// Wraps the last bare expression in print() so users get Jupyter-like output.
// =============================================================================
static inline std::string jupyter_auto_display(const std::string& source) {
    std::vector<std::string> lines;
    std::istringstream ss(source);
    std::string line;
    while (std::getline(ss, line)) lines.push_back(line);

    int last_idx = -1;
    for (int i = static_cast<int>(lines.size()) - 1; i >= 0; i--) {
        std::string trimmed = lines[i];
        size_t start = trimmed.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        trimmed = trimmed.substr(start);
        if (trimmed[0] == '#') continue;
        last_idx = i;
        break;
    }

    if (last_idx < 0) return source;

    std::string trimmed = lines[last_idx];
    size_t start = trimmed.find_first_not_of(" \t");
    if (start == std::string::npos) return source;
    std::string content = trimmed.substr(start);
    std::string indent = trimmed.substr(0, start);

    if (!indent.empty()) return source;

    static const std::set<std::string> STMT_PREFIXES = {
        "import ", "from ", "def ", "class ", "if ", "elif ", "else:",
        "for ", "while ", "try:", "except ", "except:", "finally:",
        "with ", "return ", "raise ", "pass", "break", "continue",
        "del ", "global ", "nonlocal ", "assert ", "yield ", "async ",
        "await ", "print(", "print ("
    };
    for (auto& prefix : STMT_PREFIXES) {
        if (content.substr(0, prefix.size()) == prefix) return source;
        if (content == "pass" || content == "break" || content == "continue") return source;
    }

    bool has_assign = false;
    for (size_t i = 0; i < content.size(); i++) {
        if (content[i] == '=' && i + 1 < content.size() && content[i + 1] != '=' &&
            (i == 0 || (content[i - 1] != '!' && content[i - 1] != '<' &&
             content[i - 1] != '>' && content[i - 1] != '='))) {
            int depth = 0;
            for (size_t j = 0; j < i; j++) {
                if (content[j] == '(' || content[j] == '[' || content[j] == '{') depth++;
                if (content[j] == ')' || content[j] == ']' || content[j] == '}') depth--;
            }
            if (depth == 0) { has_assign = true; break; }
        }
    }
    if (has_assign) return source;

    if (!content.empty() && content.back() == ':') return source;

    lines[last_idx] = "print(" + content + ")";

    std::string result;
    for (size_t i = 0; i < lines.size(); i++) {
        result += lines[i];
        if (i + 1 < lines.size()) result += '\n';
    }
    return result;
}

// =============================================================================
// Helper: write source to a temp .py file, return the path
// =============================================================================
static inline std::string write_temp_script(const std::string& source) {
    try {
        std::string transformed = jupyter_auto_display(source);
        file_log("write_temp_script: source_len=" + std::to_string(source.size()) +
                  " transformed_len=" + std::to_string(transformed.size()));
        fs::path tmp_dir = fs::temp_directory_path();
        fs::path tmp_file = tmp_dir / "fincept_cell.py";
        std::ofstream f(tmp_file);
        if (f.is_open()) { f << transformed; f.close(); }
        else { file_log("write_temp_script: FAILED to open file for writing!"); }
        return tmp_file.string();
    } catch (const std::exception& e) {
        file_log("write_temp_script: EXCEPTION: " + std::string(e.what()));
        return "";
    }
}

// =============================================================================
// Subprocess execution: run a .py file via Python interpreter, return result
// =============================================================================
static inline python::PythonResult run_cell_subprocess(const std::string& tmp_path) {
    python::PythonResult result;

    try {
        file_log("subprocess: ENTER, tmp_path=" + tmp_path);

        file_log("subprocess: calling resolve_python_path...");
        fs::path py = python::resolve_python_path("");
        file_log("subprocess: resolve_python_path('') = '" + py.string() + "'");

        if (py.empty() || !fs::exists(py)) {
#ifdef _WIN32
            py = "python";
#else
            py = "python3";
#endif
            file_log("subprocess: falling back to '" + py.string() + "'");
        }

#ifdef _WIN32
        std::string cmd = "\"\"" + py.string() + "\" -u -B \"" + tmp_path + "\" 2>&1\"";
#else
        std::string cmd = "\"" + py.string() + "\" -u -B \"" + tmp_path + "\" 2>&1";
#endif
        file_log("subprocess: CMD = " + cmd);

        file_log("subprocess: calling _popen...");
#ifdef _WIN32
        FILE* pipe = _popen(cmd.c_str(), "r");
#else
        FILE* pipe = popen(cmd.c_str(), "r");
#endif
        if (!pipe) {
            result.error = "Failed to start Python subprocess";
            file_log("subprocess: FAIL - _popen returned NULL, errno=" + std::to_string(errno));
            return result;
        }
        file_log("subprocess: pipe opened OK, reading output...");

        std::string output;
        char buffer[4096];
        int read_count = 0;
        while (std::fgets(buffer, sizeof(buffer), pipe)) {
            output += buffer;
            read_count++;
        }
        file_log("subprocess: read " + std::to_string(output.size()) + " bytes in " +
                  std::to_string(read_count) + " chunks from pipe");

        file_log("subprocess: calling _pclose...");
#ifdef _WIN32
        int status = _pclose(pipe);
#else
        int status = pclose(pipe);
        status = WEXITSTATUS(status);
#endif

        file_log("subprocess: _pclose returned status=" + std::to_string(status));

        result.exit_code = status;
        if (status == 0) {
            result.success = true;
            result.output = output;
            file_log("subprocess: SUCCESS");
        } else {
            result.error = output.empty() ? "Python exited with code " + std::to_string(status) : output;
            file_log("subprocess: FAIL - exit code " + std::to_string(status) +
                      " output_len=" + std::to_string(output.size()));
        }
    } catch (const std::exception& e) {
        file_log("subprocess: EXCEPTION: " + std::string(e.what()));
        result.error = std::string("Subprocess exception: ") + e.what();
    } catch (...) {
        file_log("subprocess: UNKNOWN EXCEPTION");
        result.error = "Subprocess unknown exception";
    }

    file_log("subprocess: EXIT");
    return result;
}

} // namespace fincept::code_editor
