// MpvPlayer — libmpv render API integration for inline video playback
// Renders video frames into an OpenGL FBO, returns texture ID for ImGui::Image()

#include "mpv_player.h"
#include "core/logger.h"

#ifdef FINCEPT_HAS_MPV

#include <mpv/client.h>
#include <mpv/render_gl.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <GL/gl.h>
#include <GLFW/glfw3.h>

// GL extension function pointers (not in gl.h on Windows)
#ifdef _WIN32
typedef void (APIENTRY* PFNGLGENFRAMEBUFFERSPROC)(GLsizei, GLuint*);
typedef void (APIENTRY* PFNGLBINDFRAMEBUFFERPROC)(GLenum, GLuint);
typedef void (APIENTRY* PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef void (APIENTRY* PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei, const GLuint*);
typedef GLenum (APIENTRY* PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum);

static PFNGLGENFRAMEBUFFERSPROC    glGenFramebuffers_fn = nullptr;
static PFNGLBINDFRAMEBUFFERPROC    glBindFramebuffer_fn = nullptr;
static PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D_fn = nullptr;
static PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers_fn = nullptr;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus_fn = nullptr;

#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5

static void load_gl_fbo_funcs() {
    if (glGenFramebuffers_fn) return;
    glGenFramebuffers_fn = (PFNGLGENFRAMEBUFFERSPROC)wglGetProcAddress("glGenFramebuffers");
    glBindFramebuffer_fn = (PFNGLBINDFRAMEBUFFERPROC)wglGetProcAddress("glBindFramebuffer");
    glFramebufferTexture2D_fn = (PFNGLFRAMEBUFFERTEXTURE2DPROC)wglGetProcAddress("glFramebufferTexture2D");
    glDeleteFramebuffers_fn = (PFNGLDELETEFRAMEBUFFERSPROC)wglGetProcAddress("glDeleteFramebuffers");
    glCheckFramebufferStatus_fn = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)wglGetProcAddress("glCheckFramebufferStatus");
}

#define glGenFramebuffers glGenFramebuffers_fn
#define glBindFramebuffer glBindFramebuffer_fn
#define glFramebufferTexture2D glFramebufferTexture2D_fn
#define glDeleteFramebuffers glDeleteFramebuffers_fn
#define glCheckFramebufferStatus glCheckFramebufferStatus_fn

#elif defined(__APPLE__)
// macOS — GL3 header provides FBO functions
#include <OpenGL/gl3.h>
#else
// Linux — GL functions available via glext
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif

namespace fincept::media {

// GL proc address callback for mpv
static void* get_proc_address(void* /*ctx*/, const char* name) {
    return (void*)glfwGetProcAddress(name);
}

// ============================================================================
// Construction / destruction
// ============================================================================
MpvPlayer::MpvPlayer() = default;

MpvPlayer::~MpvPlayer() {
    cleanup();
}

void MpvPlayer::cleanup() {
    if (mpv_gl_) {
        mpv_render_context_free(mpv_gl_);
        mpv_gl_ = nullptr;
    }
    if (mpv_) {
        mpv_terminate_destroy(mpv_);
        mpv_ = nullptr;
    }
    if (fbo_) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
    if (texture_) {
        glDeleteTextures(1, &texture_);
        texture_ = 0;
    }
    state_ = PlayerState::Idle;
}

// ============================================================================
// Initialize mpv + render context
// ============================================================================
bool MpvPlayer::init() {
    if (mpv_) return true;  // Already initialized

#ifdef _WIN32
    load_gl_fbo_funcs();
    if (!glGenFramebuffers) {
        LOG_ERROR("MpvPlayer", "Failed to load GL FBO functions");
        state_ = PlayerState::Error;
        error_ = "OpenGL FBO functions not available";
        return false;
    }
#endif

    mpv_ = mpv_create();
    if (!mpv_) {
        LOG_ERROR("MpvPlayer", "mpv_create() failed");
        state_ = PlayerState::Error;
        error_ = "Failed to create mpv instance";
        return false;
    }

    // Configuration
    mpv_set_option_string(mpv_, "vo", "libmpv");
    mpv_set_option_string(mpv_, "hwdec", "auto");
    mpv_set_option_string(mpv_, "ytdl", "yes");          // Enable yt-dlp
    mpv_set_option_string(mpv_, "ytdl-format", "best[height<=720]"); // Cap at 720p
    mpv_set_option_string(mpv_, "keep-open", "yes");
    mpv_set_option_string(mpv_, "idle", "yes");
    mpv_set_option_string(mpv_, "terminal", "no");
    mpv_set_option_string(mpv_, "msg-level", "all=warn");

    if (mpv_initialize(mpv_) < 0) {
        LOG_ERROR("MpvPlayer", "mpv_initialize() failed");
        error_ = "Failed to initialize mpv";
        state_ = PlayerState::Error;
        mpv_terminate_destroy(mpv_);
        mpv_ = nullptr;
        return false;
    }

    // Set up render context with OpenGL
    mpv_opengl_init_params gl_init{get_proc_address, nullptr};
    int advanced_control = 1;

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, (void*)MPV_RENDER_API_TYPE_OPENGL},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init},
        {MPV_RENDER_PARAM_ADVANCED_CONTROL, &advanced_control},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };

    if (mpv_render_context_create(&mpv_gl_, mpv_, params) < 0) {
        LOG_ERROR("MpvPlayer", "mpv_render_context_create() failed");
        error_ = "Failed to create mpv render context";
        state_ = PlayerState::Error;
        mpv_terminate_destroy(mpv_);
        mpv_ = nullptr;
        return false;
    }

    // Set wakeup callbacks
    mpv_set_wakeup_callback(mpv_, on_mpv_events, this);
    mpv_render_context_set_update_callback(mpv_gl_, on_mpv_render_update, this);

    // Observe properties we care about
    mpv_observe_property(mpv_, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv_, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv_, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv_, 0, "media-title", MPV_FORMAT_STRING);
    mpv_observe_property(mpv_, 0, "idle-active", MPV_FORMAT_FLAG);

    LOG_INFO("MpvPlayer", "Initialized successfully");
    state_ = PlayerState::Idle;
    return true;
}

