// Python setup manager — downloads Python, creates venvs, installs packages
// C++ port of setup/ module from the Tauri app

#include "setup_manager.h"
#include "python_runner.h"
#include "core/raii.h"
#include <curl/curl.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>

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
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace fincept::python {

static constexpr const char* PYTHON_VERSION = "3.12.7";

// ============================================================================
// Singleton
// ============================================================================

SetupManager& SetupManager::instance() {
    static SetupManager s;
    return s;
}

// ============================================================================
// Progress
// ============================================================================

void SetupManager::emit(const std::string& step, int progress, const std::string& msg, bool error) {
    std::lock_guard<std::mutex> lock(progress_mutex_);
    current_progress_ = {step, progress, msg, error};
    std::cerr << "[SETUP] [" << step << "] " << progress << "% - " << msg << "\n";
}

SetupProgress SetupManager::get_progress() const {
    std::lock_guard<std::mutex> lock(progress_mutex_);
    return current_progress_;
}

// ============================================================================
// Status checks
// ============================================================================

SetupStatus SetupManager::check_status() {
    SetupStatus status;
    status.python_installed = check_python_installed();
    if (status.python_installed) {
        status.python_version = get_python_version();
    }
    status.packages_installed = check_packages_installed();
    status.needs_setup = !status.python_installed || !status.packages_installed;
    return status;
}

bool SetupManager::check_python_installed() {
    auto install_dir = get_install_dir();
#ifdef _WIN32
    auto python_exe = install_dir / "python" / "python.exe";
#else
    auto python_exe = install_dir / "python" / "bin" / "python3";
#endif
    return fs::exists(python_exe);
}

bool SetupManager::check_packages_installed() {
    auto install_dir = get_install_dir();

#ifdef _WIN32
    auto venv1 = install_dir / "venv-numpy1" / "Scripts" / "python.exe";
    auto venv2 = install_dir / "venv-numpy2" / "Scripts" / "python.exe";
#else
    auto venv1 = install_dir / "venv-numpy1" / "bin" / "python3";
    auto venv2 = install_dir / "venv-numpy2" / "bin" / "python3";
#endif

    return fs::exists(venv1) && fs::exists(venv2);
}

std::string SetupManager::get_python_version() {
    auto install_dir = get_install_dir();
#ifdef _WIN32
    auto python_exe = install_dir / "python" / "python.exe";
#else
    auto python_exe = install_dir / "python" / "bin" / "python3";
#endif

    if (!fs::exists(python_exe)) return "";

    std::string cmd = "\"" + python_exe.string() + "\" --version";
    std::string output;
    if (run_command(cmd, &output)) {
        // Trim
        while (!output.empty() && (output.back() == '\n' || output.back() == '\r'))
            output.pop_back();
        return output;
    }
    return "";
}

// ============================================================================
// File download via libcurl
// ============================================================================

struct DownloadData {
    FILE* fp = nullptr;
};

static size_t download_write_cb(void* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* data = static_cast<DownloadData*>(userdata);
    return fwrite(ptr, size, nmemb, data->fp);
}

bool SetupManager::download_file(const std::string& url, const fs::path& dest) {
    core::CurlHandle curl;
    if (!curl) return false;

    DownloadData data;
#ifdef _WIN32
    core::FileHandle file(dest.wstring().c_str(), L"wb");
#else
    core::FileHandle file(dest.string().c_str(), "wb");
#endif
    if (!file) return false;
    data.fp = file.get();

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, download_write_cb);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "FinceptTerminal/3.3.1");
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 600L); // 10 min for large downloads
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl.get());
    // file and curl cleaned up automatically by destructors

    if (res != CURLE_OK) {
        std::cerr << "[SETUP] Download failed: " << curl_easy_strerror(res) << "\n";
        fs::remove(dest);
        return false;
    }

    return true;
}

// ============================================================================
// Command execution
// ============================================================================

