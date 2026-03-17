#pragma once
// Audio Capture — microphone recording via miniaudio
// Records PCM audio to a buffer for STT processing
// Cross-platform: Windows, macOS, Linux

#include <vector>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <functional>

namespace fincept::voice {

// Audio format: 16kHz mono 16-bit PCM (optimal for Whisper/STT)
constexpr int SAMPLE_RATE = 16000;
constexpr int CHANNELS = 1;
constexpr int BITS_PER_SAMPLE = 16;

class AudioCapture {
public:
    static AudioCapture& instance();

    // Initialize audio device
    bool init();
    void shutdown();
    bool is_initialized() const { return initialized_; }

    // Recording control
    bool start_recording();
    void stop_recording();
    bool is_recording() const { return recording_; }

    // Get recorded audio as 16-bit PCM samples
    std::vector<int16_t> get_recorded_samples();

    // Get recorded audio as WAV file bytes (for API upload)
    std::vector<uint8_t> get_recorded_wav();

    // Get recording duration in seconds
    float get_duration() const;

    // Clear recorded data
    void clear();

    // Callback for real-time audio level (for UI visualization)
    using LevelCallback = std::function<void(float)>;
    void set_level_callback(LevelCallback cb) { level_callback_ = cb; }

    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;

    // Called by miniaudio callback — do not call directly
    void on_audio_data(const void* input, uint32_t frame_count);

private:
    AudioCapture() = default;
    ~AudioCapture();

    std::atomic<bool> initialized_{false};
    std::atomic<bool> recording_{false};

    std::mutex buffer_mutex_;
    std::vector<int16_t> samples_;

    LevelCallback level_callback_;

    // miniaudio device handle (opaque pointer to avoid including miniaudio.h in header)
    void* device_ = nullptr;
};

} // namespace fincept::voice
