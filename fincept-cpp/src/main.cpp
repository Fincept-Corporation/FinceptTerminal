// Fincept Terminal — C++ / ImGui Desktop Application
// Entry point: GLFW + OpenGL3 + Dear ImGui (Docking)
// Hot reload: watches source files → auto-recompile → auto-relaunch

// Platform headers FIRST (before GLFW native)
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include "app.h"
#include "theme/bloomberg_theme.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <filesystem>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>

// =============================================================================
// Fullscreen Toggle — uses Win32 API on Windows, GLFW on others
// =============================================================================
static GLFWwindow* g_window = nullptr;
static bool g_fullscreen = false;

#ifdef _WIN32
static LONG g_saved_style = 0;
static LONG g_saved_exstyle = 0;
static RECT g_saved_rect = {};

void toggle_fullscreen() {
    if (!g_window) return;

    HWND hwnd = glfwGetWin32Window(g_window);
    if (!hwnd) return;

    if (!g_fullscreen) {
        // Save current window style and position
        g_saved_style = GetWindowLong(hwnd, GWL_STYLE);
        g_saved_exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        GetWindowRect(hwnd, &g_saved_rect);

        // Remove title bar, borders, etc.
        SetWindowLong(hwnd, GWL_STYLE, g_saved_style & ~(WS_CAPTION | WS_THICKFRAME));
        SetWindowLong(hwnd, GWL_EXSTYLE, g_saved_exstyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

        // Get monitor info for the monitor the window is on
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);

        // Resize to cover the full monitor
        SetWindowPos(hwnd, HWND_TOP,
            mi.rcMonitor.left, mi.rcMonitor.top,
            mi.rcMonitor.right - mi.rcMonitor.left,
            mi.rcMonitor.bottom - mi.rcMonitor.top,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

        g_fullscreen = true;
        std::printf("[Fullscreen] Entered fullscreen\n");
    } else {
        // Restore window style
        SetWindowLong(hwnd, GWL_STYLE, g_saved_style);
        SetWindowLong(hwnd, GWL_EXSTYLE, g_saved_exstyle);

        // Restore position and size
        SetWindowPos(hwnd, nullptr,
            g_saved_rect.left, g_saved_rect.top,
            g_saved_rect.right - g_saved_rect.left,
            g_saved_rect.bottom - g_saved_rect.top,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

        g_fullscreen = false;
        std::printf("[Fullscreen] Exited fullscreen\n");
    }
}
#else
static int g_windowed_x = 0, g_windowed_y = 0, g_windowed_w = 1600, g_windowed_h = 900;

void toggle_fullscreen() {
    if (!g_window) return;
    if (!g_fullscreen) {
        glfwGetWindowPos(g_window, &g_windowed_x, &g_windowed_y);
        glfwGetWindowSize(g_window, &g_windowed_w, &g_windowed_h);
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(g_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        g_fullscreen = true;
    } else {
        glfwSetWindowMonitor(g_window, nullptr, g_windowed_x, g_windowed_y, g_windowed_w, g_windowed_h, 0);
        glfwMaximizeWindow(g_window);
        g_fullscreen = false;
    }
}
#endif

// =============================================================================
// GLFW Error Callback
// =============================================================================
static void glfw_error_callback(int error, const char* description) {
    std::fprintf(stderr, "[GLFW Error %d] %s\n", error, description);
}

// =============================================================================
// Font Loading
// =============================================================================
static void load_fonts(float dpi_scale) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    float font_size = 16.0f * dpi_scale;

    // Font candidates in priority order per platform
    const char* font_candidates[] = {
        "fonts/JetBrainsMono-Regular.ttf",               // Bundled (best for terminal)
        "../fonts/JetBrainsMono-Regular.ttf",             // Build directory offset
#ifdef _WIN32
        "C:/Windows/Fonts/segoeui.ttf",                   // Segoe UI — Windows default
        "C:/Windows/Fonts/consola.ttf",                   // Consolas — monospace fallback
        "C:/Windows/Fonts/arial.ttf",                     // Arial — universally readable
#elif defined(__APPLE__)
        "/System/Library/Fonts/SFNSMono.ttf",            // SF Mono
        "/System/Library/Fonts/Menlo.ttc",                // Menlo
        "/System/Library/Fonts/Helvetica.ttc",            // Helvetica
        "/Library/Fonts/Arial.ttf",                       // Arial
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",  // DejaVu Mono
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",       // Arch Linux
#endif
    };

    ImFontConfig config;
    config.OversampleH = 3;
    config.OversampleV = 3;
    config.PixelSnapH = true;
    config.RasterizerDensity = dpi_scale;

    bool loaded = false;
    for (const char* path : font_candidates) {
        if (std::filesystem::exists(path)) {
            io.FontDefault = io.Fonts->AddFontFromFileTTF(path, font_size, &config);
            if (io.FontDefault) {
                std::printf("[Font] Loaded: %s at %.0fpx\n", path, font_size);
                loaded = true;
                break;
            }
        }
    }

    if (!loaded) {
        ImFontConfig fallback_config;
        fallback_config.SizePixels = font_size;
        io.FontDefault = io.Fonts->AddFontDefault(&fallback_config);
        std::printf("[Font] Using ImGui default font at %.0fpx\n", font_size);
    }

    io.Fonts->Build();
}

// =============================================================================
// Hot Reload: Watch source files → recompile → relaunch
// =============================================================================
namespace hot_reload {

struct WatchedFile {
    std::string path;
    std::filesystem::file_time_type last_write;
};

static std::vector<WatchedFile> watched_files_;
static std::atomic<bool> rebuild_needed_{false};
static std::atomic<bool> rebuild_in_progress_{false};
static std::atomic<bool> rebuild_succeeded_{false};
static std::atomic<bool> watcher_running_{false};
static std::thread watcher_thread_;
static std::string src_dir_;
static std::string build_dir_;
static std::string exe_path_;
static std::mutex status_mutex_;
static std::string status_message_ = "Idle";

static void scan_source_files() {
    watched_files_.clear();
    for (auto& entry : std::filesystem::recursive_directory_iterator(src_dir_)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext == ".cpp" || ext == ".h" || ext == ".hpp") {
            watched_files_.push_back({
                entry.path().string(),
                entry.last_write_time()
            });
        }
    }
    // Also watch CMakeLists.txt
    std::string cmake_path = src_dir_ + "/../CMakeLists.txt";
    if (std::filesystem::exists(cmake_path)) {
        watched_files_.push_back({
            cmake_path,
            std::filesystem::last_write_time(cmake_path)
        });
    }
    std::printf("[HotReload] Watching %zu source files\n", watched_files_.size());
}

static bool check_source_changes() {
    bool changed = false;
    for (auto& wf : watched_files_) {
        try {
            if (!std::filesystem::exists(wf.path)) continue;
            auto current = std::filesystem::last_write_time(wf.path);
            if (current != wf.last_write) {
                std::printf("[HotReload] Changed: %s\n", wf.path.c_str());
                wf.last_write = current;
                changed = true;
            }
        } catch (...) {}
    }
    // Also check for new files
    if (changed) {
        scan_source_files();
    }
    return changed;
}

static void run_rebuild() {
    rebuild_in_progress_ = true;
    rebuild_succeeded_ = false;
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        status_message_ = "Compiling...";
    }
    std::printf("[HotReload] === Rebuilding... ===\n");

#ifdef _WIN32
    // Windows: running exe is locked by the linker. Rename it out of the way first.
    // Windows allows renaming a running executable, but not overwriting it.
    std::string old_exe = exe_path_ + ".old";
    // Remove any previous .old file
    std::filesystem::remove(old_exe);
    // Rename the running exe so the linker can create a fresh one
    std::error_code ec;
    std::filesystem::rename(exe_path_, old_exe, ec);
    if (ec) {
        std::printf("[HotReload] Warning: could not rename running exe: %s\n", ec.message().c_str());
    } else {
        std::printf("[HotReload] Renamed running exe to .old\n");
    }
#endif

    // Run cmake --build
    std::string cmd = "cmake --build \"" + build_dir_ + "\" --config Release 2>&1";

#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif

    bool success = false;
    if (pipe) {
        char buffer[512];
        std::string output;
        while (fgets(buffer, sizeof(buffer), pipe)) {
            output += buffer;
            std::printf("[Build] %s", buffer);
        }
#ifdef _WIN32
        int ret = _pclose(pipe);
#else
        int ret = pclose(pipe);
#endif
        success = (ret == 0);
    }

#ifdef _WIN32
    // If build failed, rename back so the exe still exists for next attempt
    if (!success && !ec) {
        std::filesystem::rename(old_exe, exe_path_, ec);
        if (!ec) std::printf("[HotReload] Restored original exe\n");
    }
#endif

    rebuild_in_progress_ = false;

    if (success) {
        rebuild_succeeded_ = true;
        {
            std::lock_guard<std::mutex> lock(status_mutex_);
            status_message_ = "Build OK — restarting...";
        }
        std::printf("[HotReload] === Build succeeded — will relaunch ===\n");
    } else {
        {
            std::lock_guard<std::mutex> lock(status_mutex_);
            status_message_ = "Build FAILED — check console";
        }
        std::printf("[HotReload] === Build FAILED ===\n");
    }
}

static void watcher_loop() {
    while (watcher_running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        if (!watcher_running_) break;
        if (rebuild_in_progress_) continue;

        if (check_source_changes()) {
            // Debounce: wait 500ms for more changes
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (!watcher_running_) break;
            // Check again to catch rapid saves
            check_source_changes();

            run_rebuild();
        }
    }
}

void initialize() {
#ifdef _WIN32
    char path_buf[MAX_PATH];
    GetModuleFileNameA(nullptr, path_buf, MAX_PATH);
    exe_path_ = path_buf;
#elif defined(__APPLE__)
    char path_buf[4096];
    uint32_t bufsize = sizeof(path_buf);
    if (_NSGetExecutablePath(path_buf, &bufsize) == 0) {
        exe_path_ = path_buf;
    }
#elif defined(__linux__)
    try {
        exe_path_ = std::filesystem::read_symlink("/proc/self/exe").string();
    } catch (...) {}
#endif

    // Determine source and build directories relative to exe
    std::filesystem::path exe_p(exe_path_);
    // exe is at build/Release/FinceptTerminal.exe
    // build dir is build/
    // source dir is src/
    build_dir_ = exe_p.parent_path().parent_path().string();
    src_dir_ = (exe_p.parent_path().parent_path().parent_path() / "src").string();

    if (!std::filesystem::exists(src_dir_)) {
        std::printf("[HotReload] Source dir not found: %s — hot reload disabled\n", src_dir_.c_str());
        return;
    }

    std::printf("[HotReload] Source dir: %s\n", src_dir_.c_str());
    std::printf("[HotReload] Build dir:  %s\n", build_dir_.c_str());

    scan_source_files();

    watcher_running_ = true;
    watcher_thread_ = std::thread(watcher_loop);

    std::printf("[HotReload] Watcher started — edit any .cpp/.h file to trigger rebuild\n");
}

bool should_relaunch() {
    return rebuild_succeeded_.load();
}

void relaunch() {
    std::printf("[HotReload] Relaunching %s...\n", exe_path_.c_str());

#ifdef _WIN32
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    if (CreateProcessA(
        exe_path_.c_str(),
        nullptr,
        nullptr, nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        &si, &pi
    )) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        std::printf("[HotReload] New process started (PID %lu)\n", pi.dwProcessId);
    } else {
        std::printf("[HotReload] Failed to relaunch (error %lu)\n", GetLastError());
    }
#else
    // Unix: fork + exec
    pid_t pid = fork();
    if (pid == 0) {
        execl(exe_path_.c_str(), exe_path_.c_str(), nullptr);
        _exit(1); // exec failed
    } else if (pid > 0) {
        std::printf("[HotReload] New process started (PID %d)\n", pid);
    } else {
        std::printf("[HotReload] Fork failed\n");
    }
#endif
}

