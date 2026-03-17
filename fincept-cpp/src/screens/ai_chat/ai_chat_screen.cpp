#include "ai_chat_screen.h"
#include "ui/theme.h"
#include "ui/yoga_helpers.h"
#include "core/logger.h"
#include "mcp/mcp_service.h"
#include <imgui.h>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <cctype>

namespace fincept::ai_chat {

// ============================================================================
// Default quick prompts (matches TypeScript DEFAULT_QUICK_PROMPTS)
// ============================================================================
static const std::vector<QuickPrompt> DEFAULT_QUICK_PROMPTS = {
    {"MARKET TRENDS",  "Analyze current market trends and key insights"},
    {"PORTFOLIO",      "Portfolio diversification recommendations"},
    {"RISK",           "Investment risk assessment strategies"},
    {"TECHNICAL",      "Key technical analysis indicators"},
};

// ============================================================================
// Init
// ============================================================================
void AIChatScreen::init() {
    if (initialized_) return;

    quick_prompts_ = DEFAULT_QUICK_PROMPTS;
    load_config();
    load_sessions();
    load_statistics();

    // Initialize voice service
    if (!voice_initialized_) {
        auto& vs = voice::VoiceService::instance();
        vs.init();

        // When transcription is ready, put it in input and send
        vs.set_transcript_callback([this](const std::string& text) {
            std::strncpy(input_buf_, text.c_str(), sizeof(input_buf_) - 1);
            input_buf_[sizeof(input_buf_) - 1] = '\0';
            // Auto-send if in voice mode
            if (voice_mode_) {
                send_message();
            }
        });

        voice_initialized_ = true;
    }

    status_ = "READY";
    last_config_reload_ = std::chrono::steady_clock::now();
    initialized_ = true;
}

void AIChatScreen::load_config() {
    all_configs_ = db::ops::get_llm_configs();
    global_settings_ = db::ops::get_llm_global_settings();

    active_config_ = {};
    for (auto& c : all_configs_) {
        if (c.is_active) {
            active_config_ = c;
            break;
        }
    }
    if (active_config_.provider.empty() && !all_configs_.empty()) {
        active_config_ = all_configs_[0];
    }

    LLMService::instance().reload_config();
}

void AIChatScreen::load_sessions() {
    sessions_ = db::ops::get_chat_sessions();
}

void AIChatScreen::load_messages(const std::string& session_uuid) {
    messages_ = db::ops::get_chat_messages(session_uuid);
    scroll_to_bottom_ = true;
}

void AIChatScreen::load_statistics() {
    total_sessions_ = (int)sessions_.size();
    total_messages_ = 0;
    total_tokens_ = 0;
    // Quick approximation — count from loaded sessions
    for (auto& s : sessions_) {
        total_messages_ += (int)s.message_count;
    }
}

// ============================================================================
// Session operations
// ============================================================================
void AIChatScreen::create_new_session() {
    std::string title = "New Chat";
    if (std::strlen(input_buf_) > 0) {
        title = generate_smart_title(input_buf_);
    }

    auto session = db::ops::create_chat_session(title);
    current_session_uuid_ = session.session_uuid;
    messages_.clear();
    load_sessions();
    load_statistics();

    // Select the new session
    for (int i = 0; i < (int)sessions_.size(); i++) {
        if (sessions_[i].session_uuid == current_session_uuid_) {
            selected_session_idx_ = i;
            break;
        }
    }

    status_ = "NEW SESSION CREATED";
}

void AIChatScreen::delete_session(const std::string& uuid) {
    db::ops::delete_chat_session(uuid);
    if (current_session_uuid_ == uuid) {
        current_session_uuid_.clear();
        messages_.clear();
        selected_session_idx_ = -1;
    }
    load_sessions();
    load_statistics();
    status_ = "SESSION DELETED";
}

void AIChatScreen::delete_all_sessions() {
    for (auto& s : sessions_) {
        db::ops::delete_chat_session(s.session_uuid);
    }
    sessions_.clear();
    messages_.clear();
    current_session_uuid_.clear();
    selected_session_idx_ = -1;
    load_statistics();
    status_ = "ALL SESSIONS DELETED";
}

void AIChatScreen::select_session(int idx) {
    if (idx < 0 || idx >= (int)sessions_.size()) return;
    selected_session_idx_ = idx;
    current_session_uuid_ = sessions_[idx].session_uuid;
    load_messages(current_session_uuid_);
    status_ = "READY";
}

void AIChatScreen::start_rename(const std::string& uuid, const std::string& title) {
    renaming_session_uuid_ = uuid;
    std::strncpy(rename_buf_, title.c_str(), sizeof(rename_buf_) - 1);
    rename_buf_[sizeof(rename_buf_) - 1] = '\0';
}

void AIChatScreen::save_rename() {
    std::string new_title(rename_buf_);
    if (!new_title.empty() && !renaming_session_uuid_.empty()) {
        db::ops::update_chat_session_title(renaming_session_uuid_, new_title);
        load_sessions();
    }
    renaming_session_uuid_.clear();
}

void AIChatScreen::cancel_rename() {
    renaming_session_uuid_.clear();
}

void AIChatScreen::clear_current_chat() {
    if (current_session_uuid_.empty()) return;
    db::ops::clear_chat_session_messages(current_session_uuid_);
    messages_.clear();
    load_sessions();
    load_statistics();
    status_ = "CHAT CLEARED";
}

void AIChatScreen::switch_provider(const std::string& provider) {
    db::ops::set_active_llm_provider(provider);
    load_config();
    status_ = "READY";
}

// ============================================================================
// Smart title generation (mirrors TypeScript generateSmartTitle)
// ============================================================================
std::string AIChatScreen::generate_smart_title(const std::string& message) {
    if (message.empty()) return "New Chat";

    std::string lower = message;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Financial keywords
    static const char* keywords[] = {
        "market", "stock", "portfolio", "risk", "analysis",
        "trend", "trading", "investment", "price", "strategy",
        "forecast", "data", "chart", "indicator", "technical"
    };
    for (auto* kw : keywords) {
        if (lower.find(kw) != std::string::npos) {
            std::string title(kw);
            title[0] = (char)std::toupper(title[0]);
            return title + " Chat";
        }
    }

    // Question words
    if (lower.rfind("what", 0) == 0)      return "What Query";
    if (lower.rfind("how", 0) == 0)       return "How To";
    if (lower.rfind("why", 0) == 0)       return "Why Question";
    if (lower.rfind("analyze", 0) == 0)   return "Analysis";
    if (lower.rfind("explain", 0) == 0)   return "Explanation";
    if (lower.rfind("compare", 0) == 0)   return "Comparison";
    if (lower.rfind("calculate", 0) == 0) return "Calculation";

    // First 2 meaningful words (max 20 chars)
    std::string result;
    int word_count = 0;
    std::istringstream iss(message);
    std::string word;
    while (iss >> word && word_count < 2) {
        if (word.size() <= 2) continue;
        if (!result.empty()) result += " ";
        result += word;
        word_count++;
    }
    if (result.size() > 20) result = result.substr(0, 20);
    return result.empty() ? "New Chat" : result;
}

// ============================================================================
// Message sending
// ============================================================================
void AIChatScreen::send_message() {
    std::string content(input_buf_);
    // Trim
    auto start = content.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return;
    content = content.substr(start);
    auto end = content.find_last_not_of(" \t\n\r");
    if (end != std::string::npos) content = content.substr(0, end + 1);
    if (content.empty()) return;

    // Ensure we have a provider
    if (active_config_.provider.empty()) {
        error_ = "No LLM provider configured. Go to Settings > LLM Configuration.";
        error_time_ = ImGui::GetTime();
        return;
    }

    // Check API key
    if (provider_requires_api_key(active_config_.provider)) {
        if (!active_config_.api_key || active_config_.api_key->empty()) {
            error_ = active_config_.provider + " requires an API key. Configure in Settings.";
            error_time_ = ImGui::GetTime();
            return;
        }
    }

    // Ensure session
    if (current_session_uuid_.empty()) {
        create_new_session();
    }

    // Save user message
    db::ChatMessage user_msg;
    user_msg.session_uuid = current_session_uuid_;
    user_msg.role = "user";
    user_msg.content = content;
    user_msg = db::ops::add_chat_message(user_msg);
    messages_.push_back(user_msg);

    // Clear input
    std::memset(input_buf_, 0, sizeof(input_buf_));
    scroll_to_bottom_ = true;

    // Build conversation history
    std::vector<ConversationMessage> history;
    for (auto& m : messages_) {
        if (m.role == "user" || m.role == "assistant") {
            history.push_back({m.role, m.content});
        }
    }
    // Remove last (we pass it separately)
    if (!history.empty()) history.pop_back();

    // Status
    std::string prov_upper = active_config_.provider;
    std::transform(prov_upper.begin(), prov_upper.end(), prov_upper.begin(), ::toupper);
    status_ = "CALLING " + prov_upper + " API...";

    // Determine if we should use tool-calling mode (non-streaming)
    // Tool calling requires full responses to detect tool_calls in the JSON
    bool use_tools = mcp::MCPService::instance().tool_count() > 0 &&
                     (active_config_.provider == "openai" ||
                      active_config_.provider == "groq" ||
                      active_config_.provider == "deepseek" ||
                      active_config_.provider == "openrouter" ||
                      active_config_.provider == "fincept");

    // Start streaming/processing
    is_streaming_ = true;
    {
        std::lock_guard<std::mutex> lock(stream_mutex_);
        streaming_content_.clear();
    }

    if (use_tools) {
        // Non-streaming mode with tool calling support
        streaming_future_ = std::async(std::launch::async, [this, content, history]() {
            return LLMService::instance().chat(content, history);
        });
    } else {
        // Streaming mode (no tool calling)
        streaming_future_ = LLMService::instance().chat_streaming(
            content, history,
            [this](const std::string& chunk, bool done) {
                if (!done) {
                    std::lock_guard<std::mutex> lock(stream_mutex_);
                    streaming_content_ += chunk;
                }
            }
        );
    }
}

void AIChatScreen::check_streaming_result() {
    if (!is_streaming_) return;
    if (!streaming_future_.valid()) return;

    // Check if future is ready (non-blocking)
    auto status = streaming_future_.wait_for(std::chrono::milliseconds(0));
    if (status != std::future_status::ready) return;

    // Get result
    auto resp = streaming_future_.get();
    is_streaming_ = false;

    std::string final_content;
    {
        std::lock_guard<std::mutex> lock(stream_mutex_);
        final_content = streaming_content_;
        streaming_content_.clear();
    }

    // Use streamed content if response content is empty
    if (resp.content.empty() && !final_content.empty()) {
        resp.content = final_content;
    }

    if (resp.success && !resp.content.empty()) {
        // Save assistant message
        db::ChatMessage ai_msg;
        ai_msg.session_uuid = current_session_uuid_;
        ai_msg.role = "assistant";
        ai_msg.content = resp.content;
        ai_msg.provider = active_config_.provider;
        ai_msg.model = active_config_.model;
        if (resp.total_tokens > 0) ai_msg.tokens_used = resp.total_tokens;
        ai_msg = db::ops::add_chat_message(ai_msg);
        messages_.push_back(ai_msg);
        total_tokens_ += resp.total_tokens;
        scroll_to_bottom_ = true;
        status_ = "READY";

        // Voice mode: speak the AI response
        if (voice_mode_) {
            voice::VoiceService::instance().speak(resp.content);
        }
    } else if (!resp.error.empty()) {
        // Save error as assistant message
        db::ChatMessage err_msg;
        err_msg.session_uuid = current_session_uuid_;
        err_msg.role = "assistant";
        err_msg.content = "[ERROR] " + resp.error + "\n\nPlease check your LLM configuration.";
        err_msg = db::ops::add_chat_message(err_msg);
        messages_.push_back(err_msg);
        scroll_to_bottom_ = true;
        status_ = "ERROR: " + resp.error;
    } else {
        status_ = "READY";
    }

    load_sessions();
    load_statistics();
}

// ============================================================================
// Main render
// ============================================================================
void AIChatScreen::render() {
    init();
    check_streaming_result();

    // Periodically reload config (every 5 seconds)
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_config_reload_).count() > 5) {
        load_config();
        last_config_reload_ = now;
    }

    using namespace theme::colors;

    // ScreenFrame gives us full-screen layout like dashboard
    ui::ScreenFrame frame("##ai_chat", ImVec2(0, 0), BG_DARKEST);
    if (!frame.begin()) { frame.end(); return; }

    float total_w = frame.width();
    float total_h = frame.height();

    // Layout: navbar (top) + main (3 panels) + statusbar (bottom)
    float navbar_h = 36.0f;
    float statusbar_h = 24.0f;
    float main_h = total_h - navbar_h - statusbar_h;

    float sessions_w = 260.0f;
    float info_w = 280.0f;
    float chat_w = total_w - sessions_w - info_w;
    if (chat_w < 300) {
        // Collapse sidebars on narrow windows
        sessions_w = 0;
        info_w = 0;
        chat_w = total_w;
    }

    // Navbar
    render_navbar(total_w);

    // Main content area
    if (sessions_w > 0) {
        render_sessions_panel(sessions_w, main_h);
        ImGui::SameLine(0, 0);
    }

    render_chat_panel(chat_w, main_h);

    if (info_w > 0) {
        ImGui::SameLine(0, 0);
        render_info_panel(info_w, main_h);
    }

    // Status bar
    render_status_bar(total_w);

    frame.end();
}

