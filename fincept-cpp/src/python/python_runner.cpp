// Python subprocess runner — C++ port of python.rs
// Resolves paths, runs Python scripts, extracts JSON output
// Optimized: cached path resolution, concurrent pipe reads, async API

#include "python_runner.h"
#include "core/logger.h"
#include <sstream>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#endif

namespace fincept::python {

// ============================================================================
// Path cache — avoid repeated filesystem + subprocess lookups
// ============================================================================

static std::mutex s_path_cache_mutex;
static std::unordered_map<std::string, fs::path> s_python_path_cache;
static fs::path s_install_dir_cache;
static fs::path s_exe_dir_cache;
static bool s_dirs_cached = false;

static void ensure_dir_cache() {
    if (s_dirs_cached) return;
    // These never change during the lifetime of the process
    s_install_dir_cache = get_install_dir();
    s_exe_dir_cache = get_exe_dir();
    s_dirs_cached = true;
}

void clear_path_cache() {
    std::lock_guard<std::mutex> lock(s_path_cache_mutex);
    s_python_path_cache.clear();
    s_dirs_cached = false;
}

// ============================================================================
// Path resolution
// ============================================================================

fs::path get_install_dir() {
#ifdef _WIN32
    char* appdata = nullptr;
    size_t len = 0;
    if (_dupenv_s(&appdata, &len, "APPDATA") == 0 && appdata) {
        fs::path dir = fs::path(appdata) / "com.fincept.terminal";
        free(appdata);
        return dir;
    }
    return fs::path("C:/Users/Default/AppData/Roaming/com.fincept.terminal");
#elif defined(__APPLE__)
    const char* home = getenv("HOME");
    if (home) {
        return fs::path(home) / "Library" / "Application Support" / "com.fincept.terminal";
    }
    return fs::path("/tmp/com.fincept.terminal");
#else
    const char* home = getenv("HOME");
    if (home) {
        return fs::path(home) / ".local" / "share" / "com.fincept.terminal";
    }
    return fs::path("/tmp/com.fincept.terminal");
#endif
}

fs::path get_exe_dir() {
#ifdef _WIN32
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return fs::path(buf).parent_path();
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) {
        return fs::canonical(buf).parent_path();
    }
    return fs::current_path();
#else
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        return fs::path(buf).parent_path();
    }
    return fs::current_path();
#endif
}

std::string get_venv_name(const std::string& script) {
    std::string lower = script;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    for (const auto& keyword : NUMPY1_SCRIPTS) {
        std::istringstream ss(lower);
        std::string component;
        while (std::getline(ss, component, '/')) {
            std::istringstream ss2(component);
            std::string part;
            while (std::getline(ss2, part, '\\')) {
                if (part == keyword ||
                    part.find(keyword + ".") == 0 ||
                    part.find(keyword + "_") == 0) {
                    return "venv-numpy1";
                }
            }
        }
    }
    return "venv-numpy2";
}

fs::path resolve_python_path(const std::string& script) {
    auto venv = get_venv_name(script);

    // Check cache first
    {
        std::lock_guard<std::mutex> lock(s_path_cache_mutex);
        auto it = s_python_path_cache.find(venv);
        if (it != s_python_path_cache.end()) {
            return it->second;
        }
    }

    ensure_dir_cache();

#ifdef _WIN32
    auto python = s_install_dir_cache / venv / "Scripts" / "python.exe";
#else
    auto python = s_install_dir_cache / venv / "bin" / "python3";
#endif

    if (fs::exists(python)) {
        std::lock_guard<std::mutex> lock(s_path_cache_mutex);
        s_python_path_cache[venv] = python;
        LOG_INFO("Python", "Cached python path for venv '%s': %s", venv.c_str(), python.string().c_str());
        return python;
    }

    // Fallback to system Python — check once and cache
#ifdef _WIN32
    const char* sys_python = "python";
#else
    const char* sys_python = "python3";
#endif

    std::string check_cmd = std::string(sys_python) + " --version";
#ifdef _WIN32
    FILE* pipe = _popen(check_cmd.c_str(), "r");
#else
    FILE* pipe = popen(check_cmd.c_str(), "r");
#endif
    if (pipe) {
        char buf[128];
        bool found = (fgets(buf, sizeof(buf), pipe) != nullptr);
#ifdef _WIN32
        _pclose(pipe);
#else
        pclose(pipe);
#endif
        if (found) {
            fs::path sys_path(sys_python);
            std::lock_guard<std::mutex> lock(s_path_cache_mutex);
            s_python_path_cache[venv] = sys_path;
            LOG_INFO("Python", "Cached system python for venv '%s'", venv.c_str());
            return sys_path;
        }
    }

    LOG_ERROR("Python", "No python found for venv '%s'", venv.c_str());
    return {};
}