void shutdown() {
    watcher_running_ = false;
    if (watcher_thread_.joinable()) {
        watcher_thread_.join();
    }
}

std::string get_status() {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return status_message_;
}

bool is_rebuilding() {
    return rebuild_in_progress_.load();
}

} // namespace hot_reload

// =============================================================================
// Main
// =============================================================================
int main(int /*argc*/, char** /*argv*/) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    // OpenGL 3.3 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Window hints
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    // Create window
    GLFWwindow* window = glfwCreateWindow(1600, 900, "Fincept Terminal v3.3.1", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }
    g_window = window;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // VSync

    // ImGui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // Enable docking and viewports
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Multi-viewport (optional)

    // Keyboard nav
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // INI file for layout persistence
    io.IniFilename = "fincept_imgui.ini";

    // Apply Bloomberg theme
    fincept::theme::ApplyBloombergTheme();

    // DPI awareness
    float xscale = 1.0f, yscale = 1.0f;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    float dpi_scale = (xscale + yscale) / 2.0f;

    // Scale style for DPI
    ImGui::GetStyle().ScaleAllSizes(dpi_scale);

    // Init backends FIRST (before loading fonts — OpenGL needs to upload font atlas)
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    const char* glsl_version = "#version 330 core";
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load fonts AFTER backend init
    load_fonts(dpi_scale);

    // Initialize application
    fincept::App app;

    // Initialize in a thread to avoid blocking the window
    bool app_initialized = false;
    std::printf("[Main] Starting Fincept Terminal...\n");

    // Initialize hot reload
    hot_reload::initialize();

    // ========================================================================
    // Main Loop
    // ========================================================================
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Hot reload: if rebuild succeeded, relaunch
        if (hot_reload::should_relaunch()) {
            // Clean shutdown before relaunch
            app.shutdown();
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            glfwDestroyWindow(window);
            glfwTerminate();
            hot_reload::relaunch();
            hot_reload::shutdown();
            return 0;
        }

        // Initialize app on first frame (after OpenGL context is ready)
        if (!app_initialized) {
            app.initialize();
            app_initialized = true;
        }

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Render application
        app.render();

        // Render ImGui
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);

        // Clear with Bloomberg dark background
        glClearColor(0.039f, 0.039f, 0.039f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Multi-viewport support (if enabled)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_context);
        }

        glfwSwapBuffers(window);
    }

    // ========================================================================
    // Cleanup
    // ========================================================================
    hot_reload::shutdown();
    app.shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    std::printf("[Main] Fincept Terminal shutdown complete\n");
    return 0;
}