// ============================================================================
// Playback controls
// ============================================================================
void MpvPlayer::play(const std::string& url) {
    if (!mpv_) return;
    LOG_INFO("MpvPlayer", "Playing: %s", url.c_str());
    state_ = PlayerState::Loading;
    const char* cmd[] = {"loadfile", url.c_str(), nullptr};
    mpv_command_async(mpv_, 0, cmd);
}

void MpvPlayer::stop() {
    if (!mpv_) return;
    const char* cmd[] = {"stop", nullptr};
    mpv_command_async(mpv_, 0, cmd);
    state_ = PlayerState::Stopped;
}

void MpvPlayer::toggle_pause() {
    if (!mpv_) return;
    const char* cmd[] = {"cycle", "pause", nullptr};
    mpv_command_async(mpv_, 0, cmd);
}

void MpvPlayer::set_volume(int vol) {
    if (!mpv_) return;
    std::string v = std::to_string(vol);
    mpv_set_option_string(mpv_, "volume", v.c_str());
}

void MpvPlayer::seek(double seconds) {
    if (!mpv_) return;
    std::string pos = std::to_string(seconds);
    const char* cmd[] = {"seek", pos.c_str(), "absolute", nullptr};
    mpv_command_async(mpv_, 0, cmd);
}

// ============================================================================
// FBO management
// ============================================================================
void MpvPlayer::ensure_fbo(int w, int h) {
    if (w <= 0) w = 1;
    if (h <= 0) h = 1;
    if (fbo_ && tex_w_ == w && tex_h_ == h) return;

    // Delete old
    if (fbo_) { glDeleteFramebuffers(1, &fbo_); fbo_ = 0; }
    if (texture_) { glDeleteTextures(1, &texture_); texture_ = 0; }

    // Create FBO first
    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    // Create texture and attach to FBO
    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERROR("MpvPlayer", "FBO incomplete!");
    }

    // Unbind both
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    tex_w_ = w;
    tex_h_ = h;
    LOG_INFO("MpvPlayer", "FBO created: %dx%d, tex=%u, fbo=%u", w, h, texture_, fbo_);
}

// ============================================================================
// Render frame — call each ImGui frame
// ============================================================================
unsigned int MpvPlayer::render_frame(int width, int height) {
    if (!mpv_gl_) return 0;

    // Process any pending mpv events
    process_events();

    // Check if mpv wants to render
    uint64_t flags = mpv_render_context_update(mpv_gl_);
    if (!(flags & MPV_RENDER_UPDATE_FRAME)) {
        return texture_;  // Return last texture if no new frame
    }

    ensure_fbo(width, height);
    if (!fbo_) return 0;

    // Save current viewport
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    // Resize texture to match requested dimensions (like the working demo)
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Render mpv into our FBO
    mpv_opengl_fbo fbo_params{};
    fbo_params.fbo = (int)fbo_;
    fbo_params.w = width;
    fbo_params.h = height;
    fbo_params.internal_format = 0;

    int flip_y = 0;  // No flip — matches working imgui+mpv demos

    mpv_render_param render_params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &fbo_params},
        {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };

    mpv_render_context_render(mpv_gl_, render_params);

    // Restore viewport (mpv changes it)
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

    return texture_;
}

