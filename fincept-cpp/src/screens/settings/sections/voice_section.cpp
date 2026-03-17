#include "voice_section.h"
#include "voice/voice_service.h"
#include "voice/tts_engine.h"
#include "storage/database.h"
#include "ui/theme.h"
#include "core/logger.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace fincept::settings {

// ============================================================================
// Init — load all settings from DB (mirrors Tauri useEffect load)
// ============================================================================
void VoiceSection::init() {
    if (initialized_) return;

    auto& svc = voice::VoiceService::instance();
    svc.load_config();

    // Voice enabled
    auto enabled = db::ops::get_setting("voice_enabled");
    voice_enabled_ = !enabled || *enabled != "false";

    // TTS provider
    auto& tts = svc.tts_config();
    selected_provider_ = static_cast<int>(tts.provider);
    speech_rate_ = tts.rate;
    if (speech_rate_ < 0.5f) speech_rate_ = 1.4f;  // default like Tauri

    // API keys
    std::strncpy(elevenlabs_key_buf_, tts.elevenlabs_key.c_str(),
                 sizeof(elevenlabs_key_buf_) - 1);
    std::strncpy(openai_key_buf_, tts.openai_key.c_str(),
                 sizeof(openai_key_buf_) - 1);

    // STT
    auto& stt = svc.stt_config();
    selected_stt_provider_ = static_cast<int>(stt.provider);

    // Auto voice
    auto auto_voice = db::ops::get_setting("voice_auto_start");
    auto_voice_mode_ = auto_voice && *auto_voice == "true";

    load_voices();
    initialized_ = true;
}

void VoiceSection::load_voices() {
    auto provider = static_cast<voice::TTSProvider>(selected_provider_);
    current_voices_ = voice::TTSEngine::instance().get_voices(provider);

    // Find currently selected voice
    auto& svc = voice::VoiceService::instance();
    auto& tts = svc.tts_config();
    selected_voice_ = -1;
    for (int i = 0; i < (int)current_voices_.size(); i++) {
        if (current_voices_[i].id == tts.voice_id) {
            selected_voice_ = i;
            break;
        }
    }
}

// ============================================================================
// Save helpers — each mirrors a Tauri handleXXX function
// ============================================================================
void VoiceSection::save_provider() {
    auto& svc = voice::VoiceService::instance();
    svc.tts_config().provider = static_cast<voice::TTSProvider>(selected_provider_);
    selected_voice_ = -1;  // Reset voice on provider change
    svc.tts_config().voice_id.clear();
    svc.save_config();
    load_voices();

    auto& providers = voice::tts_provider_info();
    char buf[128];
    std::snprintf(buf, sizeof(buf), "TTS provider changed to %s", providers[selected_provider_].name);
    show_status(buf);
}

void VoiceSection::save_voice() {
    if (selected_voice_ < 0 || selected_voice_ >= (int)current_voices_.size()) return;
    auto& svc = voice::VoiceService::instance();
    svc.tts_config().voice_id = current_voices_[selected_voice_].id;
    svc.save_config();
    show_status("Voice updated");
}

void VoiceSection::save_rate() {
    auto& svc = voice::VoiceService::instance();
    svc.tts_config().rate = speech_rate_;
    svc.save_config();
}

void VoiceSection::save_api_key(const char* key_name, const char* value) {
    db::ops::save_setting(key_name, value, "api");
    auto& svc = voice::VoiceService::instance();
    if (std::strcmp(key_name, "elevenlabs_api_key") == 0) {
        svc.tts_config().elevenlabs_key = value;
        show_status("ElevenLabs API key saved");
    } else {
        svc.tts_config().openai_key = value;
        svc.stt_config().openai_key = value;
        show_status("OpenAI API key saved");
    }
}

void VoiceSection::save_auto_voice() {
    db::ops::save_setting("voice_auto_start", auto_voice_mode_ ? "true" : "false", "voice");
    show_status(auto_voice_mode_ ? "Auto voice mode enabled" : "Auto voice mode disabled");
}

