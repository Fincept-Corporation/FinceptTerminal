#pragma once
// Python subprocess runner — production-grade persistent worker pool
//
// Architecture:
//   - Persistent worker processes (1 per venv) stay alive for app lifetime
//   - Workers pre-import heavy libraries (pandas, numpy, yfinance) at startup
//   - Requests sent via stdin as newline-delimited JSON (NDJSON)
//   - Responses received via stdout as NDJSON
//   - Warm interpreter: ~50ms per call vs ~3000ms cold start
//   - Concurrency limiter prevents resource exhaustion
//   - Automatic worker restart on crash
//   - Graceful shutdown on app exit

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

fs::path get_install_dir();
fs::path get_exe_dir();
std::string get_venv_name(const std::string& script);
fs::path resolve_python_path(const std::string& script);
fs::path resolve_script_path(const std::string& script);
bool is_python_available();
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

/// Execute a Python script and return JSON result (blocking, uses worker pool)
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
// Worker pool lifecycle
// ============================================================================

/// Initialize the persistent worker pool (call once at app startup)
void init_worker_pool();

/// Shutdown all workers gracefully (call at app shutdown)
void shutdown_worker_pool();

// ============================================================================
// Utility
// ============================================================================

/// Extract last JSON line/block from Python output
std::string extract_json(const std::string& output);

} // namespace fincept::python
