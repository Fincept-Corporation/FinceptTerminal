#include "voice_service.h"
#include "audio_capture.h"
#include "audio_player.h"
#include "tts_engine.h"
#include "stt_engine.h"
#include "storage/database.h"
#include "core/logger.h"
#include <thread>
#include <chrono>
#include <cstdlib>

namespace fincept::voice {

VoiceService& VoiceService::instance() {
    static VoiceService inst;
    return inst;
}

bool VoiceService::init() {
    load_config();

    bool ok = true;
    ok &= AudioCapture::instance().init();
    ok &= AudioPlayer::instance().init();
    ok &= TTSEngine::instance().init();
    ok &= STTEngine::instance().init();

    AudioCapture::instance().set_level_callback([this](float level) {
        audio_level_ = level;
    });

    AudioPlayer::instance().set_finish_callback([this]() {
        if (voice_mode_) {
            start_listening();
        } else {
            set_state(VoiceState::Idle);
        }
    });

    LOG_INFO("Voice", "Initialized (capture=%s, ok=%s)",
             AudioCapture::instance().is_initialized() ? "OK" : "FAIL",
             ok ? "OK" : "FAIL");
    return ok;
}

void VoiceService::shutdown() {
    disable_voice_mode();
    stop_speaking();
    stop_listening();

    AudioCapture::instance().shutdown();
    AudioPlayer::instance().shutdown();
    TTSEngine::instance().shutdown();
    STTEngine::instance().shutdown();
}

void VoiceService::set_state(VoiceState new_state) {
    state_ = new_state;
    if (on_state_change_) {
        on_state_change_(new_state);
    }
}

void VoiceService::load_config() {
    auto provider_str = db::ops::get_setting("voice_tts_provider");
    tts_config_.provider = provider_str ? provider_from_string(*provider_str) : TTSProvider::Native;

    auto voice_id = db::ops::get_setting("voice_tts_voice_id");
    tts_config_.voice_id = voice_id.value_or("");

    auto rate_str = db::ops::get_setting("voice_tts_rate");
    tts_config_.rate = rate_str ? std::stof(*rate_str) : 1.0f;

    auto el_key = db::ops::get_setting("elevenlabs_api_key");
    tts_config_.elevenlabs_key = el_key.value_or("");

    auto oai_key = db::ops::get_setting("openai_tts_api_key");
    if (oai_key) {
        tts_config_.openai_key = *oai_key;
    } else {
        auto main_key = db::ops::get_setting("openai_api_key");
        tts_config_.openai_key = main_key.value_or("");
    }

    auto stt_prov = db::ops::get_setting("voice_stt_provider");
    stt_config_.provider = stt_prov ? stt_provider_from_string(*stt_prov) : STTProvider::OpenAIAPI;
    stt_config_.openai_key = tts_config_.openai_key;
    stt_config_.language = "en";

    LOG_INFO("Voice", "Config: TTS=%s STT=%s",
             provider_to_string(tts_config_.provider),
             stt_provider_to_string(stt_config_.provider));
}

void VoiceService::save_config() {
    db::ops::save_setting("voice_tts_provider",
                          provider_to_string(tts_config_.provider), "voice");
    db::ops::save_setting("voice_tts_voice_id",
                          tts_config_.voice_id, "voice");
    db::ops::save_setting("voice_tts_rate",
                          std::to_string(tts_config_.rate), "voice");
    db::ops::save_setting("voice_stt_provider",
                          stt_provider_to_string(stt_config_.provider), "voice");

    if (!tts_config_.elevenlabs_key.empty()) {
        db::ops::save_setting("elevenlabs_api_key",
                              tts_config_.elevenlabs_key, "api");
    }
    if (!tts_config_.openai_key.empty()) {
        db::ops::save_setting("openai_tts_api_key",
                              tts_config_.openai_key, "api");
    }
}

void VoiceService::enable_voice_mode() {
    if (voice_mode_) return;
    voice_mode_ = true;
    LOG_INFO("Voice", "Voice mode ENABLED");
    start_listening();
}

void VoiceService::disable_voice_mode() {
    voice_mode_ = false;
    stop_listening();
    stop_speaking();
    set_state(VoiceState::Idle);
    LOG_INFO("Voice", "Voice mode DISABLED");
}

bool VoiceService::start_listening() {
    if (state_ == VoiceState::Listening) return true;

    auto& capture = AudioCapture::instance();
    if (!capture.is_initialized()) {
        if (!capture.init()) {
            last_error_ = "Microphone not available";
            set_state(VoiceState::Error);
            return false;
        }
    }

    if (!capture.start_recording()) {
        last_error_ = "Failed to start recording";
        set_state(VoiceState::Error);
        return false;
    }

    set_state(VoiceState::Listening);
    return true;
}

void VoiceService::stop_listening() {
    auto& capture = AudioCapture::instance();
    if (!capture.is_recording()) return;

    capture.stop_recording();

    if (capture.get_duration() > 0.3f) {
        process_recording();
    } else {
        capture.clear();
        if (voice_mode_) {
            set_state(VoiceState::Listening);
            capture.start_recording();
        } else {
            set_state(VoiceState::Idle);
        }
    }
}

void VoiceService::process_recording() {
    set_state(VoiceState::Processing);

    auto wav_data = AudioCapture::instance().get_recorded_wav();
    AudioCapture::instance().clear();

    if (wav_data.empty()) {
        if (voice_mode_) start_listening();
        else set_state(VoiceState::Idle);
        return;
    }

    active_task_ = std::async(std::launch::async, [this, wav_data]() {
        auto future = STTEngine::instance().transcribe(wav_data, stt_config_);
        auto result = future.get();

        if (result.success && !result.text.empty()) {
            last_transcript_ = result.text;
            LOG_INFO("Voice", "Transcript: \"%s\"", result.text.c_str());

            if (on_transcript_) {
                on_transcript_(result.text);
            }
        } else {
            last_error_ = result.error.empty() ? "No speech detected" : result.error;
            LOG_WARN("Voice", "Transcription failed: %s", last_error_.c_str());

            if (voice_mode_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                start_listening();
            } else {
                set_state(VoiceState::Idle);
            }
        }
    });
}

void VoiceService::speak(const std::string& text) {
    if (text.empty()) return;

    if (AudioCapture::instance().is_recording()) {
        AudioCapture::instance().stop_recording();
    }
    TTSEngine::instance().stop();

    set_state(VoiceState::Speaking);

    active_task_ = std::async(std::launch::async, [this, text]() {
        auto future = TTSEngine::instance().speak(text, tts_config_);
        bool ok = future.get();

        if (!ok) {
            LOG_WARN("Voice", "TTS failed");
            if (voice_mode_) {
                start_listening();
            } else {
                set_state(VoiceState::Idle);
            }
        }
    });
}

void VoiceService::stop_speaking() {
    TTSEngine::instance().stop();
    AudioPlayer::instance().stop();
    if (state_ == VoiceState::Speaking) {
        set_state(VoiceState::Idle);
    }
}

} // namespace fincept::voice
