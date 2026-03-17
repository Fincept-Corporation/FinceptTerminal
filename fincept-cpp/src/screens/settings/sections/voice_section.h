#pragma once
// Voice Settings Section — mirrors Tauri ChatBubbleSection.tsx exactly
// Panels: Enable toggle, TTS provider grid, voice list, speed slider,
//         test voice, API keys (collapsible), auto-start toggle, info card

#include <imgui.h>
#include "voice/voice_types.h"
#include <string>
#include <vector>

namespace fincept::settings {

class VoiceSection {
public:
    void render();

private:
    bool initialized_ = false;

    // Enable/disable
    bool voice_enabled_ = true;

    // TTS config
    int selected_provider_ = 0;   // index into tts_provider_info()
    int selected_voice_ = -1;     // index into current_voices_
    float speech_rate_ = 1.4f;    // default 1.4x like Tauri

    // STT config
    int selected_stt_provider_ = 0;

    // API keys
    char elevenlabs_key_buf_[256] = {};
    char openai_key_buf_[256] = {};
    bool show_elevenlabs_key_ = false;
    bool show_openai_key_ = false;
    bool api_keys_expanded_ = false;

    // Voice list for current provider
    std::vector<voice::VoiceDef> current_voices_;

    // Auto voice mode
    bool auto_voice_mode_ = false;

    // Status message
    std::string status_msg_;
    double status_expire_ = 0;
    bool status_is_error_ = false;

    void init();
    void load_voices();
    void save_provider();
    void save_voice();
    void save_rate();
    void save_api_key(const char* key_name, const char* value);
    void save_auto_voice();
    void save_enabled();
    void test_voice();
    void show_status(const char* msg, bool is_error = false);

    // Render sub-panels (mirroring Tauri sections)
    void render_enable_toggle();
    void render_tts_panel();
    void render_api_keys_panel();
    void render_auto_voice_toggle();
    void render_info_card();
};

} // namespace fincept::settings