bool SetupManager::run_command(const std::string& cmd, std::string* output) {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead = nullptr, hWrite = nullptr;
    CreatePipe(&hRead, &hWrite, &sa, 0);
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi{};
    std::string cmd_line = "cmd /C " + cmd;

    BOOL ok = CreateProcessA(
        nullptr, cmd_line.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi
    );
    CloseHandle(hWrite);

    if (!ok) {
        CloseHandle(hRead);
        return false;
    }

    std::string out;
    char buffer[4096];
    DWORD bytesRead;
    while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        out += buffer;
    }
    CloseHandle(hRead);

    WaitForSingleObject(pi.hProcess, 300000); // 5 min timeout

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (output) *output = out;
    return exit_code == 0;
#else
    std::string full_cmd = cmd + " 2>&1";
    FILE* pipe = popen(full_cmd.c_str(), "r");
    if (!pipe) return false;

    std::string out;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) out += buffer;

    int status = pclose(pipe);
    if (output) *output = out;
    return WEXITSTATUS(status) == 0;
#endif
}

// ============================================================================
// Install Python
// ============================================================================

bool SetupManager::install_python() {
    emit("python", 0, "Installing Python...");

    auto install_dir = get_install_dir();
    auto python_dir = install_dir / "python";
    fs::create_directories(python_dir);

    emit("python", 10, "Downloading Python " + std::string(PYTHON_VERSION) + "...");

#ifdef _WIN32
    // Windows: download embedded Python from python.org
    std::string url = "https://www.python.org/ftp/python/" + std::string(PYTHON_VERSION)
                    + "/python-" + std::string(PYTHON_VERSION) + "-embed-amd64.zip";
    auto zip_path = python_dir / "python.zip";

    if (!download_file(url, zip_path)) {
        emit("python", 0, "Failed to download Python", true);
        return false;
    }

    emit("python", 50, "Extracting Python...");

    // Extract using PowerShell
    std::string extract_cmd = "powershell -Command \"Expand-Archive -Path '"
        + zip_path.string() + "' -DestinationPath '" + python_dir.string() + "' -Force\"";
    if (!run_command(extract_cmd)) {
        emit("python", 0, "Failed to extract Python", true);
        return false;
    }
    fs::remove(zip_path);

    // Enable pip by modifying python312._pth
    auto pth_file = python_dir / ("python" + std::string(PYTHON_VERSION).substr(0, 1)
                    + std::string(PYTHON_VERSION).substr(2, 2) + "._pth");
    // Try common names
    for (const auto& name : {"python312._pth", "python311._pth", "python313._pth"}) {
        auto p = python_dir / name;
        if (fs::exists(p)) {
            pth_file = p;
            break;
        }
    }

    if (fs::exists(pth_file)) {
        std::ifstream in(pth_file);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();

        // Enable import site
        size_t pos = content.find("#import site");
        if (pos != std::string::npos) {
            content.replace(pos, 12, "import site");
        }

        // Add site-packages if not present
        if (content.find("Lib/site-packages") == std::string::npos &&
            content.find("Lib\\site-packages") == std::string::npos) {
            content += "\nLib/site-packages\n";
        }

        std::ofstream out(pth_file);
        out << content;
    }

    // Create Lib/site-packages
    fs::create_directories(python_dir / "Lib" / "site-packages");

#elif defined(__APPLE__)
    // macOS: use python-build-standalone from Astral
    std::string arch = "aarch64";
    #if defined(__x86_64__)
    arch = "x86_64";
    #endif

    std::string platform = arch + "-apple-darwin-install_only";
    std::string url = "https://github.com/astral-sh/python-build-standalone/releases/download/20240726/cpython-3.12.4+20240726-"
                    + platform + ".tar.gz";
    auto tar_path = python_dir / "python.tar.gz";

    if (!download_file(url, tar_path)) {
        emit("python", 0, "Failed to download Python", true);
        return false;
    }

    emit("python", 50, "Extracting Python...");

    std::string extract_cmd = "tar -xzf \"" + tar_path.string() + "\" -C \""
                            + python_dir.string() + "\" --strip-components=1";
    if (!run_command(extract_cmd)) {
        emit("python", 0, "Failed to extract Python", true);
        return false;
    }
    fs::remove(tar_path);

#else
    // Linux: use python-build-standalone from Astral
    std::string arch = "x86_64";
    #if defined(__aarch64__)
    arch = "aarch64";
    #endif

    std::string platform = arch + "-unknown-linux-gnu-install_only";
    std::string url = "https://github.com/astral-sh/python-build-standalone/releases/download/20240726/cpython-3.12.4+20240726-"
                    + platform + ".tar.gz";
    auto tar_path = python_dir / "python.tar.gz";

    if (!download_file(url, tar_path)) {
        emit("python", 0, "Failed to download Python", true);
        return false;
    }

    emit("python", 50, "Extracting Python...");

    std::string extract_cmd = "tar -xzf \"" + tar_path.string() + "\" -C \""
                            + python_dir.string() + "\" --strip-components=1";
    if (!run_command(extract_cmd)) {
        emit("python", 0, "Failed to extract Python", true);
        return false;
    }
    fs::remove(tar_path);
#endif

    // Install pip
    emit("python", 70, "Installing pip...");

    auto get_pip_path = python_dir / "get-pip.py";
    if (!download_file("https://bootstrap.pypa.io/get-pip.py", get_pip_path)) {
        emit("python", 0, "Failed to download get-pip.py", true);
        return false;
    }

#ifdef _WIN32
    auto python_exe = python_dir / "python.exe";
#else
    auto python_exe = python_dir / "bin" / "python3";
#endif

    std::string pip_cmd = "\"" + python_exe.string() + "\" \"" + get_pip_path.string()
                        + "\" --no-warn-script-location";
    std::string pip_output;
    if (!run_command(pip_cmd, &pip_output)) {
        emit("python", 0, "Failed to install pip: " + pip_output, true);
        return false;
    }
    fs::remove(get_pip_path);

    // Verify pip
    emit("python", 90, "Verifying pip...");
    std::string verify_cmd = "\"" + python_exe.string() + "\" -m pip --version";
    if (!run_command(verify_cmd)) {
        emit("python", 0, "pip verification failed", true);
        return false;
    }

    emit("python", 100, "Python installed");
    return true;
}

