#pragma once
// Voice types — shared enums, configs, voice definitions
// Mirrors ttsService.ts TTSProvider, TTSVoice, TTSConfig from Tauri app

#include <string>
#include <vector>

namespace fincept::voice {

// TTS provider enum — matches Tauri's TTSProvider type
enum class TTSProvider {
    Native,      // Windows SAPI5 / macOS AVSpeech (free, offline)
    ElevenLabs,  // ElevenLabs API (paid, cloud)
    OpenAI,      // OpenAI TTS API (paid, cloud)
};

// STT provider enum
enum class STTProvider {
    Whisper,     // whisper.cpp local (offline)
    OpenAIAPI,   // OpenAI Whisper API (cloud)
};

// Voice definition
struct VoiceDef {
    std::string id;
    std::string name;
    TTSProvider provider;
    std::string language;
    std::string gender;  // "male", "female", "neutral"
};

// TTS configuration (persisted to DB)
struct TTSConfig {
    TTSProvider provider = TTSProvider::Native;
    std::string voice_id;
    float rate = 1.0f;           // 0.5 - 2.0
    std::string elevenlabs_key;
    std::string openai_key;
};

// STT configuration
struct STTConfig {
    STTProvider provider = STTProvider::OpenAIAPI;
    std::string model_path;      // whisper.cpp model file
    std::string language = "en";
    std::string openai_key;
};

// Voice service state
enum class VoiceState {
    Idle,
    Listening,
    Processing,
    Speaking,
    Error,
};

// ============================================================================
// Predefined voice lists — mirrors Tauri ttsService.ts
// ============================================================================

// ElevenLabs voices
inline const std::vector<VoiceDef>& elevenlabs_voices() {
    static const std::vector<VoiceDef> voices = {
        {"EXAVITQu4vr4xnSDxMaL", "Sarah",     TTSProvider::ElevenLabs, "en-US", "female"},
        {"TX3LPaxmHKxFdv7VOQHJ", "Liam",      TTSProvider::ElevenLabs, "en-US", "male"},
        {"XB0fDUnXU5powFXDhCwa", "Charlotte",  TTSProvider::ElevenLabs, "en-US", "female"},
        {"pFZP5JQG7iQjIQuC4Bku", "Lily",       TTSProvider::ElevenLabs, "en-US", "female"},
        {"onwK4e9ZLuTAKqWW03F9", "Daniel",     TTSProvider::ElevenLabs, "en-US", "male"},
        {"21m00Tcm4TlvDq8ikWAM", "Rachel",     TTSProvider::ElevenLabs, "en-US", "female"},
    };
    return voices;
}

// OpenAI TTS voices
inline const std::vector<VoiceDef>& openai_voices() {
    static const std::vector<VoiceDef> voices = {
        {"alloy",   "Alloy",   TTSProvider::OpenAI, "en-US", "neutral"},
        {"echo",    "Echo",    TTSProvider::OpenAI, "en-US", "male"},
        {"fable",   "Fable",   TTSProvider::OpenAI, "en-US", "male"},
        {"onyx",    "Onyx",    TTSProvider::OpenAI, "en-US", "male"},
        {"nova",    "Nova",    TTSProvider::OpenAI, "en-US", "female"},
        {"shimmer", "Shimmer", TTSProvider::OpenAI, "en-US", "female"},
    };
    return voices;
}

// Native platform voices (discovered at runtime on Windows SAPI5)
// These are populated dynamically by tts_engine

// Provider metadata for UI
struct ProviderInfo {
    TTSProvider id;
    const char* name;
    const char* description;
    bool requires_key;
};

inline const std::vector<ProviderInfo>& tts_provider_info() {
    static const std::vector<ProviderInfo> providers = {
        {TTSProvider::Native,     "System Native", "Uses your OS voices (free, offline)",        false},
        {TTSProvider::ElevenLabs, "ElevenLabs",    "Ultra-realistic AI voices (API key needed)", true},
        {TTSProvider::OpenAI,     "OpenAI TTS",    "OpenAI text-to-speech (API key needed)",     true},
    };
    return providers;
}

// Provider string conversion
inline const char* provider_to_string(TTSProvider p) {
    switch (p) {
        case TTSProvider::Native:     return "native";
        case TTSProvider::ElevenLabs: return "elevenlabs";
        case TTSProvider::OpenAI:     return "openai";
    }
    return "native";
}

inline TTSProvider provider_from_string(const std::string& s) {
    if (s == "elevenlabs") return TTSProvider::ElevenLabs;
    if (s == "openai")     return TTSProvider::OpenAI;
    return TTSProvider::Native;
}

inline const char* stt_provider_to_string(STTProvider p) {
    switch (p) {
        case STTProvider::Whisper:  return "whisper";
        case STTProvider::OpenAIAPI: return "openai_api";
    }
    return "openai_api";
}

inline STTProvider stt_provider_from_string(const std::string& s) {
    if (s == "whisper") return STTProvider::Whisper;
    return STTProvider::OpenAIAPI;
}

inline const char* voice_state_to_string(VoiceState s) {
    switch (s) {
        case VoiceState::Idle:       return "IDLE";
        case VoiceState::Listening:  return "LISTENING";
        case VoiceState::Processing: return "PROCESSING";
        case VoiceState::Speaking:   return "SPEAKING";
        case VoiceState::Error:      return "ERROR";
    }
    return "IDLE";
}

} // namespace fincept::voice
