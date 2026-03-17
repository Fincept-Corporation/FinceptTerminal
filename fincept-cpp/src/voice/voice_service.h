#pragma once
// Voice Service — orchestrates STT + TTS + audio capture/playback
// Mirrors Tauri ChatBubble.tsx voice mode workflow:
//   Enable voice → Start listening → Capture audio → STT transcribe →
//   Send to AI → Get response → TTS speak → Loop back to listening
//
// This is the main public API that the AI Chat screen and settings use.

#include "voice_types.h"
#include <string>
#include <functional>
#include <mutex>
#include <atomic>
#include <future>

namespace fincept::voice {

class VoiceService {
public:
    static VoiceService& instance();

    // ── Lifecycle ──
    bool init();
    void shutdown();

    // ── Voice Mode (full loop: listen → transcribe → callback → speak response) ──
    void enable_voice_mode();
    void disable_voice_mode();
    bool is_voice_mode() const { return voice_mode_; }

    // ── Manual recording (for mic button without voice mode loop) ──
    bool start_listening();
    void stop_listening();
    bool is_listening() const { return state_ == VoiceState::Listening; }

    // ── TTS ──
    void speak(const std::string& text);
    void stop_speaking();
    bool is_speaking() const { return state_ == VoiceState::Speaking; }

    // ── State ──
    VoiceState state() const { return state_; }
    float audio_level() const { return audio_level_; }
    const std::string& last_transcript() const { return last_transcript_; }
    const std::string& last_error() const { return last_error_; }

    // ── Configuration (loaded from / saved to DB) ──
    void load_config();
    void save_config();
    TTSConfig& tts_config() { return tts_config_; }
    STTConfig& stt_config() { return stt_config_; }
    const TTSConfig& tts_config() const { return tts_config_; }
    const STTConfig& stt_config() const { return stt_config_; }

    // ── Callbacks ──
    // Called when transcription is ready (UI should send the message)
    using TranscriptCallback = std::function<void(const std::string& text)>;
    void set_transcript_callback(TranscriptCallback cb) { on_transcript_ = cb; }

    // Called when state changes (UI should update indicators)
    using StateCallback = std::function<void(VoiceState)>;
    void set_state_callback(StateCallback cb) { on_state_change_ = cb; }

    VoiceService(const VoiceService&) = delete;
    VoiceService& operator=(const VoiceService&) = delete;

private:
    VoiceService() = default;

    std::atomic<VoiceState> state_{VoiceState::Idle};
    std::atomic<bool> voice_mode_{false};
    std::atomic<float> audio_level_{0.0f};

    std::mutex mutex_;
    TTSConfig tts_config_;
    STTConfig stt_config_;

    std::string last_transcript_;
    std::string last_error_;

    TranscriptCallback on_transcript_;
    StateCallback on_state_change_;

    std::future<void> active_task_;

    void set_state(VoiceState new_state);

    // Internal workflow
    void process_recording();     // Stop recording → transcribe → callback
    void voice_mode_loop();       // Full loop cycle
};

} // namespace fincept::voice
