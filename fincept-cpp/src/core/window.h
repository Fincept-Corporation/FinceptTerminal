#pragma once
// Window management — GLFW window, fullscreen toggle, DPI scaling
// Encapsulates all platform-specific window code

#include <GLFW/glfw3.h>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

namespace fincept::core {

class Window {
public:
    static Window& instance() {
        static Window w;
        return w;
    }

    // Create the GLFW window. Returns false on failure.
    bool create(int width, int height, const char* title);

    // Destroy the window and terminate GLFW
    void destroy();

    // Toggle fullscreen mode
    void toggle_fullscreen();

    // Signal the window to close
    void request_exit();

    // Check if window should close
    bool should_close() const;

    // Poll events
    void poll_events();

    // Swap buffers
    void swap_buffers();

    // Get framebuffer size
    void get_framebuffer_size(int& w, int& h) const;

    // DPI scale factor
    float dpi_scale() const { return dpi_scale_; }

    // Raw GLFW handle (for ImGui backend init)
    GLFWwindow* handle() const { return window_; }

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

private:
    Window() = default;
    ~Window() = default;

    GLFWwindow* window_ = nullptr;
    bool fullscreen_ = false;
    float dpi_scale_ = 1.0f;

#ifdef _WIN32
    LONG saved_style_ = 0;
    LONG saved_exstyle_ = 0;
    RECT saved_rect_ = {};
#else
    int windowed_x_ = 0, windowed_y_ = 0;
    int windowed_w_ = 1600, windowed_h_ = 900;
#endif
};

} // namespace fincept::core