// ============================================================================
// Install UV
// ============================================================================

bool SetupManager::install_uv() {
    emit("uv", 0, "Installing UV package manager...");

    auto install_dir = get_install_dir();
#ifdef _WIN32
    auto python_exe = install_dir / "python" / "python.exe";
#else
    auto python_exe = install_dir / "python" / "bin" / "python3";
#endif

    std::string cmd = "\"" + python_exe.string() + "\" -m pip install --no-warn-script-location uv";
    std::string output;
    if (!run_command(cmd, &output)) {
        emit("uv", 0, "Failed to install UV: " + output, true);
        return false;
    }

    emit("uv", 100, "UV installed");
    return true;
}

// ============================================================================
// Create venv
// ============================================================================

bool SetupManager::create_venv(const std::string& venv_name) {
    emit("venv", 0, "Creating " + venv_name + "...");

    auto install_dir = get_install_dir();
#ifdef _WIN32
    auto python_exe = install_dir / "python" / "python.exe";
    auto uv_exe = install_dir / "python" / "Scripts" / "uv.exe";
#else
    auto python_exe = install_dir / "python" / "bin" / "python3";
    auto uv_exe = install_dir / "python" / "bin" / "uv";
#endif

    auto venv_path = install_dir / venv_name;

    std::string cmd;
    if (fs::exists(uv_exe)) {
        cmd = "\"" + uv_exe.string() + "\" venv \"" + venv_path.string()
            + "\" --python \"" + python_exe.string() + "\"";
    } else {
        // Fallback: python -m uv
        cmd = "\"" + python_exe.string() + "\" -m uv venv \"" + venv_path.string()
            + "\" --python \"" + python_exe.string() + "\"";
    }

    std::string output;
    if (!run_command(cmd, &output)) {
        emit("venv", 0, "Failed to create " + venv_name + ": " + output, true);
        return false;
    }

    emit("venv", 100, venv_name + " created");
    return true;
}