// ============================================================================
// Navbar
// ============================================================================
void AIChatScreen::render_navbar(float width) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##chat_navbar", ImVec2(width, 36), ImGuiChildFlags_None);

    float pad = 8.0f;
    ImGui::SetCursorPos(ImVec2(pad, 6));

    // Title
    ImGui::TextColored(ACCENT, "AI CHAT");
    ImGui::SameLine();
    ImGui::TextColored(BORDER, "|");
    ImGui::SameLine();

    // Provider selector button
    std::string prov_label = active_config_.provider.empty() ? "NO PROVIDER" : active_config_.provider;
    std::transform(prov_label.begin(), prov_label.end(), prov_label.begin(), ::toupper);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(WARNING.x, WARNING.y, WARNING.z, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(WARNING.x, WARNING.y, WARNING.z, 0.3f));
    ImGui::PushStyleColor(ImGuiCol_Text, WARNING);
    if (ImGui::SmallButton((prov_label + " v##prov_btn").c_str())) {
        ImGui::OpenPopup("##prov_popup");
    }
    ImGui::PopStyleColor(3);

    // Provider dropdown popup
    ImGui::PushStyleColor(ImGuiCol_PopupBg, BG_PANEL);
    ImGui::PushStyleColor(ImGuiCol_Border, ACCENT);
    ImGui::SetNextWindowSizeConstraints(ImVec2(220, 0), ImVec2(220, 400));
    if (ImGui::BeginPopup("##prov_popup")) {
        ImGui::TextColored(TEXT_DIM, "SELECT PROVIDER");
        ImGui::Separator();
        std::string pending_provider;
        for (auto& cfg : all_configs_) {
            bool is_active = (cfg.provider == active_config_.provider);
            std::string label = cfg.provider;
            std::transform(label.begin(), label.end(), label.begin(), ::toupper);
            if (is_active) label += "  [ACTIVE]";

            if (ImGui::Selectable(label.c_str(), is_active)) {
                pending_provider = cfg.provider;
                ImGui::CloseCurrentPopup();
            }
            ImGui::TextColored(TEXT_DIM, "  Model: %s", cfg.model.c_str());
        }
        if (!pending_provider.empty()) {
            switch_provider(pending_provider);
        }
        if (all_configs_.empty()) {
            ImGui::TextColored(TEXT_DIM, "No providers configured.");
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    ImGui::TextColored(BORDER, "|");
    ImGui::SameLine();

    // Clock
    auto t = std::time(nullptr);
    auto tm = std::localtime(&t);
    char clock_buf[16];
    std::snprintf(clock_buf, sizeof(clock_buf), "%02d:%02d:%02d",
                  tm->tm_hour, tm->tm_min, tm->tm_sec);
    ImGui::TextColored(TEXT_SECONDARY, "%s", clock_buf);

    // Right side buttons
    float right_x = width - 320;
    ImGui::SameLine(right_x);

    // Voice mode toggle
    {
        auto& vs = voice::VoiceService::instance();
        bool vm = voice_mode_;
        ImVec4 voice_color = vm ? ACCENT : TEXT_DIM;
        ImVec4 voice_bg = vm ? ACCENT_BG : ImVec4(0, 0, 0, 0);

        ImGui::PushStyleColor(ImGuiCol_Button, voice_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, vm ? ACCENT_BG : BG_HOVER);
        ImGui::PushStyleColor(ImGuiCol_Text, voice_color);
        if (ImGui::SmallButton(vm ? "VOICE [ON]" : "VOICE")) {
            voice_mode_ = !voice_mode_;
            if (voice_mode_) {
                vs.enable_voice_mode();
            } else {
                vs.disable_voice_mode();
            }
        }
        ImGui::PopStyleColor(3);

        // Voice state indicator
        if (vs.state() != voice::VoiceState::Idle) {
            ImGui::SameLine();
            ImVec4 state_color = SUCCESS;
            if (vs.state() == voice::VoiceState::Speaking) state_color = INFO;
            if (vs.state() == voice::VoiceState::Processing) state_color = WARNING;
            if (vs.state() == voice::VoiceState::Error) state_color = ERROR_RED;
            ImGui::TextColored(state_color, "%s", voice::voice_state_to_string(vs.state()));
        }
    }

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
    ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
    if (ImGui::SmallButton("SETTINGS##chat")) {
        // Could navigate to settings tab — for now just note it
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if (theme::AccentButton("+ NEW CHAT")) {
        create_new_session();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(); // ChildBg
}

// ============================================================================
// Sessions panel (left sidebar)
// ============================================================================
void AIChatScreen::render_sessions_panel(float width, float height) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::PushStyleColor(ImGuiCol_Border, BORDER_DIM);
    ImGui::BeginChild("##sessions_panel", ImVec2(width, height), ImGuiChildFlags_Borders);

    // Header
    ImGui::TextColored(TEXT_DIM, "SESSIONS (%d)", total_sessions_);
    ImGui::SameLine(width - 70);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ERROR_RED.x, ERROR_RED.y, ERROR_RED.z, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
    if (ImGui::SmallButton("CLEAR ALL") && !sessions_.empty()) {
        delete_all_sessions();
    }
    ImGui::PopStyleColor(2);

    // Search
    ImGui::SetNextItemWidth(width - 16);
    ImGui::InputTextWithHint("##sess_search", "Search sessions...", search_buf_, sizeof(search_buf_));

    // Stats
    ImGui::TextColored(INFO, "MSGS: %d | TOKENS: %d", total_messages_, total_tokens_);
    ImGui::Separator();

    // Session list
    std::string filter(search_buf_);
    std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

    ImGui::BeginChild("##sess_list", ImVec2(0, 0));
    for (int i = 0; i < (int)sessions_.size(); i++) {
        auto& s = sessions_[i];

        // Filter
        if (!filter.empty()) {
            std::string title_lower = s.title;
            std::transform(title_lower.begin(), title_lower.end(), title_lower.begin(), ::tolower);
            if (title_lower.find(filter) == std::string::npos) continue;
        }

        bool is_selected = (s.session_uuid == current_session_uuid_);

        ImGui::PushID(i);

        // Highlight selected
        if (is_selected) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(ACCENT.x, ACCENT.y, ACCENT.z, 0.08f));
        }

        float item_h = 52;
        ImGui::BeginChild(("##sess_" + std::to_string(i)).c_str(),
                          ImVec2(width - 16, item_h), ImGuiChildFlags_None);

        // Renaming mode
        if (renaming_session_uuid_ == s.session_uuid) {
            ImGui::SetNextItemWidth(width - 80);
            if (ImGui::InputText("##rename", rename_buf_, sizeof(rename_buf_),
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                save_rename();
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, SUCCESS);
            if (ImGui::SmallButton("OK")) save_rename();
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
            if (ImGui::SmallButton("X")) cancel_rename();
            ImGui::PopStyleColor();
        } else {
            // Title (clickable)
            if (ImGui::Selectable(("##sel_" + s.session_uuid).c_str(), is_selected,
                                  ImGuiSelectableFlags_AllowDoubleClick, ImVec2(width - 60, 20))) {
                select_session(i);
            }
            ImGui::SameLine(8);
            ImGui::TextColored(is_selected ? TEXT_PRIMARY : TEXT_SECONDARY, "%s", s.title.c_str());

            // Bottom row: message count + time + actions
            ImGui::TextColored(TEXT_DIM, "%lld msgs", (long long)s.message_count);
            ImGui::SameLine(width - 80);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, WARNING);
            if (ImGui::SmallButton("Ren")) {
                start_rename(s.session_uuid, s.title);
            }
            ImGui::PopStyleColor(2);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
            if (ImGui::SmallButton("Del")) {
                delete_session(s.session_uuid);
            }
            ImGui::PopStyleColor(2);
        }

        ImGui::EndChild();

        if (is_selected) ImGui::PopStyleColor();

        ImGui::PopID();
    }
    ImGui::EndChild(); // sess_list

    ImGui::EndChild(); // sessions_panel
    ImGui::PopStyleColor(2); // ChildBg, Border
}

