// Hot Reload implementation — file watcher + rebuild + relaunch

#include "core/hot_reload.h"
#include "core/logger.h"

#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <filesystem>
#include <cstdio>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace fincept::core {

// Internal state
struct WatchedFile {
    std::string path;
    std::filesystem::file_time_type last_write;
};

static std::vector<WatchedFile> s_watched_files;
static std::atomic<bool> s_rebuild_needed{false};
static std::atomic<bool> s_rebuild_in_progress{false};
static std::atomic<bool> s_rebuild_succeeded{false};
static std::atomic<bool> s_watcher_running{false};
static std::thread s_watcher_thread;
static std::string s_src_dir;
static std::string s_build_dir;
static std::string s_exe_path;
static std::mutex s_status_mutex;
static std::string s_status_message = "Idle";

static void scan_source_files() {
    s_watched_files.clear();
    for (auto& entry : std::filesystem::recursive_directory_iterator(s_src_dir)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext == ".cpp" || ext == ".h" || ext == ".hpp") {
            s_watched_files.push_back({entry.path().string(), entry.last_write_time()});
        }
    }
    std::string cmake_path = s_src_dir + "/../CMakeLists.txt";
    if (std::filesystem::exists(cmake_path)) {
        s_watched_files.push_back({cmake_path, std::filesystem::last_write_time(cmake_path)});
    }
    LOG_DEBUG("HotReload", "Watching %zu source files", s_watched_files.size());
}

static bool check_source_changes() {
    bool changed = false;
    for (auto& wf : s_watched_files) {
        try {
            if (!std::filesystem::exists(wf.path)) continue;
            auto current = std::filesystem::last_write_time(wf.path);
            if (current != wf.last_write) {
                LOG_INFO("HotReload", "Changed: %s", wf.path.c_str());
                wf.last_write = current;
                changed = true;
            }
        } catch (...) {}
    }
    if (changed) scan_source_files();
    return changed;
}

static void run_rebuild() {
    s_rebuild_in_progress = true;
    s_rebuild_succeeded = false;
    {
        std::lock_guard<std::mutex> lock(s_status_mutex);
        s_status_message = "Compiling...";
    }
    LOG_INFO("HotReload", "Rebuilding...");

#ifdef _WIN32
    std::string old_exe = s_exe_path + ".old";
    std::filesystem::remove(old_exe);
    std::error_code ec;
    std::filesystem::rename(s_exe_path, old_exe, ec);
    if (ec) {
        LOG_WARN("HotReload", "Could not rename running exe: %s", ec.message().c_str());
    }
#endif

    std::string cmd = "cmake --build \"" + s_build_dir + "\" --config Release 2>&1";

#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif

    bool success = false;
    if (pipe) {
        char buffer[512];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            // Build output goes to console
        }
#ifdef _WIN32
        int ret = _pclose(pipe);
#else
        int ret = pclose(pipe);
#endif
        success = (ret == 0);
    }

#ifdef _WIN32
    if (!success && !ec) {
        std::filesystem::rename(old_exe, s_exe_path, ec);
    }
#endif

    s_rebuild_in_progress = false;

    if (success) {
        s_rebuild_succeeded = true;
        std::lock_guard<std::mutex> lock(s_status_mutex);
        s_status_message = "Build OK — restarting...";
        LOG_INFO("HotReload", "Build succeeded — will relaunch");
    } else {
        std::lock_guard<std::mutex> lock(s_status_mutex);
        s_status_message = "Build FAILED — check console";
        LOG_ERROR("HotReload", "Build FAILED");
    }
}

static void watcher_loop() {
    while (s_watcher_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        if (!s_watcher_running) break;
        if (s_rebuild_in_progress) continue;

        if (check_source_changes()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (!s_watcher_running) break;
            check_source_changes();
            run_rebuild();
        }
    }
}

void HotReload::initialize() {
#ifdef _WIN32
    char path_buf[MAX_PATH];
    GetModuleFileNameA(nullptr, path_buf, MAX_PATH);
    s_exe_path = path_buf;
#elif defined(__APPLE__)
    char path_buf[4096];
    uint32_t bufsize = sizeof(path_buf);
    if (_NSGetExecutablePath(path_buf, &bufsize) == 0) s_exe_path = path_buf;
#elif defined(__linux__)
    try { s_exe_path = std::filesystem::read_symlink("/proc/self/exe").string(); }
    catch (...) {}
#endif

    std::filesystem::path exe_p(s_exe_path);
    s_build_dir = exe_p.parent_path().parent_path().string();
    s_src_dir = (exe_p.parent_path().parent_path().parent_path() / "src").string();

    if (!std::filesystem::exists(s_src_dir)) {
        LOG_WARN("HotReload", "Source dir not found: %s — disabled", s_src_dir.c_str());
        return;
    }

    LOG_INFO("HotReload", "Src: %s, Build: %s", s_src_dir.c_str(), s_build_dir.c_str());
    scan_source_files();

    s_watcher_running = true;
    s_watcher_thread = std::thread(watcher_loop);
    LOG_INFO("HotReload", "Watcher started");
}

void HotReload::shutdown() {
    s_watcher_running = false;
    if (s_watcher_thread.joinable()) s_watcher_thread.join();
}

bool HotReload::should_relaunch() const {
    return s_rebuild_succeeded.load();
}

void HotReload::relaunch() {
    LOG_INFO("HotReload", "Relaunching %s", s_exe_path.c_str());

#ifdef _WIN32
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (CreateProcessA(s_exe_path.c_str(), nullptr, nullptr, nullptr,
                       FALSE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        LOG_ERROR("HotReload", "Failed to relaunch (error %lu)", GetLastError());
    }
#else
    pid_t pid = fork();
    if (pid == 0) {
        execl(s_exe_path.c_str(), s_exe_path.c_str(), nullptr);
        _exit(1);
    }
#endif
}

std::string HotReload::status() const {
    std::lock_guard<std::mutex> lock(s_status_mutex);
    return s_status_message;
}

bool HotReload::is_rebuilding() const {
    return s_rebuild_in_progress.load();
}

} // namespace fincept::core