void VoiceSection::save_enabled() {
    db::ops::save_setting("voice_enabled", voice_enabled_ ? "true" : "false", "voice");
    show_status(voice_enabled_ ? "Voice enabled" : "Voice disabled");
}

void VoiceSection::test_voice() {
    auto& svc = voice::VoiceService::instance();
    svc.speak("Hello, I am your Fincept AI assistant. How can I help you today?");
}

void VoiceSection::show_status(const char* msg, bool is_error) {
    status_msg_ = msg;
    status_is_error_ = is_error;
    status_expire_ = ImGui::GetTime() + 3.0;
}

// ============================================================================
// Main render — mirrors ChatBubbleSection.tsx return() layout
// ============================================================================
void VoiceSection::render() {
    init();
    using namespace theme::colors;

    // Header
    ImGui::TextColored(ACCENT, "AI CHAT BUBBLE");
    ImGui::TextColored(TEXT_DIM, "Configure the floating AI chat assistant bubble and voice settings.");
    ImGui::Separator();
    ImGui::Spacing();

    float panel_w = ImGui::GetContentRegionAvail().x;

    // Status message
    if (!status_msg_.empty() && ImGui::GetTime() < status_expire_) {
        ImVec4 color = status_is_error_ ? ERROR_RED : SUCCESS;
        ImGui::TextColored(color, "%s", status_msg_.c_str());
        ImGui::Spacing();
    }

    render_enable_toggle();
    render_tts_panel();
    render_api_keys_panel();
    render_auto_voice_toggle();
    render_info_card();
}

// ============================================================================
// Panel: Enable/Disable Toggle (mirrors Tauri ENABLE CHAT BUBBLE section)
// ============================================================================
void VoiceSection::render_enable_toggle() {
    using namespace theme::colors;
    float panel_w = ImGui::GetContentRegionAvail().x;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##voice_enable", ImVec2(panel_w, 80), ImGuiChildFlags_Borders);

    ImGui::SetCursorPos(ImVec2(12, 10));
    ImGui::TextColored(TEXT_PRIMARY, "ENABLE VOICE");
    ImGui::SetCursorPos(ImVec2(12, 28));
    ImGui::TextColored(TEXT_DIM, "Enable voice input/output in AI chat");

    // Toggle button on the right
    float toggle_x = panel_w - 70;
    ImGui::SetCursorPos(ImVec2(toggle_x, 12));
    ImVec4 toggle_color = voice_enabled_ ? SUCCESS : TEXT_DIM;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(toggle_color.x, toggle_color.y, toggle_color.z, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(toggle_color.x, toggle_color.y, toggle_color.z, 0.3f));
    ImGui::PushStyleColor(ImGuiCol_Text, toggle_color);
    if (ImGui::Button(voice_enabled_ ? "ON ##ve" : "OFF##ve", ImVec2(50, 24))) {
        voice_enabled_ = !voice_enabled_;
        save_enabled();
    }
    ImGui::PopStyleColor(3);

    // Status indicator badge
    ImGui::SetCursorPos(ImVec2(12, 52));
    ImVec4 badge_color = voice_enabled_ ? SUCCESS : TEXT_DIM;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(badge_color.x, badge_color.y, badge_color.z, 0.08f));
    ImGui::PushStyleColor(ImGuiCol_Border, badge_color);
    ImGui::BeginChild("##ve_badge", ImVec2(panel_w - 24, 20), ImGuiChildFlags_Borders);
    // Dot
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddCircleFilled(
        ImVec2(p.x + 10, p.y + 10), 3.0f,
        ImGui::ColorConvertFloat4ToU32(badge_color));
    ImGui::SetCursorPosX(22);
    ImGui::TextColored(badge_color, voice_enabled_ ? "ACTIVE" : "DISABLED");
    ImGui::EndChild();
    ImGui::PopStyleColor(2);

    ImGui::EndChild();
    ImGui::PopStyleColor(); // ChildBg
    ImGui::Spacing();
}