// ============================================================================
// Chat panel (center)
// ============================================================================
void AIChatScreen::render_chat_panel(float width, float height) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARK);
    ImGui::BeginChild("##chat_panel", ImVec2(width, height), ImGuiChildFlags_None);

    // Chat header
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
        ImGui::BeginChild("##chat_header", ImVec2(width, 30), ImGuiChildFlags_None);
        ImGui::SetCursorPos(ImVec2(8, 6));

        if (!current_session_uuid_.empty()) {
            // Find session title
            std::string title = "CHAT";
            for (auto& s : sessions_) {
                if (s.session_uuid == current_session_uuid_) {
                    title = s.title;
                    break;
                }
            }
            std::transform(title.begin(), title.end(), title.begin(), ::toupper);
            ImGui::TextColored(TEXT_DIM, "%s", title.c_str());

            ImGui::SameLine(width - 60);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ERROR_RED.x, ERROR_RED.y, ERROR_RED.z, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_Text, ERROR_RED);
            if (ImGui::SmallButton("CLEAR")) {
                clear_current_chat();
            }
            ImGui::PopStyleColor(2);
        } else {
            ImGui::TextColored(TEXT_DIM, "NO ACTIVE SESSION");
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    // Messages area
    float input_h = 90.0f;
    float messages_h = height - 30 - input_h;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##messages_area", ImVec2(width, messages_h), ImGuiChildFlags_None);

    if (messages_.empty() && streaming_content_.empty() && !is_streaming_) {
        render_welcome_screen(width, messages_h);
    } else {
        ImGui::SetCursorPosX(8);
        ImGui::BeginGroup();

        for (auto& msg : messages_) {
            render_message(msg, width - 32);
        }

        // Streaming content
        if (is_streaming_) {
            render_streaming_message(width - 32);
        }

        ImGui::EndGroup();

        if (scroll_to_bottom_) {
            ImGui::SetScrollHereY(1.0f);
            scroll_to_bottom_ = false;
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(); // messages bg

    // Input area
    render_input_area(width);

    ImGui::EndChild(); // chat_panel
    ImGui::PopStyleColor(); // ChildBg
}

// ============================================================================
// Welcome screen (when no messages)
// ============================================================================
void AIChatScreen::render_welcome_screen(float width, float height) {
    using namespace theme::colors;

    float center_x = width / 2 - 100;
    float center_y = height / 2 - 60;
    ImGui::SetCursorPos(ImVec2(center_x, center_y));

    ImGui::BeginGroup();
    ImGui::TextColored(ACCENT, "AI CHAT");
    ImGui::TextColored(TEXT_SECONDARY, "Financial intelligence assistant");
    ImGui::Spacing();

    std::string prov_info = "PROVIDER: " +
        (active_config_.provider.empty() ? "NONE" : active_config_.provider) +
        " | MODEL: " + (active_config_.model.empty() ? "N/A" : active_config_.model);
    std::transform(prov_info.begin(), prov_info.end(), prov_info.begin(), ::toupper);

    ImGui::PushStyleColor(ImGuiCol_Text, WARNING);
    ImGui::TextWrapped("%s", prov_info.c_str());
    ImGui::PopStyleColor();

    ImGui::Spacing();
    if (theme::AccentButton("START NEW CHAT")) {
        create_new_session();
    }
    ImGui::EndGroup();
}

// ============================================================================
// Message rendering
// ============================================================================
void AIChatScreen::render_message(const db::ChatMessage& msg, float width) {
    using namespace theme::colors;

    bool is_user = (msg.role == "user");
    ImVec4 border_color = is_user ? WARNING : ACCENT;
    ImVec4 label_color = is_user ? WARNING : ACCENT;
    const char* label = is_user ? "YOU" : "AI";

    ImGui::PushID(msg.id.c_str());
    ImGui::Spacing();

    // Alignment: user right, assistant left
    float msg_max_w = width * 0.85f;
    if (is_user) {
        ImGui::SetCursorPosX(width - msg_max_w + 8);
    } else {
        ImGui::SetCursorPosX(8);
    }

    // Message box
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, border_color);

    std::string child_id = "##msg_" + msg.id;
    ImGui::BeginChild(child_id.c_str(), ImVec2(msg_max_w, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);

    // Header: role + timestamp + provider
    ImGui::TextColored(label_color, "%s", label);
    ImGui::SameLine();
    if (!msg.timestamp.empty()) {
        // Show just the time portion
        std::string time_str = msg.timestamp;
        auto space = time_str.find(' ');
        if (space != std::string::npos && space + 1 < time_str.size()) {
            time_str = time_str.substr(space + 1);
        }
        // Truncate to HH:MM:SS
        if (time_str.size() > 8) time_str = time_str.substr(0, 8);
        ImGui::TextColored(TEXT_DIM, "%s", time_str.c_str());
    }
    if (msg.provider) {
        ImGui::SameLine();
        std::string prov = *msg.provider;
        std::transform(prov.begin(), prov.end(), prov.begin(), ::toupper);
        ImGui::TextColored(TEXT_DIM, "| %s", prov.c_str());
    }
    ImGui::Separator();

    // Content
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + msg_max_w - 20);

    // Check for error messages
    if (!is_user && msg.content.rfind("[ERROR]", 0) == 0) {
        ImGui::TextColored(ERROR_RED, "%s", msg.content.c_str());
    } else {
        ImGui::TextColored(TEXT_PRIMARY, "%s", msg.content.c_str());
    }

    ImGui::PopTextWrapPos();

    ImGui::EndChild();
    ImGui::PopStyleColor(2); // Border, ChildBg
    ImGui::PopStyleVar();    // ChildBorderSize

    ImGui::PopID();
}

// ============================================================================
// Streaming message (live response)
// ============================================================================
void AIChatScreen::render_streaming_message(float width) {
    using namespace theme::colors;

    std::string content;
    {
        std::lock_guard<std::mutex> lock(stream_mutex_);
        content = streaming_content_;
    }

    ImGui::Spacing();
    ImGui::SetCursorPosX(8);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 2.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ACCENT);

    float msg_w = width * 0.85f;
    ImGui::BeginChild("##streaming_msg", ImVec2(msg_w, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);

    // Animated header
    ImGui::TextColored(ACCENT, "AI");
    ImGui::SameLine();
    double t = ImGui::GetTime();
    int dots = ((int)(t * 3)) % 4;
    char dot_buf[8] = {};
    for (int i = 0; i < dots; i++) dot_buf[i] = '.';
    ImGui::TextColored(ACCENT, "TYPING%s", dot_buf);
    ImGui::Separator();

    if (!content.empty()) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + msg_w - 20);
        ImGui::TextColored(TEXT_PRIMARY, "%s", content.c_str());
        ImGui::PopTextWrapPos();

        // Blinking cursor
        if (((int)(t * 2)) % 2 == 0) {
            ImGui::SameLine(0, 0);
            ImGui::TextColored(ACCENT, "|");
        }
    } else {
        ImGui::TextColored(TEXT_DIM, "Thinking...");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();

    // Auto-scroll while streaming
    ImGui::SetScrollHereY(1.0f);
}

