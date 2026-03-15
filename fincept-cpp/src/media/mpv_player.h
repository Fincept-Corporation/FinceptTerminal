#pragma once
// Lightweight mpv wrapper — renders video into an OpenGL texture for ImGui::Image()
// Uses libmpv render API with FBO offscreen rendering
// Cross-platform: Windows, Linux, macOS

#include <string>
#include <functional>
#include <mutex>
#include <atomic>

// Forward declarations — avoid pulling mpv headers into every TU
struct mpv_handle;
struct mpv_render_context;

namespace fincept::media {

enum class PlayerState { Idle, Loading, Playing, Paused, Stopped, Error };

class MpvPlayer {
public:
    MpvPlayer();
    ~MpvPlayer();

    // Non-copyable
    MpvPlayer(const MpvPlayer&) = delete;
    MpvPlayer& operator=(const MpvPlayer&) = delete;

    // Initialize mpv + render context. Call once after GL context is current.
    bool init();

    // Load and play a URL (YouTube URLs require yt-dlp in PATH)
    void play(const std::string& url);
    void stop();
    void toggle_pause();
    void set_volume(int vol);  // 0-100

    // Call each frame to render into internal FBO. Returns GL texture ID (0 if not ready).
    // width/height = desired render size in pixels
    unsigned int render_frame(int width, int height);

    // State queries
    PlayerState state() const { return state_.load(); }
    bool is_playing() const { return state_ == PlayerState::Playing; }
    double duration() const { return duration_; }
    double position() const { return position_; }
    std::string error_msg() const;
    std::string media_title() const;

    // Seek to position (seconds)
    void seek(double seconds);

private:
    mpv_handle* mpv_ = nullptr;
    mpv_render_context* mpv_gl_ = nullptr;

    // OpenGL FBO for offscreen rendering
    unsigned int fbo_ = 0;
    unsigned int texture_ = 0;
    int tex_w_ = 0;
    int tex_h_ = 0;

    std::atomic<PlayerState> state_{PlayerState::Idle};
    double duration_ = 0;
    double position_ = 0;
    std::string error_;
    std::string title_;
    mutable std::mutex prop_mutex_;

    std::atomic<bool> wakeup_flag_{false};

    void ensure_fbo(int w, int h);
    void process_events();
    void cleanup();

    static void on_mpv_events(void* ctx);
    static void on_mpv_render_update(void* ctx);
};

} // namespace fincept::media
