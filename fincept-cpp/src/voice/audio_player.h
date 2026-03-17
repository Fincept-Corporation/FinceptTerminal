#pragma once
// Audio Player — plays audio buffers (MP3/WAV from cloud TTS responses)
// Uses miniaudio for cross-platform playback

#include <vector>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <functional>

namespace fincept::voice {

class AudioPlayer {
public:
    static AudioPlayer& instance();

    bool init();
    void shutdown();

    // Play raw PCM samples (16-bit, mono, 16kHz or specified rate)
    bool play_pcm(const std::vector<int16_t>& samples, int sample_rate = 16000);

    // Play from WAV file bytes
    bool play_wav(const std::vector<uint8_t>& wav_data);

    // Play from MP3 file bytes (from ElevenLabs/OpenAI API response)
    bool play_mp3(const std::vector<uint8_t>& mp3_data);

    // Stop playback
    void stop();

    bool is_playing() const { return playing_; }

    // Callback when playback finishes
    using FinishCallback = std::function<void()>;
    void set_finish_callback(FinishCallback cb) { finish_callback_ = cb; }

    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    // Called by miniaudio — do not call directly
    void on_playback_data(void* output, uint32_t frame_count);

private:
    AudioPlayer() = default;
    ~AudioPlayer();

    std::atomic<bool> initialized_{false};
    std::atomic<bool> playing_{false};

    std::mutex buffer_mutex_;
    std::vector<int16_t> playback_samples_;
    size_t playback_pos_ = 0;
    int playback_rate_ = 24000;  // default for OpenAI TTS

    FinishCallback finish_callback_;

    void* device_ = nullptr;  // ma_device*

    bool start_device(int sample_rate);
    void stop_device();
};

} // namespace fincept::voice