// ============================================================================
// Install packages
// ============================================================================

bool SetupManager::install_packages(const std::string& venv_name,
                                     const std::string& requirements_file,
                                     const std::string& label) {
    emit("packages", 0, "Installing " + label + " packages...");

    auto install_dir = get_install_dir();
    auto exe_dir = get_exe_dir();

#ifdef _WIN32
    auto python_exe = install_dir / "python" / "python.exe";
    auto uv_exe = install_dir / "python" / "Scripts" / "uv.exe";
    auto venv_python = install_dir / venv_name / "Scripts" / "python.exe";
#else
    auto python_exe = install_dir / "python" / "bin" / "python3";
    auto uv_exe = install_dir / "python" / "bin" / "uv";
    auto venv_python = install_dir / venv_name / "bin" / "python3";
#endif

    // Find requirements file: check exe_dir/resources/ then exe_dir/
    fs::path req_path;
    std::vector<fs::path> req_candidates = {
        exe_dir / "resources" / requirements_file,
        exe_dir / requirements_file,
        fs::current_path() / "resources" / requirements_file,
        fs::current_path() / requirements_file,
    };
    for (const auto& p : req_candidates) {
        if (fs::exists(p)) { req_path = p; break; }
    }

    if (req_path.empty()) {
        emit("packages", 0, "Requirements file not found: " + requirements_file, true);
        return false;
    }

    emit("packages", 20, "Installing " + label + " packages with UV...");

    std::string cmd;
    if (fs::exists(uv_exe)) {
        cmd = "\"" + uv_exe.string() + "\"";
    } else {
        cmd = "\"" + python_exe.string() + "\" -m uv";
    }

    cmd += " pip install --python \"" + venv_python.string()
         + "\" -r \"" + req_path.string() + "\"";

    // Set env vars to prevent C extension build failures
#ifdef _WIN32
    // On Windows, set env vars before the command
    cmd = "set PEEWEE_NO_SQLITE_EXTENSIONS=1 && set PEEWEE_NO_C_EXTENSION=1 && " + cmd;
#else
    cmd = "PEEWEE_NO_SQLITE_EXTENSIONS=1 PEEWEE_NO_C_EXTENSION=1 " + cmd;
#endif

    std::string output;
    if (!run_command(cmd, &output)) {
        emit("packages", 0, label + " package install failed", true);
        std::cerr << "[SETUP] Package install output: " << output << "\n";
        return false;
    }

    emit("packages", 100, label + " packages installed");
    return true;
}

// ============================================================================
// Full setup flow
// ============================================================================

bool SetupManager::run_setup() {
    if (running_.load()) return false;
    running_ = true;

    emit("init", 0, "Starting setup...");

    auto install_dir = get_install_dir();
    fs::create_directories(install_dir);
    std::cerr << "[SETUP] Install directory: " << install_dir << "\n";

    // Step 1: Python
    if (!check_python_installed()) {
        if (!install_python()) {
            running_ = false;
            return false;
        }
    } else {
        auto ver = get_python_version();
        emit("python", 100, "Python already installed: " + ver);
    }

    // Step 2: UV
    if (!install_uv()) {
        running_ = false;
        return false;
    }

    // Step 3: Create venvs and install packages
    if (!check_packages_installed()) {
        // venv-numpy1
        emit("venv", 0, "Creating NumPy 1.x environment...");
        if (!create_venv("venv-numpy1")) { running_ = false; return false; }
        if (!install_packages("venv-numpy1", "requirements-numpy1.txt", "NumPy 1.x")) {
            running_ = false; return false;
        }

        // venv-numpy2
        emit("venv", 50, "Creating NumPy 2.x environment...");
        if (!create_venv("venv-numpy2")) { running_ = false; return false; }
        if (!install_packages("venv-numpy2", "requirements-numpy2.txt", "NumPy 2.x")) {
            running_ = false; return false;
        }
    } else {
        emit("packages", 100, "Packages already installed");
    }

    emit("complete", 100, "Setup complete!");
    complete_ = true;
    running_ = false;
    return true;
}

} // namespace fincept::python