// ============================================================================
// Input area
// ============================================================================
void AIChatScreen::render_input_area(float width) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##input_area", ImVec2(width, 90), ImGuiChildFlags_None);

    float btn_w = 80;
    float mic_w = 40;
    float input_w = width - btn_w - mic_w - 32;

    ImGui::SetCursorPos(ImVec2(8, 8));

    // Multiline input
    ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
    ImGui::PushStyleColor(ImGuiCol_Border, BORDER_DIM);
    ImGui::SetNextItemWidth(input_w);

    bool enter_pressed = ImGui::InputTextMultiline("##chat_input", input_buf_, sizeof(input_buf_),
        ImVec2(input_w, 50),
        ImGuiInputTextFlags_CtrlEnterForNewLine |
        ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopStyleColor(2);

    // Send on Enter (when not streaming)
    bool can_send = std::strlen(input_buf_) > 0 && !is_streaming_;

    // MIC button
    ImGui::SameLine();
    ImGui::SetCursorPosY(8);
    {
        auto& vs = voice::VoiceService::instance();
        bool listening = vs.is_listening();

        ImVec4 mic_color = listening ? ERROR_RED : TEXT_SECONDARY;
        ImVec4 mic_bg = listening ? ImVec4(ERROR_RED.x, ERROR_RED.y, ERROR_RED.z, 0.15f) : BG_WIDGET;

        ImGui::PushStyleColor(ImGuiCol_Button, mic_bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, listening ? ImVec4(ERROR_RED.x, ERROR_RED.y, ERROR_RED.z, 0.3f) : BG_HOVER);
        ImGui::PushStyleColor(ImGuiCol_Text, mic_color);

        if (ImGui::Button(listening ? "STOP" : "MIC", ImVec2(mic_w, 50))) {
            if (listening) {
                vs.stop_listening();
            } else {
                vs.start_listening();
            }
        }
        ImGui::PopStyleColor(3);

        // Pulsing indicator when listening
        if (listening) {
            float level = vs.audio_level();
            ImVec2 p = ImGui::GetItemRectMin();
            float bar_h = level * 40.0f;
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(p.x, p.y + 50 - bar_h), ImVec2(p.x + mic_w, p.y + 50),
                ImGui::ColorConvertFloat4ToU32(ImVec4(ERROR_RED.x, ERROR_RED.y, ERROR_RED.z, 0.4f)));
        }
    }

    // SEND button
    ImGui::SameLine();
    ImGui::SetCursorPosY(8);

    if (can_send) {
        if (theme::AccentButton("SEND##chat_send") || enter_pressed) {
            send_message();
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DISABLED);
        ImGui::Button("SEND##chat_send_dis");
        ImGui::PopStyleColor(2);
    }

    // Status line
    ImGui::SetCursorPos(ImVec2(8, 65));
    {
        auto& vs = voice::VoiceService::instance();
        if (vs.is_listening()) {
            double t = ImGui::GetTime();
            int dots = ((int)(t * 3)) % 4;
            char dot_buf[8] = {};
            for (int i = 0; i < dots; i++) dot_buf[i] = '.';
            ImGui::TextColored(ERROR_RED, "LISTENING%s", dot_buf);
        } else if (vs.is_speaking()) {
            ImGui::TextColored(INFO, "SPEAKING...");
        } else {
            size_t len = std::strlen(input_buf_);
            if (len > 0) {
                ImGui::TextColored(TEXT_DIM, "%zu CHARS", len);
            } else {
                ImGui::TextColored(TEXT_DIM, "STATUS: %s", status_.c_str());
            }
        }
    }

    // Error display
    if (!error_.empty() && (ImGui::GetTime() - error_time_) < 8.0) {
        ImGui::SameLine(width - 400);
        ImGui::TextColored(ERROR_RED, "%s", error_.c_str());
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(); // ChildBg
}