// ============================================================================
// Panel: TTS Settings (provider grid, voice list, speed, test)
// ============================================================================
void VoiceSection::render_tts_panel() {
    using namespace theme::colors;
    float panel_w = ImGui::GetContentRegionAvail().x;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##voice_tts", ImVec2(panel_w, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);

    // Header
    ImGui::SetCursorPos(ImVec2(12, 8));
    ImGui::TextColored(ACCENT, "TEXT-TO-SPEECH");

    ImGui::SetCursorPosX(12);
    ImGui::Spacing();

    // ── TTS Provider Selection (2x2 grid like Tauri) ──
    ImGui::SetCursorPosX(12);
    ImGui::TextColored(TEXT_DIM, "TTS PROVIDER");
    ImGui::Spacing();

    auto& providers = voice::tts_provider_info();
    float grid_w = (panel_w - 36) / 2.0f;

    for (int i = 0; i < (int)providers.size(); i++) {
        bool is_selected = (selected_provider_ == i);

        // 2-column grid: new line every 2 items
        if (i % 2 == 0) {
            ImGui::SetCursorPosX(12);
        } else {
            ImGui::SameLine(0, 6);
        }

        ImGui::PushID(i);

        ImVec4 btn_bg = is_selected ? ImVec4(ACCENT.x, ACCENT.y, ACCENT.z, 0.08f) : BG_DARKEST;
        ImVec4 btn_border = is_selected ? ACCENT : BORDER_DIM;

        ImGui::PushStyleColor(ImGuiCol_Button, btn_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, is_selected ? btn_bg : BG_HOVER);
        ImGui::PushStyleColor(ImGuiCol_Border, btn_border);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        if (ImGui::Button("##prov", ImVec2(grid_w, 48))) {
            if (!is_selected) {
                selected_provider_ = i;
                save_provider();
            }
        }

        // Draw text on top of button
        ImVec2 btn_min = ImGui::GetItemRectMin();
        ImVec4 name_col = is_selected ? ACCENT : TEXT_PRIMARY;
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(btn_min.x + 10, btn_min.y + 6),
            ImGui::ColorConvertFloat4ToU32(name_col),
            providers[i].name);
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(btn_min.x + 10, btn_min.y + 22),
            ImGui::ColorConvertFloat4ToU32(TEXT_DIM),
            providers[i].description);

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::SetCursorPosX(12);
    ImGui::Separator();
    ImGui::Spacing();

    // ── Voice Selection (scrollable list like Tauri) ──
    {
        const char* provider_labels[] = {"SYSTEM VOICES", "ELEVENLABS VOICES", "OPENAI VOICES"};
        int label_idx = selected_provider_;
        if (label_idx < 0 || label_idx > 2) label_idx = 0;

        ImGui::SetCursorPosX(12);
        if (selected_provider_ == 0) {
            char vlabel[64];
            std::snprintf(vlabel, sizeof(vlabel), "SYSTEM VOICES (%d)", (int)current_voices_.size());
            ImGui::TextColored(TEXT_DIM, "%s", vlabel);
        } else {
            ImGui::TextColored(TEXT_DIM, "%s", provider_labels[label_idx]);
        }
        ImGui::Spacing();

        if (current_voices_.empty()) {
            ImGui::SetCursorPosX(12);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
            ImGui::BeginChild("##no_voices", ImVec2(panel_w - 24, 36), ImGuiChildFlags_Borders);
            ImGui::SetCursorPos(ImVec2(8, 10));
            ImGui::TextColored(TEXT_DIM, "No voices available for this provider.");
            ImGui::EndChild();
            ImGui::PopStyleColor();
        } else {
            // Scrollable voice list (max 160px like Tauri)
            ImGui::SetCursorPosX(12);
            ImGui::BeginChild("##voice_list", ImVec2(panel_w - 24, 160));

            for (int i = 0; i < (int)current_voices_.size(); i++) {
                bool is_sel = (selected_voice_ == i);
                auto& v = current_voices_[i];

                ImGui::PushID(i + 100);

                ImVec4 item_bg = is_sel ? ImVec4(ACCENT.x, ACCENT.y, ACCENT.z, 0.08f) : BG_DARKEST;
                ImVec4 item_border = is_sel ? ACCENT : BORDER_DIM;

                ImGui::PushStyleColor(ImGuiCol_Button, item_bg);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, is_sel ? item_bg : BG_HOVER);
                ImGui::PushStyleColor(ImGuiCol_Border, item_border);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

                float voice_w = ImGui::GetContentRegionAvail().x;
                if (ImGui::Button("##voice", ImVec2(voice_w, 36))) {
                    selected_voice_ = i;
                    save_voice();
                }

                // Draw voice info on button
                ImVec2 bmin = ImGui::GetItemRectMin();
                ImVec4 vcol = is_sel ? ACCENT : TEXT_PRIMARY;
                ImGui::GetWindowDrawList()->AddText(
                    ImVec2(bmin.x + 10, bmin.y + 4),
                    ImGui::ColorConvertFloat4ToU32(vcol),
                    v.name.c_str());

                char sub[128];
                std::snprintf(sub, sizeof(sub), "%s %s %s",
                              v.language.c_str(),
                              v.gender.empty() ? "" : "|",
                              v.gender.c_str());
                ImGui::GetWindowDrawList()->AddText(
                    ImVec2(bmin.x + 10, bmin.y + 20),
                    ImGui::ColorConvertFloat4ToU32(TEXT_DIM),
                    sub);

                // Orange dot indicator for selected (like Tauri)
                if (is_sel) {
                    ImVec2 bmax = ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddCircleFilled(
                        ImVec2(bmax.x - 14, (bmin.y + bmax.y) / 2),
                        3.0f, ImGui::ColorConvertFloat4ToU32(ACCENT));
                }

                ImGui::PopStyleVar();
                ImGui::PopStyleColor(3);
                ImGui::PopID();
            }

            ImGui::EndChild();
        }
    }

    ImGui::Spacing();
    ImGui::SetCursorPosX(12);
    ImGui::Separator();
    ImGui::Spacing();

    // ── Speech Speed Slider (matches Tauri layout) ──
    {
        ImGui::SetCursorPosX(12);
        ImGui::TextColored(TEXT_DIM, "SPEECH SPEED");
        ImGui::SameLine(panel_w - 60);
        ImGui::TextColored(ACCENT, "%.1fx", speech_rate_);

        ImGui::SetCursorPosX(12);
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ACCENT);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ACCENT_HOVER);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_DARKEST);
        ImGui::SetNextItemWidth(panel_w - 24);
        if (ImGui::SliderFloat("##speech_rate", &speech_rate_, 0.5f, 2.0f, "")) {
            save_rate();
        }
        ImGui::PopStyleColor(3);

        // Labels: 0.5x (Slow) | 1.0x | 2.0x (Fast) — like Tauri
        ImGui::SetCursorPosX(12);
        ImGui::TextColored(TEXT_DIM, "0.5x (Slow)");
        ImGui::SameLine((panel_w - 24) / 2);
        ImGui::TextColored(TEXT_DIM, "1.0x");
        ImGui::SameLine(panel_w - 80);
        ImGui::TextColored(TEXT_DIM, "2.0x (Fast)");
    }

    ImGui::Spacing();

    // ── Test Voice Button (full width like Tauri) ──
    {
        ImGui::SetCursorPosX(12);
        bool can_test = !current_voices_.empty() && selected_voice_ >= 0;

        if (can_test) {
            ImGui::PushStyleColor(ImGuiCol_Button, BG_DARKEST);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
            ImGui::PushStyleColor(ImGuiCol_Border, BORDER_DIM);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_WIDGET);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DISABLED);
            ImGui::PushStyleColor(ImGuiCol_Border, BORDER_DIM);
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        if (ImGui::Button("TEST VOICE##test_btn", ImVec2(panel_w - 24, 32)) && can_test) {
            test_voice();
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);

        // Show speaking status
        if (voice::VoiceService::instance().is_speaking()) {
            ImGui::SameLine();
            ImGui::TextColored(ACCENT, "SPEAKING...");
        }
    }

    ImGui::Spacing();
    ImGui::EndChild();
    ImGui::PopStyleColor(); // ChildBg
    ImGui::Spacing();
}