fs::path resolve_script_path(const std::string& script) {
    ensure_dir_cache();

    std::vector<fs::path> candidates;
    candidates.reserve(6);

    candidates.push_back(s_exe_dir_cache / "scripts" / script);
    candidates.push_back(s_exe_dir_cache / "resources" / "scripts" / script);

#ifdef __APPLE__
    auto contents_dir = s_exe_dir_cache.parent_path();
    candidates.push_back(contents_dir / "Resources" / "scripts" / script);
#endif

    auto cwd = fs::current_path();
    candidates.push_back(cwd / "scripts" / script);
    candidates.push_back(cwd / "resources" / "scripts" / script);
    candidates.push_back(cwd / ".." / "fincept-terminal-desktop" / "src-tauri" / "resources" / "scripts" / script);

    for (const auto& p : candidates) {
        if (fs::exists(p)) {
            return fs::canonical(p);
        }
    }

    LOG_ERROR("Python", "Script '%s' NOT FOUND in %zu search paths", script.c_str(), candidates.size());
    return {};
}

bool is_python_available() {
    auto path = resolve_python_path("default");
    return !path.empty();
}

// ============================================================================
// JSON extraction
// ============================================================================

std::string extract_json(const std::string& output) {
    std::istringstream stream(output);
    std::string line;
    std::string last_json;

    while (std::getline(stream, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        char first = line[start];
        if (first == '{' || first == '[') {
            last_json = line.substr(start);
        }
    }

    if (!last_json.empty()) return last_json;
    return {};
}

// ============================================================================
// Subprocess execution
// ============================================================================

#ifdef _WIN32

// Helper: read an entire pipe handle into a string (used by reader threads)
static void read_pipe_to_string(HANDLE hPipe, std::string& out) {
    char buffer[4096];
    DWORD bytesRead;
    while (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        out.append(buffer, bytesRead);
    }
}

static PythonResult run_subprocess(const fs::path& python, const fs::path& script,
                                    const std::vector<std::string>& args,
                                    const std::string* stdin_data = nullptr) {
    PythonResult result;

    // Build command line
    std::string cmd = "\"" + python.string() + "\" -u -B \"" + script.string() + "\"";
    for (const auto& arg : args) {
        cmd += " \"" + arg + "\"";
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    // Create stdout pipe
    HANDLE hStdoutRead = nullptr, hStdoutWrite = nullptr;
    CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0);
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);

    // Create stderr pipe
    HANDLE hStderrRead = nullptr, hStderrWrite = nullptr;
    CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0);
    SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);

    // Create stdin pipe if needed
    HANDLE hStdinRead = nullptr, hStdinWrite = nullptr;
    if (stdin_data) {
        CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0);
        SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);
    }

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStderrWrite;
    si.hStdInput = stdin_data ? hStdinRead : nullptr;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi{};
    std::string cmd_line = cmd;

    BOOL ok = CreateProcessA(
        nullptr, cmd_line.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi
    );

    // Close write ends so reads will EOF when the child exits
    CloseHandle(hStdoutWrite);
    CloseHandle(hStderrWrite);
    if (stdin_data && hStdinRead) CloseHandle(hStdinRead);

    if (!ok) {
        result.error = "Failed to create process";
        if (hStdoutRead) CloseHandle(hStdoutRead);
        if (hStderrRead) CloseHandle(hStderrRead);
        if (stdin_data && hStdinWrite) CloseHandle(hStdinWrite);
        return result;
    }

    // Write stdin data (do this before reading to avoid deadlock)
    if (stdin_data && hStdinWrite) {
        DWORD written;
        WriteFile(hStdinWrite, stdin_data->c_str(), (DWORD)stdin_data->size(), &written, nullptr);
        CloseHandle(hStdinWrite);
    }

    // Read stdout and stderr CONCURRENTLY to prevent deadlock
    // If we read them sequentially, the child can block writing to stderr
    // while we're blocked reading stdout (or vice versa), causing a deadlock.
    std::string stdout_str;
    std::string stderr_str;

    std::thread stderr_reader([&]() {
        read_pipe_to_string(hStderrRead, stderr_str);
    });

    // Read stdout on this thread
    read_pipe_to_string(hStdoutRead, stdout_str);

    // Wait for stderr reader to finish
    stderr_reader.join();

    CloseHandle(hStdoutRead);
    CloseHandle(hStderrRead);

    // Wait for process with timeout
    DWORD wait_result = WaitForSingleObject(pi.hProcess, 120000); // 2 min

    DWORD exit_code = 0;
    if (wait_result == WAIT_TIMEOUT) {
        LOG_WARN("Python", "Process timed out after 120s, terminating");
        TerminateProcess(pi.hProcess, 1);
        exit_code = 1;
        result.error = "Script timed out after 120 seconds";
    } else {
        GetExitCodeProcess(pi.hProcess, &exit_code);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    result.exit_code = (int)exit_code;

    if (exit_code == 0) {
        result.success = true;
        auto json_str = extract_json(stdout_str);
        result.output = json_str.empty() ? stdout_str : json_str;
    } else {
        // Many Python scripts exit(1) but still write valid JSON to stdout.
        // Check stdout first for a structured JSON response.
        auto json_str = extract_json(stdout_str);
        if (!json_str.empty()) {
            try {
                auto j = nlohmann::json::parse(json_str);
                if (j.contains("success")) {
                    // Script returned a structured response — pass it through
                    // so callers can parse the error message from the JSON
                    result.success = true;  // let caller handle success/error
                    result.output = json_str;
                    return result;
                }
            } catch (...) {}
        }
        // Fall back to extracting error from stderr/stdout
        std::string extracted_error;
        for (const auto& src : {stderr_str, stdout_str}) {
            if (src.empty()) continue;
            auto js = extract_json(src);
            if (!js.empty()) {
                try {
                    auto j = nlohmann::json::parse(js);
                    if (j.contains("error") && j["error"].is_string()) {
                        extracted_error = j["error"].get<std::string>();
                        break;
                    }
                } catch (...) {}
            }
        }
        if (!extracted_error.empty()) {
            result.error = extracted_error;
        } else if (result.error.empty()) {
            result.error = "Script failed (exit " + std::to_string(exit_code) + ")";
            if (!stderr_str.empty()) result.error += ": " + stderr_str;
        }
    }

    return result;
}

