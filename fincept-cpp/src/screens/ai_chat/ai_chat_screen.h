#pragma once
// AI Chat Screen — Multi-provider LLM chat with session management
// Layout: [Sessions Sidebar] [Chat Area] [Info Sidebar]
// Mirrors ChatTab.tsx from the desktop app

#include "llm_service.h"
#include "storage/database.h"
#include "voice/voice_service.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <future>
#include <chrono>

namespace fincept::ai_chat {

// Quick prompt shortcut
struct QuickPrompt {
    std::string label;
    std::string prompt;
};

class AIChatScreen {
public:
    void render();

private:
    bool initialized_ = false;

    // ── Sessions ──
    std::vector<db::ChatSession> sessions_;
    std::string current_session_uuid_;
    int selected_session_idx_ = -1;

    // Session search
    char search_buf_[128] = {};

    // Session rename
    std::string renaming_session_uuid_;
    char rename_buf_[256] = {};

    // ── Messages ──
    std::vector<db::ChatMessage> messages_;

    // ── Input ──
    char input_buf_[4096] = {};
    bool scroll_to_bottom_ = false;

    // ── Streaming ──
    std::mutex stream_mutex_;
    std::string streaming_content_;
    std::atomic<bool> is_streaming_{false};
    std::future<LLMResponse> streaming_future_;

    // ── LLM Config (cached) ──
    db::LLMConfig active_config_;
    db::LLMGlobalSettings global_settings_;
    std::vector<db::LLMConfig> all_configs_;

    // ── Statistics ──
    int total_sessions_ = 0;
    int total_messages_ = 0;
    int total_tokens_ = 0;

    // ── Quick Prompts ──
    std::vector<QuickPrompt> quick_prompts_;

    // ── Voice ──
    bool voice_initialized_ = false;
    bool voice_mode_ = false;

    // ── Status ──
    std::string status_ = "INITIALIZING...";
    std::string error_;
    double error_time_ = 0;

    // ── Timing ──
    std::chrono::steady_clock::time_point last_config_reload_;

    // ── Init ──
    void init();
    void load_sessions();
    void load_messages(const std::string& session_uuid);
    void load_config();
    void load_statistics();

    // ── Session operations ──
    void create_new_session();
    void delete_session(const std::string& uuid);
    void delete_all_sessions();
    void select_session(int idx);
    void start_rename(const std::string& uuid, const std::string& title);
    void save_rename();
    void cancel_rename();
    void clear_current_chat();

    // ── Message sending ──
    void send_message();
    void check_streaming_result();

    // ── Smart title ──
    static std::string generate_smart_title(const std::string& message);

    // ── Render sections ──
    void render_navbar(float width);
    void render_sessions_panel(float width, float height);
    void render_chat_panel(float width, float height);
    void render_info_panel(float width, float height);
    void render_status_bar(float width);

    // Chat sub-renders
    void render_welcome_screen(float width, float height);
    void render_message(const db::ChatMessage& msg, float width);
    void render_streaming_message(float width);
    void render_input_area(float width);

    // Provider switching
    void switch_provider(const std::string& provider);
};

} // namespace fincept::ai_chat
