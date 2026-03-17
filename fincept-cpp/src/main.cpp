// Fincept Terminal — Entry Point
// Initializes window, ImGui, and runs the main loop.
// All subsystems are encapsulated in their own classes.

#include "app.h"
#include "core/config.h"
#include "core/logger.h"
#include "core/window.h"
#include "core/font_loader.h"
#include "core/hot_reload.h"
#include "ui/theme.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objbase.h>
#include <cstdio>
#endif

// Global callbacks required by app.h (called from App)
void toggle_fullscreen() {
    fincept::core::Window::instance().toggle_fullscreen();
}

void request_exit() {
    fincept::core::Window::instance().request_exit();
}

#ifdef _WIN32
static LONG WINAPI crash_handler(EXCEPTION_POINTERS* ep) {
    FILE* f = nullptr;
    fopen_s(&f, "fincept_crash.log", "a");
    if (f) {
        DWORD code = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0;
        void* addr = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionAddress : nullptr;
        fprintf(f, "SEH Exception: code=0x%08lX addr=%p\n", code, addr);
        fclose(f);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

int main(int /*argc*/, char** /*argv*/) {
    using namespace fincept;

#ifdef _WIN32
    SetUnhandledExceptionFilter(crash_handler);
    // Initialize COM for SAPI5 text-to-speech
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    // Redirect stdout/stderr to a log file so output survives crashes
    {
        FILE* dummy = nullptr;
        (void)freopen_s(&dummy, "fincept_stdout.log", "w", stdout);
        (void)freopen_s(&dummy, "fincept_stderr.log", "w", stderr);
        // Disable buffering so output is flushed immediately
        if (stdout) setvbuf(stdout, nullptr, _IONBF, 0);
        if (stderr) setvbuf(stderr, nullptr, _IONBF, 0);
    }
#endif

    auto& window = core::Window::instance();
    auto& hot_reload = core::HotReload::instance();

    // Create window
    std::string title = std::string(config::APP_NAME) + " v" + config::APP_VERSION;
    if (!window.create(1600, 900, title.c_str())) {
        return 1;
    }

    // ImGui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = "fincept_imgui.ini";

    // Theme and DPI
    theme::ApplyFinceptTheme();
    ImGui::GetStyle().ScaleAllSizes(window.dpi_scale());

    // Init backends
    ImGui_ImplGlfw_InitForOpenGL(window.handle(), true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // Fonts
    core::load_fonts(window.dpi_scale());

    // App
    App app;
    bool app_initialized = false;

    // Enable file logging for debugging
    core::Logger::instance().set_log_file("fincept_debug.log");
    LOG_INFO("Main", "Starting %s v%s", config::APP_NAME, config::APP_VERSION);

    // Hot reload
    hot_reload.initialize();

    // Main loop
    while (!window.should_close()) {
        window.poll_events();

        // Hot reload relaunch
        if (hot_reload.should_relaunch()) {
            app.shutdown();
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImPlot::DestroyContext();
            ImGui::DestroyContext();
            window.destroy();
            hot_reload.relaunch();
            hot_reload.shutdown();
            return 0;
        }

        // Initialize app on first frame
        if (!app_initialized) {
            app.initialize();
            app_initialized = true;
        }

        // ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        try {
            app.render();
        } catch (const std::exception& e) {
            LOG_INFO("Main", "CRASH: std::exception in render: %s", e.what());
            fprintf(stderr, "CRASH: %s\n", e.what());
        } catch (...) {
            LOG_INFO("Main", "CRASH: unknown exception in render");
            fprintf(stderr, "CRASH: unknown exception\n");
        }

        // Render
        ImGui::Render();
        int display_w, display_h;
        window.get_framebuffer_size(display_w, display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.067f, 0.067f, 0.071f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Multi-viewport
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup);
        }

        window.swap_buffers();

        // Frame rate limiter — cap at ~60 FPS to prevent 100% CPU usage
        // on low-end devices.  A financial terminal doesn't need 300+ FPS;
        // this reduces power draw, heat, and fan noise significantly.
        // Uses sleep rather than vsync to stay responsive to input events.
        {
            static auto last_frame = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            auto frame_time = std::chrono::duration_cast<std::chrono::microseconds>(now - last_frame);
            constexpr auto target_frame_us = std::chrono::microseconds(16000); // ~60 FPS
            if (frame_time < target_frame_us) {
                std::this_thread::sleep_for(target_frame_us - frame_time);
            }
            last_frame = std::chrono::steady_clock::now();
        }
    }

    // Cleanup
    hot_reload.shutdown();
    app.shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    window.destroy();

    LOG_INFO("Main", "Shutdown complete");

#ifdef _WIN32
    CoUninitialize();
#endif

    return 0;
}
