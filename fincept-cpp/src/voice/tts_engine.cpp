#include "tts_engine.h"
#include "text_preprocessor.h"
#include "audio_player.h"
#include "http/http_client.h"
#include "storage/database.h"
#include "core/logger.h"
#include <nlohmann/json.hpp>
#include <thread>

#ifdef _WIN32
#include <sapi.h>
#include <sphelper.h>
#include <atlbase.h>
#pragma comment(lib, "ole32.lib")
#endif

namespace fincept::voice {

using json = nlohmann::json;

TTSEngine& TTSEngine::instance() {
    static TTSEngine inst;
    return inst;
}

TTSEngine::~TTSEngine() {
    shutdown();
}

bool TTSEngine::init() {
    if (initialized_) return true;

#ifdef _WIN32
    ISpVoice* voice = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL,
                                   IID_ISpVoice, (void**)&voice);
    if (SUCCEEDED(hr) && voice) {
        native_handle_ = voice;
        LOG_INFO("TTS", "Windows SAPI5 initialized");
    } else {
        LOG_WARN("TTS", "SAPI5 not available");
    }
#endif

    discover_native_voices();
    initialized_ = true;
    LOG_INFO("TTS", "Engine initialized with %zu native voices", native_voices_.size());
    return true;
}

void TTSEngine::shutdown() {
    stop();

#ifdef _WIN32
    if (native_handle_) {
        auto* voice = static_cast<ISpVoice*>(native_handle_);
        voice->Release();
        native_handle_ = nullptr;
    }
#endif

    initialized_ = false;
}

void TTSEngine::discover_native_voices() {
    native_voices_.clear();

#ifdef _WIN32
    CComPtr<IEnumSpObjectTokens> enum_tokens;
    HRESULT hr = SpEnumTokens(SPCAT_VOICES, nullptr, nullptr, &enum_tokens);
    if (FAILED(hr)) return;

    ULONG count = 0;
    enum_tokens->GetCount(&count);

    for (ULONG i = 0; i < count; i++) {
        CComPtr<ISpObjectToken> token;
        if (FAILED(enum_tokens->Next(1, &token, nullptr))) continue;

        WCHAR* name_w = nullptr;
        if (SUCCEEDED(SpGetDescription(token, &name_w)) && name_w) {
            int len = WideCharToMultiByte(CP_UTF8, 0, name_w, -1, nullptr, 0, nullptr, nullptr);
            std::string name(len - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, name_w, -1, name.data(), len, nullptr, nullptr);
            CoTaskMemFree(name_w);

            WCHAR* id_w = nullptr;
            token->GetId(&id_w);
            std::string id;
            if (id_w) {
                int id_len = WideCharToMultiByte(CP_UTF8, 0, id_w, -1, nullptr, 0, nullptr, nullptr);
                id.resize(id_len - 1);
                WideCharToMultiByte(CP_UTF8, 0, id_w, -1, id.data(), id_len, nullptr, nullptr);
                CoTaskMemFree(id_w);
            }

            VoiceDef def;
            def.id = id.empty() ? ("native_" + std::to_string(i)) : id;
            def.name = name;
            def.provider = TTSProvider::Native;
            def.language = "en-US";
            def.gender = "neutral";
            native_voices_.push_back(def);
        }
    }
#elif defined(__APPLE__)
    VoiceDef def;
    def.id = "com.apple.ttsbundle.Samantha-compact";
    def.name = "Samantha";
    def.provider = TTSProvider::Native;
    def.language = "en-US";
    def.gender = "female";
    native_voices_.push_back(def);
#else
    VoiceDef def;
    def.id = "default";
    def.name = "Default System Voice";
    def.provider = TTSProvider::Native;
    def.language = "en";
    def.gender = "neutral";
    native_voices_.push_back(def);
#endif

    native_voices_loaded_ = true;
}

std::vector<VoiceDef> TTSEngine::get_voices(TTSProvider provider) {
    switch (provider) {
        case TTSProvider::Native:     return native_voices_;
        case TTSProvider::ElevenLabs: return elevenlabs_voices();
        case TTSProvider::OpenAI:     return openai_voices();
    }
    return {};
}

const std::vector<VoiceDef>& TTSEngine::get_native_voices() {
    if (!native_voices_loaded_) discover_native_voices();
    return native_voices_;
}

std::future<bool> TTSEngine::speak(const std::string& text, const TTSConfig& config) {
    return std::async(std::launch::async, [this, text, config]() -> bool {
        if (text.empty()) return false;

        std::string cleaned = TextPreprocessor::clean(text);
        if (cleaned.empty()) return false;

        speaking_ = true;
        bool result = false;

        try {
            switch (config.provider) {
                case TTSProvider::Native:
                    result = speak_native(cleaned, config);
                    break;
                case TTSProvider::ElevenLabs:
                    result = speak_elevenlabs(cleaned, config);
                    break;
                case TTSProvider::OpenAI:
                    result = speak_openai(cleaned, config);
                    break;
            }
        } catch (const std::exception& e) {
            LOG_ERROR("TTS", "Exception: %s", e.what());
        }

        speaking_ = false;
        return result;
    });
}

