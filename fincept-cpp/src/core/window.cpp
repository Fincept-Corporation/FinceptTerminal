// Window management implementation

#include "core/window.h"
#include "core/logger.h"
#include <cstdio>

namespace fincept::core {

static void glfw_error_callback(int error, const char* description) {
    LOG_ERROR("GLFW", "Error %d: %s", error, description);
}

bool Window::create(int width, int height, const char* title) {
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
        LOG_FATAL("Window", "Failed to initialize GLFW");
        return false;
    }

    // OpenGL 3.3 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window_) {
        LOG_FATAL("Window", "Failed to create GLFW window");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1); // VSync

    // DPI scaling
    float xscale = 1.0f, yscale = 1.0f;
    glfwGetWindowContentScale(window_, &xscale, &yscale);
    dpi_scale_ = (xscale + yscale) / 2.0f;

    LOG_INFO("Window", "Created %dx%d, DPI scale: %.2f", width, height, dpi_scale_);
    return true;
}

void Window::destroy() {
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

void Window::toggle_fullscreen() {
    if (!window_) return;

#ifdef _WIN32
    HWND hwnd = glfwGetWin32Window(window_);
    if (!hwnd) return;

    if (!fullscreen_) {
        saved_style_ = GetWindowLong(hwnd, GWL_STYLE);
        saved_exstyle_ = GetWindowLong(hwnd, GWL_EXSTYLE);
        GetWindowRect(hwnd, &saved_rect_);

        SetWindowLong(hwnd, GWL_STYLE,
            saved_style_ & ~(WS_CAPTION | WS_THICKFRAME));
        SetWindowLong(hwnd, GWL_EXSTYLE,
            saved_exstyle_ & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
                               WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

        MONITORINFO mi = {sizeof(mi)};
        GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);

        SetWindowPos(hwnd, HWND_TOP,
            mi.rcMonitor.left, mi.rcMonitor.top,
            mi.rcMonitor.right - mi.rcMonitor.left,
            mi.rcMonitor.bottom - mi.rcMonitor.top,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

        fullscreen_ = true;
    } else {
        SetWindowLong(hwnd, GWL_STYLE, saved_style_);
        SetWindowLong(hwnd, GWL_EXSTYLE, saved_exstyle_);

        SetWindowPos(hwnd, nullptr,
            saved_rect_.left, saved_rect_.top,
            saved_rect_.right - saved_rect_.left,
            saved_rect_.bottom - saved_rect_.top,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

        fullscreen_ = false;
    }
#else
    if (!fullscreen_) {
        glfwGetWindowPos(window_, &windowed_x_, &windowed_y_);
        glfwGetWindowSize(window_, &windowed_w_, &windowed_h_);
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(window_, monitor, 0, 0,
                             mode->width, mode->height, mode->refreshRate);
        fullscreen_ = true;
    } else {
        glfwSetWindowMonitor(window_, nullptr,
                             windowed_x_, windowed_y_,
                             windowed_w_, windowed_h_, 0);
        glfwMaximizeWindow(window_);
        fullscreen_ = false;
    }
#endif
    LOG_DEBUG("Window", "Fullscreen: %s", fullscreen_ ? "on" : "off");
}

void Window::request_exit() {
    if (window_) glfwSetWindowShouldClose(window_, GLFW_TRUE);
}

bool Window::should_close() const {
    return window_ && glfwWindowShouldClose(window_);
}

void Window::poll_events() {
    glfwPollEvents();
}

void Window::swap_buffers() {
    if (window_) glfwSwapBuffers(window_);
}

void Window::get_framebuffer_size(int& w, int& h) const {
    if (window_) glfwGetFramebufferSize(window_, &w, &h);
    else { w = 0; h = 0; }
}

} // namespace fincept::core