// ============================================================================
// Info panel (right sidebar)
// ============================================================================
void AIChatScreen::render_info_panel(float width, float height) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::PushStyleColor(ImGuiCol_Border, BORDER_DIM);
    ImGui::BeginChild("##info_panel", ImVec2(width, height), ImGuiChildFlags_Borders);

    // ── Quick Prompts Section ──
    ImGui::TextColored(TEXT_DIM, "QUICK PROMPTS");
    ImGui::Separator();

    for (auto& qp : quick_prompts_) {
        ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
        if (ImGui::Button(qp.label.c_str(), ImVec2(width - 16, 0))) {
            std::strncpy(input_buf_, qp.prompt.c_str(), sizeof(input_buf_) - 1);
            input_buf_[sizeof(input_buf_) - 1] = '\0';
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", qp.prompt.c_str());
        }
        ImGui::PopStyleColor(3);
    }

    ImGui::Spacing();
    ImGui::Separator();

    // ── System Info Section ──
    ImGui::TextColored(TEXT_DIM, "SYSTEM INFO");
    ImGui::Separator();

    if (!active_config_.provider.empty()) {
        std::string prov = active_config_.provider;
        std::transform(prov.begin(), prov.end(), prov.begin(), ::toupper);
        ImGui::TextColored(INFO, "PROVIDER: %s", prov.c_str());
        ImGui::TextColored(INFO, "MODEL: %s", active_config_.model.c_str());

        if (active_config_.base_url && !active_config_.base_url->empty()) {
            ImGui::TextColored(TEXT_DIM, "ENDPOINT: %s", active_config_.base_url->c_str());
        }

        ImGui::TextColored(INFO, "TEMP: %.2f | MAX TOKENS: %lld",
            global_settings_.temperature, (long long)global_settings_.max_tokens);

        ImGui::Spacing();

        // Status badges
        // Streaming
        bool supports_stream = provider_supports_streaming(active_config_.provider);
        ImGui::TextColored(supports_stream ? SUCCESS : TEXT_DIM,
            "STREAMING: %s", supports_stream ? "ENABLED" : "DISABLED");

        // API Key
        bool needs_key = provider_requires_api_key(active_config_.provider);
        if (!needs_key) {
            ImGui::TextColored(INFO, "API KEY: NOT REQUIRED");
        } else {
            bool has_key = active_config_.api_key && !active_config_.api_key->empty();
            ImGui::TextColored(has_key ? SUCCESS : ERROR_RED,
                "API KEY: %s", has_key ? "CONFIGURED" : "MISSING");
        }

        // System prompt
        if (!global_settings_.system_prompt.empty()) {
            ImGui::TextColored(ACCENT, "CUSTOM PROMPT: YES");
        }
    } else {
        ImGui::TextColored(ERROR_RED, "NO LLM PROVIDER CONFIGURED");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}