// ============================================================================
// Panel: API Keys (collapsible, shown only for ElevenLabs/OpenAI)
// ============================================================================
void VoiceSection::render_api_keys_panel() {
    using namespace theme::colors;
    float panel_w = ImGui::GetContentRegionAvail().x;

    // Only show for providers that need API keys (like Tauri conditional render)
    auto provider = static_cast<voice::TTSProvider>(selected_provider_);
    if (provider != voice::TTSProvider::ElevenLabs &&
        provider != voice::TTSProvider::OpenAI) {
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##voice_apikeys", ImVec2(panel_w, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);

    // Collapsible header (like Tauri ChevronDown/Up toggle)
    ImGui::SetCursorPos(ImVec2(12, 8));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));

    char header_label[64];
    std::snprintf(header_label, sizeof(header_label), "API KEYS  %s##ak_toggle",
                  api_keys_expanded_ ? "[-]" : "[+]");
    ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
    if (ImGui::Button(header_label, ImVec2(panel_w - 24, 20))) {
        api_keys_expanded_ = !api_keys_expanded_;
    }
    ImGui::PopStyleColor(3);

    if (api_keys_expanded_) {
        ImGui::Spacing();

        // ElevenLabs API Key
        if (provider == voice::TTSProvider::ElevenLabs) {
            ImGui::SetCursorPosX(12);
            ImGui::TextColored(TEXT_DIM, "ELEVENLABS API KEY");
            ImGui::SetCursorPosX(12);

            float input_w = panel_w - 130;
            ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_DARKEST);
            ImGui::SetNextItemWidth(input_w);
            ImGuiInputTextFlags flags = show_elevenlabs_key_ ? 0 : ImGuiInputTextFlags_Password;
            ImGui::InputText("##el_key", elevenlabs_key_buf_, sizeof(elevenlabs_key_buf_), flags);
            ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
            if (ImGui::SmallButton(show_elevenlabs_key_ ? "HIDE##el" : "SHOW##el")) {
                show_elevenlabs_key_ = !show_elevenlabs_key_;
            }
            ImGui::PopStyleColor(2);

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
            ImGui::PushStyleColor(ImGuiCol_Text, BG_DARKEST);
            if (ImGui::SmallButton("SAVE##el_save")) {
                save_api_key("elevenlabs_api_key", elevenlabs_key_buf_);
            }
            ImGui::PopStyleColor(2);

            ImGui::SetCursorPosX(12);
            ImGui::TextColored(TEXT_DIM, "Get key from elevenlabs.io");
            ImGui::Spacing();
        }

        // OpenAI API Key
        if (provider == voice::TTSProvider::OpenAI) {
            ImGui::SetCursorPosX(12);
            ImGui::TextColored(TEXT_DIM, "OPENAI API KEY");
            ImGui::SetCursorPosX(12);

            float input_w = panel_w - 130;
            ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_DARKEST);
            ImGui::SetNextItemWidth(input_w);
            ImGuiInputTextFlags flags = show_openai_key_ ? 0 : ImGuiInputTextFlags_Password;
            ImGui::InputText("##oai_key", openai_key_buf_, sizeof(openai_key_buf_), flags);
            ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
            if (ImGui::SmallButton(show_openai_key_ ? "HIDE##oai" : "SHOW##oai")) {
                show_openai_key_ = !show_openai_key_;
            }
            ImGui::PopStyleColor(2);

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
            ImGui::PushStyleColor(ImGuiCol_Text, BG_DARKEST);
            if (ImGui::SmallButton("SAVE##oai_save")) {
                save_api_key("openai_tts_api_key", openai_key_buf_);
            }
            ImGui::PopStyleColor(2);

            ImGui::SetCursorPosX(12);
            ImGui::TextColored(TEXT_DIM, "Get key from platform.openai.com/api-keys");
            ImGui::Spacing();
        }
    }

    ImGui::Spacing();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