#else // Unix (macOS/Linux)

static PythonResult run_subprocess(const fs::path& python, const fs::path& script,
                                    const std::vector<std::string>& args,
                                    const std::string* stdin_data = nullptr) {
    PythonResult result;

    std::string cmd = "\"" + python.string() + "\" -u -B \"" + script.string() + "\"";
    for (const auto& arg : args) {
        cmd += " \"" + arg + "\"";
    }
    cmd += " 2>&1";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        result.error = "Failed to execute subprocess";
        return result;
    }

    std::string output;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
    }

    int status = pclose(pipe);
    result.exit_code = WEXITSTATUS(status);

    if (result.exit_code == 0) {
        result.success = true;
        auto json_str = extract_json(output);
        result.output = json_str.empty() ? output : json_str;
    } else {
        result.error = "Script failed (exit " + std::to_string(result.exit_code) + "): " + output;
    }

    return result;
}

#endif

// ============================================================================
// Synchronous API
// ============================================================================

PythonResult execute(const std::string& script, const std::vector<std::string>& args) {
    auto python = resolve_python_path(script);
    if (python.empty()) {
        return {false, "", "Python not found. Run setup first.", -1};
    }

    auto script_path = resolve_script_path(script);
    if (script_path.empty()) {
        return {false, "", "Script '" + script + "' not found.", -1};
    }

    return run_subprocess(python, script_path, args);
}

PythonResult execute_sync(const std::string& script, const std::vector<std::string>& args) {
    return execute(script, args);
}

PythonResult execute_with_stdin(const std::string& script,
                                const std::vector<std::string>& args,
                                const std::string& stdin_data) {
    auto python = resolve_python_path(script);
    if (python.empty()) {
        return {false, "", "Python not found. Run setup first.", -1};
    }

    auto script_path = resolve_script_path(script);
    if (script_path.empty()) {
        return {false, "", "Script '" + script + "' not found.", -1};
    }

    return run_subprocess(python, script_path, args, &stdin_data);
}

// ============================================================================
// Async API — runs on a detached thread, returns future
// ============================================================================

std::future<PythonResult> execute_async(const std::string& script,
                                         const std::vector<std::string>& args) {
    return std::async(std::launch::async, [script, args]() -> PythonResult {
        return execute(script, args);
    });
}

std::future<PythonResult> execute_async_with_stdin(const std::string& script,
                                                    const std::vector<std::string>& args,
                                                    const std::string& stdin_data) {
    return std::async(std::launch::async, [script, args, stdin_data]() -> PythonResult {
        return execute_with_stdin(script, args, stdin_data);
    });
}

} // namespace fincept::python
