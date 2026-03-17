#pragma once
// STT Engine — speech-to-text via OpenAI Whisper API
// Sends recorded WAV audio to OpenAI for transcription
// Future: local whisper.cpp integration (add whisper-cpp vcpkg port)

#include "voice_types.h"
#include <string>
#include <vector>
#include <future>
#include <atomic>
#include <cstdint>

namespace fincept::voice {

// Transcription result
struct TranscriptionResult {
    std::string text;
    std::string language;
    float duration = 0.0f;
    bool success = false;
    std::string error;
};

class STTEngine {
public:
    static STTEngine& instance();

    bool init();
    void shutdown();

    // Transcribe WAV audio bytes via configured provider
    std::future<TranscriptionResult> transcribe(
        const std::vector<uint8_t>& wav_data, const STTConfig& config);

    // Transcribe PCM samples (converts to WAV internally)
    std::future<TranscriptionResult> transcribe_pcm(
        const std::vector<int16_t>& samples, int sample_rate,
        const STTConfig& config);

    STTEngine(const STTEngine&) = delete;
    STTEngine& operator=(const STTEngine&) = delete;

private:
    STTEngine() = default;

    std::atomic<bool> initialized_{false};

    // Provider-specific transcription
    TranscriptionResult transcribe_openai_api(
        const std::vector<uint8_t>& wav_data, const STTConfig& config);
};

} // namespace fincept::voice