// ============================================================================
// Panel: Auto-Start Voice Mode (mirrors Tauri toggle panel)
// ============================================================================
void VoiceSection::render_auto_voice_toggle() {
    using namespace theme::colors;
    float panel_w = ImGui::GetContentRegionAvail().x;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##voice_autostart", ImVec2(panel_w, 64), ImGuiChildFlags_Borders);

    ImGui::SetCursorPos(ImVec2(12, 10));
    ImGui::TextColored(ACCENT, "AUTO-START VOICE MODE");
    ImGui::SetCursorPos(ImVec2(12, 28));
    ImGui::TextColored(TEXT_DIM, "Automatically enable voice mode when opening chat");

    // Toggle on the right
    float toggle_x = panel_w - 70;
    ImGui::SetCursorPos(ImVec2(toggle_x, 14));
    ImVec4 toggle_color = auto_voice_mode_ ? SUCCESS : TEXT_DIM;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(toggle_color.x, toggle_color.y, toggle_color.z, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(toggle_color.x, toggle_color.y, toggle_color.z, 0.3f));
    ImGui::PushStyleColor(ImGuiCol_Text, toggle_color);
    if (ImGui::Button(auto_voice_mode_ ? "ON ##av" : "OFF##av", ImVec2(50, 24))) {
        auto_voice_mode_ = !auto_voice_mode_;
        save_auto_voice();
    }
    ImGui::PopStyleColor(3);

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

// ============================================================================
// Panel: Info Card (orange-tinted, bullet list — mirrors Tauri info card)
// ============================================================================
void VoiceSection::render_info_card() {
    using namespace theme::colors;
    float panel_w = ImGui::GetContentRegionAvail().x;

    // Orange tinted background like Tauri: FINCEPT.ORANGE + 10% opacity
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(ACCENT.x, ACCENT.y, ACCENT.z, 0.06f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ACCENT.x, ACCENT.y, ACCENT.z, 0.25f));
    ImGui::BeginChild("##voice_info_card", ImVec2(panel_w, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);

    ImGui::SetCursorPos(ImVec2(12, 8));
    ImGui::TextColored(ACCENT, "VOICE MODE FEATURES");
    ImGui::Spacing();

    ImGui::SetCursorPosX(24);
    ImGui::TextColored(TEXT_SECONDARY, "- Hands-free conversation with AI assistant");
    ImGui::SetCursorPosX(24);
    ImGui::TextColored(TEXT_SECONDARY, "- 3 TTS providers: Native, ElevenLabs, OpenAI");
    ImGui::SetCursorPosX(24);
    ImGui::TextColored(TEXT_SECONDARY, "- Speech recognition via OpenAI Whisper");
    ImGui::SetCursorPosX(24);
    ImGui::TextColored(TEXT_SECONDARY, "- ElevenLabs/OpenAI: Premium AI voices (API key required)");
    ImGui::SetCursorPosX(24);
    ImGui::TextColored(TEXT_SECONDARY, "- Works even when chat is in background");
    ImGui::SetCursorPosX(24);
    ImGui::TextColored(TEXT_SECONDARY, "- Click VOICE button in AI Chat to activate");

    ImGui::Spacing();
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}

} // namespace fincept::settings