void TTSEngine::stop() {
    speaking_ = false;

#ifdef _WIN32
    if (native_handle_) {
        auto* voice = static_cast<ISpVoice*>(native_handle_);
        voice->Speak(L"", SPF_PURGEBEFORESPEAK, nullptr);
    }
#endif

    AudioPlayer::instance().stop();
}

bool TTSEngine::speak_native(const std::string& text, const TTSConfig& config) {
#ifdef _WIN32
    if (!native_handle_) {
        LOG_ERROR("TTS", "SAPI5 not initialized");
        return false;
    }

    auto* voice = static_cast<ISpVoice*>(native_handle_);

    // Set voice if specified
    if (!config.voice_id.empty()) {
        CComPtr<IEnumSpObjectTokens> enum_tokens;
        if (SUCCEEDED(SpEnumTokens(SPCAT_VOICES, nullptr, nullptr, &enum_tokens))) {
            ULONG count = 0;
            enum_tokens->GetCount(&count);
            for (ULONG i = 0; i < count; i++) {
                CComPtr<ISpObjectToken> token;
                if (FAILED(enum_tokens->Next(1, &token, nullptr))) continue;

                WCHAR* id_w = nullptr;
                token->GetId(&id_w);
                if (id_w) {
                    int len = WideCharToMultiByte(CP_UTF8, 0, id_w, -1, nullptr, 0, nullptr, nullptr);
                    std::string id(len - 1, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, id_w, -1, id.data(), len, nullptr, nullptr);
                    CoTaskMemFree(id_w);

                    if (id == config.voice_id) {
                        voice->SetVoice(token);
                        break;
                    }
                }
            }
        }
    }

    // Set rate: SAPI rate is -10 to 10, our rate is 0.5 to 2.0
    long sapi_rate = (long)((config.rate - 1.0f) * 10.0f);
    if (sapi_rate < -10) sapi_rate = -10;
    if (sapi_rate > 10) sapi_rate = 10;
    voice->SetRate(sapi_rate);

    // Convert to wide string
    int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring wtext(wlen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wtext.data(), wlen);

    HRESULT hr = voice->Speak(wtext.c_str(), SPF_DEFAULT, nullptr);
    return SUCCEEDED(hr);

#elif defined(__APPLE__)
    LOG_WARN("TTS", "macOS native TTS not yet implemented");
    (void)text; (void)config;
    return false;
#else
    (void)config;
    std::string cmd = "espeak-ng \"" + text + "\" 2>/dev/null";
    int ret = system(cmd.c_str());
    return ret == 0;
#endif
}

bool TTSEngine::speak_elevenlabs(const std::string& text, const TTSConfig& config) {
    if (config.elevenlabs_key.empty()) {
        LOG_WARN("TTS", "ElevenLabs API key not set, falling back to native");
        return speak_native(text, config);
    }

    std::string voice_id = config.voice_id;
    if (voice_id.empty()) voice_id = "EXAVITQu4vr4xnSDxMaL";

    std::string url = "https://api.elevenlabs.io/v1/text-to-speech/" + voice_id;

    json body = {
        {"text", text},
        {"model_id", "eleven_monolingual_v1"},
        {"voice_settings", {
            {"stability", 0.5},
            {"similarity_boost", 0.75},
        }}
    };

    http::Headers headers = {
        {"Accept", "audio/mpeg"},
        {"Content-Type", "application/json"},
        {"xi-api-key", config.elevenlabs_key},
    };

    auto& client = http::HttpClient::instance();
    auto resp = client.post(url, body.dump(), headers);

    if (!resp.success || resp.status_code != 200) {
        LOG_ERROR("TTS", "ElevenLabs API error: %d %s", resp.status_code, resp.error.c_str());
        return speak_native(text, config);
    }

    std::vector<uint8_t> mp3_data(resp.body.begin(), resp.body.end());
    AudioPlayer::instance().init();
    return AudioPlayer::instance().play_mp3(mp3_data);
}

bool TTSEngine::speak_openai(const std::string& text, const TTSConfig& config) {
    if (config.openai_key.empty()) {
        LOG_WARN("TTS", "OpenAI API key not set, falling back to native");
        return speak_native(text, config);
    }

    std::string voice_id = config.voice_id;
    if (voice_id.empty()) voice_id = "alloy";

    json body = {
        {"model", "tts-1"},
        {"input", text},
        {"voice", voice_id},
        {"speed", config.rate},
    };

    http::Headers headers = {
        {"Authorization", "Bearer " + config.openai_key},
        {"Content-Type", "application/json"},
    };

    auto& client = http::HttpClient::instance();
    auto resp = client.post("https://api.openai.com/v1/audio/speech", body.dump(), headers);

    if (!resp.success || resp.status_code != 200) {
        LOG_ERROR("TTS", "OpenAI TTS error: %d %s", resp.status_code, resp.error.c_str());
        return speak_native(text, config);
    }

    std::vector<uint8_t> mp3_data(resp.body.begin(), resp.body.end());
    AudioPlayer::instance().init();
    return AudioPlayer::instance().play_mp3(mp3_data);
}

} // namespace fincept::voice