// ============================================================================
// Event processing
// ============================================================================
void MpvPlayer::process_events() {
    if (!mpv_) return;

    while (true) {
        mpv_event* event = mpv_wait_event(mpv_, 0);
        if (event->event_id == MPV_EVENT_NONE) break;

        switch (event->event_id) {
            case MPV_EVENT_FILE_LOADED:
                state_ = PlayerState::Playing;
                break;

            case MPV_EVENT_END_FILE: {
                auto* data = (mpv_event_end_file*)event->data;
                if (data->reason == MPV_END_FILE_REASON_ERROR) {
                    std::lock_guard<std::mutex> lock(prop_mutex_);
                    error_ = "Playback error";
                    state_ = PlayerState::Error;
                } else {
                    state_ = PlayerState::Stopped;
                }
                break;
            }

            case MPV_EVENT_PROPERTY_CHANGE: {
                auto* prop = (mpv_event_property*)event->data;
                if (prop->format == MPV_FORMAT_DOUBLE) {
                    double val = *(double*)prop->data;
                    if (strcmp(prop->name, "duration") == 0) duration_ = val;
                    else if (strcmp(prop->name, "time-pos") == 0) position_ = val;
                } else if (prop->format == MPV_FORMAT_FLAG) {
                    int val = *(int*)prop->data;
                    if (strcmp(prop->name, "pause") == 0) {
                        if (val && state_ == PlayerState::Playing) state_ = PlayerState::Paused;
                        else if (!val && state_ == PlayerState::Paused) state_ = PlayerState::Playing;
                    } else if (strcmp(prop->name, "idle-active") == 0) {
                        if (val) state_ = PlayerState::Idle;
                    }
                } else if (prop->format == MPV_FORMAT_STRING) {
                    if (strcmp(prop->name, "media-title") == 0) {
                        std::lock_guard<std::mutex> lock(prop_mutex_);
                        title_ = *(char**)prop->data;
                    }
                }
                break;
            }

            case MPV_EVENT_LOG_MESSAGE: {
                auto* msg = (mpv_event_log_message*)event->data;
                if (msg->log_level <= MPV_LOG_LEVEL_WARN) {
                    LOG_WARN("mpv", "[%s] %s", msg->prefix, msg->text);
                }
                break;
            }

            default:
                break;
        }
    }
}

// ============================================================================
// Accessors
// ============================================================================
std::string MpvPlayer::error_msg() const {
    std::lock_guard<std::mutex> lock(prop_mutex_);
    return error_;
}

std::string MpvPlayer::media_title() const {
    std::lock_guard<std::mutex> lock(prop_mutex_);
    return title_;
}

// ============================================================================
// Callbacks
// ============================================================================
void MpvPlayer::on_mpv_events(void* ctx) {
    auto* self = static_cast<MpvPlayer*>(ctx);
    self->wakeup_flag_ = true;
}

void MpvPlayer::on_mpv_render_update(void* ctx) {
    // Nothing needed — we poll in render_frame()
    (void)ctx;
}

} // namespace fincept::media

#else // !FINCEPT_HAS_MPV — stub implementation

namespace fincept::media {

MpvPlayer::MpvPlayer() = default;
MpvPlayer::~MpvPlayer() = default;

bool MpvPlayer::init() {
    error_ = "mpv not available (build without FINCEPT_HAS_MPV)";
    state_ = PlayerState::Error;
    return false;
}

void MpvPlayer::play(const std::string&) {}
void MpvPlayer::stop() {}
void MpvPlayer::toggle_pause() {}
void MpvPlayer::set_volume(int) {}
void MpvPlayer::seek(double) {}
unsigned int MpvPlayer::render_frame(int, int) { return 0; }
void MpvPlayer::ensure_fbo(int, int) {}
void MpvPlayer::process_events() {}
void MpvPlayer::cleanup() {}

std::string MpvPlayer::error_msg() const {
    return "mpv not available";
}

std::string MpvPlayer::media_title() const {
    return {};
}

void MpvPlayer::on_mpv_events(void*) {}
void MpvPlayer::on_mpv_render_update(void*) {}

} // namespace fincept::media

#endif
