#pragma once
// Python subprocess runner — mirrors python.rs from the Tauri app
// Resolves Python path, script path, executes scripts via subprocess, extracts JSON
// Non-blocking async API available to avoid UI thread stalls

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <filesystem>
#include <future>

namespace fincept::python {

namespace fs = std::filesystem;

// Scripts requiring NumPy 1.x venv
inline const std::vector<std::string> NUMPY1_SCRIPTS = {
    "vectorbt", "backtesting", "gluonts", "functime",
    "pyportfolioopt", "financepy", "ffn", "ffn_wrapper"
};

// ============================================================================
// Path resolution
// ============================================================================

/// Get the app data directory (production path only)
/// Windows: %APPDATA%/com.fincept.terminal/
/// macOS:   ~/Library/Application Support/com.fincept.terminal/
/// Linux:   ~/.local/share/com.fincept.terminal/
fs::path get_install_dir();

/// Get the directory containing the running executable
fs::path get_exe_dir();

/// Determine which venv to use for a given script
std::string get_venv_name(const std::string& script);

/// Resolve the Python executable path for a given script
/// Checks venv first, falls back to system Python (result is cached)
fs::path resolve_python_path(const std::string& script);

/// Resolve the script path relative to the exe directory
/// Searches: exe_dir/scripts/, exe_dir/resources/scripts/, CWD/scripts/
fs::path resolve_script_path(const std::string& script);

/// Check if Python is available (any venv or system)
bool is_python_available();

/// Clear the cached Python path lookups (call after venv setup changes)
void clear_path_cache();

// ============================================================================
// Subprocess execution
// ============================================================================

struct PythonResult {
    bool success = false;
    std::string output;     // Extracted JSON or raw stdout
    std::string error;      // Error message if failed
    int exit_code = -1;
};

/// Execute a Python script and return JSON result (blocking)
PythonResult execute(const std::string& script, const std::vector<std::string>& args);

/// Execute a Python script synchronously (same as execute, explicit name)
PythonResult execute_sync(const std::string& script, const std::vector<std::string>& args);

/// Execute with stdin data (for large payloads, blocking)
PythonResult execute_with_stdin(const std::string& script,
                                const std::vector<std::string>& args,
                                const std::string& stdin_data);

/// Execute a Python script asynchronously — returns a future, never blocks the caller
std::future<PythonResult> execute_async(const std::string& script,
                                         const std::vector<std::string>& args);

/// Execute with stdin data asynchronously — returns a future
std::future<PythonResult> execute_async_with_stdin(const std::string& script,
                                                    const std::vector<std::string>& args,
                                                    const std::string& stdin_data);

// ============================================================================
// Utility
// ============================================================================

/// Extract last JSON line/block from Python output
std::string extract_json(const std::string& output);

} // namespace fincept::python