// ============================================================================
// Status bar (bottom)
// ============================================================================
void AIChatScreen::render_status_bar(float width) {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##chat_statusbar", ImVec2(width, 24), ImGuiChildFlags_None);

    ImGui::SetCursorPos(ImVec2(8, 4));

    ImGui::TextColored(TEXT_DIM, "TAB: AI CHAT");
    ImGui::SameLine();
    ImGui::TextColored(TEXT_DIM, "|");
    ImGui::SameLine();

    std::string prov = active_config_.provider;
    std::transform(prov.begin(), prov.end(), prov.begin(), ::toupper);
    ImGui::TextColored(TEXT_DIM, "PROVIDER: %s", prov.empty() ? "NONE" : prov.c_str());
    ImGui::SameLine();
    ImGui::TextColored(TEXT_DIM, "|");
    ImGui::SameLine();
    ImGui::TextColored(TEXT_DIM, "SESSION: %s",
        current_session_uuid_.empty() ? "NONE" : "ACTIVE");

    // Right side
    float right_x = width - 250;
    ImGui::SameLine(right_x);
    ImGui::TextColored(TEXT_DIM, "MESSAGES: %d", (int)messages_.size());
    ImGui::SameLine();
    ImGui::TextColored(TEXT_DIM, "| STATUS: %s",
        is_streaming_ ? "STREAMING..." : "READY");

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace fincept::ai_chat
