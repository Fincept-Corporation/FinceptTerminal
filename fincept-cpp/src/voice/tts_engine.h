#pragma once
// TTS Engine — multi-provider text-to-speech
// Mirrors Tauri ttsService.ts with 3 providers:
//   1. Native (SAPI5 on Windows, AVSpeechSynthesizer on macOS)
//   2. ElevenLabs API (cloud)
//   3. OpenAI TTS API (cloud)

#include "voice_types.h"
#include <string>
#include <vector>
#include <functional>
#include <future>
#include <atomic>

namespace fincept::voice {

class TTSEngine {
public:
    static TTSEngine& instance();

    // Initialize platform-native TTS
    bool init();
    void shutdown();

    // Speak text using configured provider
    // Preprocesses text, then routes to provider
    // Returns future that resolves when speech finishes
    std::future<bool> speak(const std::string& text, const TTSConfig& config);

    // Stop all speech immediately
    void stop();

    bool is_speaking() const { return speaking_; }

    // Get available voices for a provider
    std::vector<VoiceDef> get_voices(TTSProvider provider);

    // Get native platform voices (discovered at runtime)
    const std::vector<VoiceDef>& get_native_voices();

    TTSEngine(const TTSEngine&) = delete;
    TTSEngine& operator=(const TTSEngine&) = delete;

private:
    TTSEngine() = default;
    ~TTSEngine();

    std::atomic<bool> initialized_{false};
    std::atomic<bool> speaking_{false};

    // Native voices discovered at runtime
    std::vector<VoiceDef> native_voices_;
    bool native_voices_loaded_ = false;

    // Provider-specific speak methods
    bool speak_native(const std::string& text, const TTSConfig& config);
    bool speak_elevenlabs(const std::string& text, const TTSConfig& config);
    bool speak_openai(const std::string& text, const TTSConfig& config);

    // Discover native platform voices
    void discover_native_voices();

    // Platform-specific native TTS handle
    void* native_handle_ = nullptr;
};

} // namespace fincept::voice
