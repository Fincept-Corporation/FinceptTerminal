// Agent Studio Screen — 8-view AI agent management interface

#include "agent_studio_screen.h"
#include "storage/database.h"
#include "core/logger.h"
#include "ui/theme.h"
#include "python/python_runner.h"
#include <imgui.h>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <algorithm>

namespace fincept::agent_studio {

// ============================================================================
// Init
// ============================================================================
void AgentStudioScreen::init() {
    if (initialized_) return;

    std::strncpy(test_query_, "Analyze AAPL stock and provide your investment thesis.",
                 sizeof(test_query_) - 1);

    // Load agents
    agents_ = AgentDiscovery::instance().get_all_agents();
    last_discovery_time_ = ImGui::GetTime();

    initialized_ = true;
    LOG_INFO("AgentStudio", "Initialized with %zu agents", agents_.size());
}

// ============================================================================
// Main render
// ============================================================================
void AgentStudioScreen::render() {
    init();

    using namespace theme::colors;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    float tab_bar_h = ImGui::GetFrameHeight() + 4;
    float footer_h = ImGui::GetFrameHeight();

    float x = vp->WorkPos.x;
    float y = vp->WorkPos.y + tab_bar_h;
    float w = vp->WorkSize.x;
    float h = vp->WorkSize.y - tab_bar_h - footer_h;

    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(w, h));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_DARK);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("##agent_studio", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoDocking);

    // Keyboard shortcuts (Ctrl+1-8 to switch views)
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && ImGui::GetIO().KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_1)) view_mode_ = ViewMode::Agents;
        if (ImGui::IsKeyPressed(ImGuiKey_2)) view_mode_ = ViewMode::Teams;
        if (ImGui::IsKeyPressed(ImGuiKey_3)) view_mode_ = ViewMode::Chat;
        if (ImGui::IsKeyPressed(ImGuiKey_4)) view_mode_ = ViewMode::Planner;
        if (ImGui::IsKeyPressed(ImGuiKey_5)) view_mode_ = ViewMode::Workflows;
        if (ImGui::IsKeyPressed(ImGuiKey_6)) view_mode_ = ViewMode::Tools;
        if (ImGui::IsKeyPressed(ImGuiKey_7)) view_mode_ = ViewMode::System;
        if (ImGui::IsKeyPressed(ImGuiKey_8)) view_mode_ = ViewMode::Create;
    }

    // View mode tabs at top
    render_view_tabs(w);

    // Content area
    float tabs_h = 36.0f;
    float content_h = h - tabs_h - 8;
    float content_w = w - 16;

    ImGui::SetCursorPos(ImVec2(8, tabs_h + 4));
    ImGui::BeginChild("##agent_content", ImVec2(content_w, content_h));

    switch (view_mode_) {
        case ViewMode::Agents:    render_agents_view(content_w, content_h); break;
        case ViewMode::Teams:     render_teams_view(content_w, content_h); break;
        case ViewMode::Chat:      render_chat_view(content_w, content_h); break;
        case ViewMode::Planner:   render_planner_view(content_w, content_h); break;
        case ViewMode::Workflows: render_workflows_view(content_w, content_h); break;
        case ViewMode::Tools:     render_tools_view(content_w, content_h); break;
        case ViewMode::System:    render_system_view(content_w, content_h); break;
        case ViewMode::Create:    render_create_agent_view(content_w, content_h); break;
    }

    ImGui::EndChild();
    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // Refresh agent list periodically
    double now = ImGui::GetTime();
    if (now - last_discovery_time_ > 600.0) {
        agents_ = AgentDiscovery::instance().get_all_agents();
        last_discovery_time_ = now;
    }

    poll_futures();
}

// ============================================================================
// Poll async results on main thread
// ============================================================================
void AgentStudioScreen::poll_futures() {
    if (agent_executing_ && agent_result_future_.valid() &&
        agent_result_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        last_agent_result_ = agent_result_future_.get();
        agent_executing_ = false;
    }

    if (chat_executing_ && chat_result_future_.valid() &&
        chat_result_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        auto result = chat_result_future_.get();
        AgentChatMessage msg;
        msg.role = AgentChatMessage::Role::Assistant;
        msg.content = result.success ? result.response : ("Error: " + result.error);
        msg.agent_name = result.model_used.empty() ? "Agent" : result.model_used;
        time_t t = time(nullptr);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%H:%M:%S", localtime(&t));
        msg.timestamp = buf;
        chat_messages_.push_back(std::move(msg));
        chat_scroll_to_bottom_ = true;
        chat_executing_ = false;
    }

    if (team_executing_ && team_result_future_.valid() &&
        team_result_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        last_team_result_ = team_result_future_.get();
        team_executing_ = false;
    }

    if (plan_generating_ && plan_future_.valid() &&
        plan_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        current_plan_ = plan_future_.get();
        plan_generating_ = false;
        plan_executing_ = false;
    }
}

// ============================================================================
// View tab navigation bar
// ============================================================================
void AgentStudioScreen::render_view_tabs(float /*width*/) {
    using namespace theme::colors;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
    ImGui::SetCursorPos(ImVec2(8, 4));

    ViewMode modes[] = {
        ViewMode::Agents, ViewMode::Teams, ViewMode::Chat,
        ViewMode::Planner, ViewMode::Workflows, ViewMode::Tools,
        ViewMode::System, ViewMode::Create
    };

    for (auto mode : modes) {
        bool active = (view_mode_ == mode);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ACCENT_HOVER);
            ImGui::PushStyleColor(ImGuiCol_Text, BG_DARKEST);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BG_HOVER);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SECONDARY);
        }

        if (ImGui::Button(view_mode_label(mode))) {
            view_mode_ = mode;
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
    }

    ImGui::NewLine();
    ImGui::PopStyleVar(2);
    ImGui::Separator();
}

// ============================================================================
// Agents View — 3-column: [Category+List] [Details+Config] [Query+Results]
// ============================================================================
void AgentStudioScreen::render_agents_view(float width, float height) {
    using namespace theme::colors;

    float list_w = 200.0f;
    float result_w = 300.0f;
    float config_w = width - list_w - result_w - 16;

    // ── Left Panel: Categories + Agent List ──
    ImGui::BeginChild("##agent_list", ImVec2(list_w, height), ImGuiChildFlags_Borders);

    ImGui::TextColored(ACCENT, "CATEGORIES");
    ImGui::Separator();

    auto categories = builtin_categories();
    for (auto& cat : categories) {
        bool selected = (selected_category_ == cat.id);
        ImVec4 col = ImGui::ColorConvertU32ToFloat4(cat.color);
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Text, col);
        }
        char label[128];
        std::snprintf(label, sizeof(label), "%s %s", cat.icon, cat.name.c_str());
        if (ImGui::Selectable(label, selected)) {
            selected_category_ = cat.id;
            selected_agent_idx_ = -1;
        }
        if (selected) ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ACCENT, "AGENTS");
    ImGui::Separator();

    // Search
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##agent_search", "Search...", agent_search_, sizeof(agent_search_));

    // Filter agents
    std::vector<int> visible;
    std::string search_str(agent_search_);
    std::transform(search_str.begin(), search_str.end(), search_str.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });

    for (int i = 0; i < (int)agents_.size(); i++) {
        auto& a = agents_[i];
        if (selected_category_ != "all" && a.category != selected_category_) continue;
        if (!search_str.empty()) {
            std::string name = a.name;
            std::transform(name.begin(), name.end(), name.begin(),
                           [](unsigned char c) { return (char)std::tolower(c); });
            if (name.find(search_str) == std::string::npos) continue;
        }
        visible.push_back(i);
    }

    for (int idx : visible) {
        bool selected = (idx == selected_agent_idx_);
        if (agents_[idx].user_created) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.66f, 0.33f, 0.97f, 1.0f));
        }
        if (ImGui::Selectable(agents_[idx].name.c_str(), selected)) {
            selected_agent_idx_ = idx;
            // Populate edit buffers
            auto& a = agents_[idx];
            std::strncpy(edit_provider_, a.provider.c_str(), sizeof(edit_provider_) - 1);
            std::string model = a.config.contains("model")
                ? a.config["model"].value("model_id", "gpt-4o") : "gpt-4o";
            std::strncpy(edit_model_, model.c_str(), sizeof(edit_model_) - 1);
            edit_temperature_ = (float)a.config.value("temperature", 0.7);
            edit_max_tokens_ = a.config.value("max_tokens", 4096);
            std::string instr = a.config.value("instructions", "");
            std::strncpy(edit_instructions_, instr.c_str(), sizeof(edit_instructions_) - 1);
            edit_memory_ = a.config.value("memory", false);
            edit_reasoning_ = a.config.value("reasoning", false);
            edit_guardrails_ = false;
            edit_knowledge_ = a.config.value("knowledge", false);
        }
        if (agents_[idx].user_created) ImGui::PopStyleColor();
    }

    ImGui::TextColored(TEXT_DIM, "%d agents", (int)visible.size());

    ImGui::EndChild();
    ImGui::SameLine(0, 4);

    // ── Center Panel: Agent Details + Config ──
    ImGui::BeginChild("##agent_config", ImVec2(config_w, height), ImGuiChildFlags_Borders);

    if (selected_agent_idx_ >= 0 && selected_agent_idx_ < (int)agents_.size()) {
        auto& agent = agents_[selected_agent_idx_];

        ImGui::TextColored(ACCENT, "%s", agent.name.c_str());
        ImGui::TextColored(TEXT_SECONDARY, "Category: %s", agent.category.c_str());
        ImGui::TextWrapped("%s", agent.description.c_str());
        if (agent.user_created) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.66f, 0.33f, 0.97f, 1.0f), "[CUSTOM]");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ACCENT, "CONFIGURATION");
        ImGui::Spacing();

        float label_w = 100.0f;
        float input_w = ImGui::GetContentRegionAvail().x - label_w - 8;

        // Provider
        ImGui::TextColored(TEXT_SECONDARY, "Provider");
        ImGui::SameLine(label_w);
        ImGui::SetNextItemWidth(input_w);
        ImGui::InputText("##agent_provider", edit_provider_, sizeof(edit_provider_));

        // Model
        ImGui::TextColored(TEXT_SECONDARY, "Model");
        ImGui::SameLine(label_w);
        ImGui::SetNextItemWidth(input_w);
        ImGui::InputText("##agent_model", edit_model_, sizeof(edit_model_));

        // Temperature
        ImGui::TextColored(TEXT_SECONDARY, "Temperature");
        ImGui::SameLine(label_w);
        ImGui::SetNextItemWidth(input_w);
        ImGui::SliderFloat("##agent_temp", &edit_temperature_, 0.0f, 2.0f, "%.2f");

        // Max Tokens
        ImGui::TextColored(TEXT_SECONDARY, "Max Tokens");
        ImGui::SameLine(label_w);
        ImGui::SetNextItemWidth(input_w);
        ImGui::InputInt("##agent_tokens", &edit_max_tokens_, 100, 1000);

        ImGui::Spacing();

        // Instructions
        ImGui::TextColored(TEXT_SECONDARY, "Instructions");
        float avail = ImGui::GetContentRegionAvail().y - 80;
        if (avail < 60) avail = 60;
        ImGui::InputTextMultiline("##agent_instr", edit_instructions_,
            sizeof(edit_instructions_), ImVec2(-1, avail));

        // Feature toggles
        ImGui::Checkbox("Memory", &edit_memory_);
        ImGui::SameLine();
        ImGui::Checkbox("Reasoning", &edit_reasoning_);
        ImGui::SameLine();
        ImGui::Checkbox("Guardrails", &edit_guardrails_);
        ImGui::SameLine();
        ImGui::Checkbox("Knowledge", &edit_knowledge_);

        // Capabilities
        if (!agent.capabilities.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(TEXT_DIM, "Capabilities:");
            ImGui::SameLine();
            for (size_t i = 0; i < agent.capabilities.size(); i++) {
                if (i > 0) ImGui::SameLine();
                ImGui::TextColored(INFO, "[%s]", agent.capabilities[i].c_str());
            }
        }
    } else {
        ImGui::TextColored(TEXT_DIM, "Select an agent from the list to view its configuration.");
    }

    ImGui::EndChild();
    ImGui::SameLine(0, 4);

    // ── Right Panel: Query + Results ──
    ImGui::BeginChild("##agent_results", ImVec2(result_w, height), ImGuiChildFlags_Borders);

    ImGui::TextColored(ACCENT, "TEST QUERY");
    ImGui::Separator();

    ImGui::InputTextMultiline("##test_query", test_query_, sizeof(test_query_),
        ImVec2(-1, 80));

    bool can_run = selected_agent_idx_ >= 0 && !agent_executing_;
    if (!can_run) ImGui::BeginDisabled();
    if (theme::AccentButton("RUN AGENT", ImVec2(-1, 0))) {
        AgentConfig config;
        config.provider = edit_provider_;
        config.model_id = edit_model_;
        config.temperature = edit_temperature_;
        config.max_tokens = edit_max_tokens_;
        config.instructions = edit_instructions_;
        config.enable_memory = edit_memory_;
        config.enable_reasoning = edit_reasoning_;
        config.enable_guardrails = edit_guardrails_;
        config.enable_knowledge = edit_knowledge_;

        agent_result_future_ = AgentService::instance().run_agent(config, test_query_);
        agent_executing_ = true;
        last_agent_result_ = {};
    }
    if (!can_run) ImGui::EndDisabled();

    if (agent_executing_) {
        ImGui::Spacing();
        theme::LoadingSpinner("Executing...");
    }

    if (!last_agent_result_.response.empty() || !last_agent_result_.error.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ACCENT, "RESULT");
        ImGui::Separator();

        if (last_agent_result_.success) {
            ImGui::TextWrapped("%s", last_agent_result_.response.c_str());
        } else {
            ImGui::TextColored(ERROR_RED, "Error: %s", last_agent_result_.error.c_str());
        }

        ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM, "Time: %.1fms | Tokens: %d | Model: %s",
            last_agent_result_.execution_time_ms,
            last_agent_result_.tokens_used,
            last_agent_result_.model_used.c_str());
    }

    ImGui::EndChild();
}

// ============================================================================
// Chat View
// ============================================================================
void AgentStudioScreen::render_chat_view(float width, float height) {
    using namespace theme::colors;

    // Top bar: auto-routing toggle + quick actions
    ImGui::Checkbox("Auto-Routing", &use_auto_routing_);
    ImGui::SameLine(0, 16);

    if (ImGui::SmallButton("ANALYZE")) {
        std::strncpy(chat_input_, "Analyze AAPL stock and provide investment thesis", sizeof(chat_input_) - 1);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("REBALANCE")) {
        std::strncpy(chat_input_, "Suggest portfolio rebalancing strategy", sizeof(chat_input_) - 1);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("RISK")) {
        std::strncpy(chat_input_, "Assess current portfolio risk and suggest hedging", sizeof(chat_input_) - 1);
    }

    ImGui::Separator();

    // Message area
    float input_h = 36;
    float messages_h = height - ImGui::GetCursorPosY() - input_h - 16;

    ImGui::BeginChild("##chat_messages", ImVec2(width, messages_h), ImGuiChildFlags_Borders);

    if (chat_messages_.empty()) {
        ImGui::SetCursorPos(ImVec2(width * 0.3f, messages_h * 0.4f));
        ImGui::TextColored(TEXT_DIM, "Start a conversation with the AI agent.");
        ImGui::SetCursorPosX(width * 0.25f);
        ImGui::TextColored(TEXT_DIM, "Type a message below or use a quick action above.");
    }

    for (auto& msg : chat_messages_) {
        ImGui::Spacing();

        if (msg.role == AgentChatMessage::Role::User) {
            ImGui::TextColored(WARNING, "YOU");
            ImGui::SameLine();
            ImGui::TextColored(TEXT_DIM, "[%s]", msg.timestamp.c_str());
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
            ImGui::TextWrapped("%s", msg.content.c_str());
            ImGui::PopStyleColor();
        } else if (msg.role == AgentChatMessage::Role::Assistant) {
            ImGui::TextColored(SUCCESS, "ASSISTANT");
            if (!msg.agent_name.empty()) {
                ImGui::SameLine();
                ImGui::TextColored(INFO, "(%s)", msg.agent_name.c_str());
            }
            ImGui::SameLine();
            ImGui::TextColored(TEXT_DIM, "[%s]", msg.timestamp.c_str());
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
            ImGui::TextWrapped("%s", msg.content.c_str());
            ImGui::PopStyleColor();

            if (msg.routing_info && msg.routing_info->success) {
                ImGui::TextColored(TEXT_DIM, "  Routed to: %s (%.0f%% confidence)",
                    msg.routing_info->agent_id.c_str(),
                    msg.routing_info->confidence * 100);
            }
        } else {
            ImGui::TextColored(TEXT_DIM, "[SYSTEM] %s", msg.content.c_str());
        }

        ImGui::Separator();
    }

    if (chat_executing_) {
        ImGui::Spacing();
        theme::LoadingSpinner("Thinking...");
    }

    if (chat_scroll_to_bottom_) {
        ImGui::SetScrollHereY(1.0f);
        chat_scroll_to_bottom_ = false;
    }

    ImGui::EndChild();

    // Input bar
    ImGui::SetNextItemWidth(width - 80);
    bool enter_pressed = ImGui::InputText("##chat_input", chat_input_, sizeof(chat_input_),
        ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();

    bool can_send = !chat_executing_ && std::strlen(chat_input_) > 0;
    if (!can_send) ImGui::BeginDisabled();
    bool send = ImGui::Button("SEND", ImVec2(70, 0)) || (enter_pressed && can_send);
    if (!can_send) ImGui::EndDisabled();

    if (send) {
        // Add user message
        AgentChatMessage user_msg;
        user_msg.role = AgentChatMessage::Role::User;
        user_msg.content = chat_input_;
        time_t t = time(nullptr);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%H:%M:%S", localtime(&t));
        user_msg.timestamp = buf;
        chat_messages_.push_back(std::move(user_msg));

        // Build config from active LLM
        AgentConfig config;
        auto llm_configs = db::ops::get_llm_configs();
        for (auto& c : llm_configs) {
            if (c.is_active) {
                config.provider = c.provider;
                config.model_id = c.model;
                break;
            }
        }
        auto global = db::ops::get_llm_global_settings();
        config.temperature = global.temperature;
        config.max_tokens = (int)global.max_tokens;
        config.instructions = global.system_prompt;

        std::string query = chat_input_;

        if (use_auto_routing_) {
            std::string api_key;
            for (auto& c : llm_configs) {
                if (c.is_active && c.api_key) { api_key = *c.api_key; break; }
            }
            chat_result_future_ = AgentService::instance().execute_routed(
                query, config.provider, config.model_id, api_key);
        } else {
            chat_result_future_ = AgentService::instance().run_agent(config, query);
        }

        chat_executing_ = true;
        chat_scroll_to_bottom_ = true;
        std::memset(chat_input_, 0, sizeof(chat_input_));
    }
}

// ============================================================================
// Teams View
// ============================================================================
void AgentStudioScreen::render_teams_view(float width, float height) {
    using namespace theme::colors;

    float member_w = 200.0f;
    float avail_w = 250.0f;
    float config_w = width - member_w - avail_w - 16;

    // ── Left: Team Members ──
    ImGui::BeginChild("##team_members", ImVec2(member_w, height), ImGuiChildFlags_Borders);
    ImGui::TextColored(ACCENT, "TEAM MEMBERS");
    ImGui::Separator();

    for (int i = 0; i < (int)team_config_.members.size(); i++) {
        auto& m = team_config_.members[i];
        bool is_leader = (i == team_config_.leader_index);

        char label[256];
        std::snprintf(label, sizeof(label), "%s%s %s",
            is_leader ? "[L] " : "", m.name.c_str(), m.role.c_str());

        ImGui::PushID(i);
        ImGui::TextColored(is_leader ? ACCENT : TEXT_PRIMARY, "%d. %s", i + 1, m.name.c_str());
        if (!m.role.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(TEXT_DIM, "(%s)", m.role.c_str());
        }
        ImGui::TextColored(TEXT_DIM, "   %s/%s", m.provider.c_str(), m.model_id.c_str());

        if (ImGui::SmallButton("Remove")) {
            team_config_.members.erase(team_config_.members.begin() + i);
            if (team_config_.leader_index >= (int)team_config_.members.size())
                team_config_.leader_index = 0;
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }

    if (team_config_.members.empty()) {
        ImGui::TextColored(TEXT_DIM, "No members. Add agents from the list.");
    }

    ImGui::EndChild();
    ImGui::SameLine(0, 4);

    // ── Center: Available Agents ──
    ImGui::BeginChild("##team_avail", ImVec2(avail_w, height), ImGuiChildFlags_Borders);
    ImGui::TextColored(ACCENT, "AVAILABLE AGENTS");
    ImGui::Separator();

    for (int i = 0; i < (int)agents_.size(); i++) {
        ImGui::PushID(1000 + i);
        if (ImGui::Selectable(agents_[i].name.c_str(), team_selected_agent_ == i)) {
            team_selected_agent_ = i;
        }
        ImGui::SameLine(avail_w - 30);
        if (ImGui::SmallButton("+")) {
            TeamMember member;
            member.name = agents_[i].name;
            member.role = agents_[i].category;
            member.provider = agents_[i].provider;
            member.model_id = agents_[i].config.contains("model")
                ? agents_[i].config["model"].value("model_id", "gpt-4o") : "gpt-4o";
            member.instructions = agents_[i].config.value("instructions", "");
            team_config_.members.push_back(std::move(member));
        }
        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::SameLine(0, 4);

    // ── Right: Team Config + Execution ──
    ImGui::BeginChild("##team_config", ImVec2(config_w, height), ImGuiChildFlags_Borders);
    ImGui::TextColored(ACCENT, "TEAM CONFIGURATION");
    ImGui::Separator();

    // Team name
    char name_buf[256];
    std::strncpy(name_buf, team_config_.name.c_str(), sizeof(name_buf) - 1);
    name_buf[sizeof(name_buf) - 1] = 0;
    ImGui::TextColored(TEXT_SECONDARY, "Name");
    ImGui::SameLine(80);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##team_name", name_buf, sizeof(name_buf))) {
        team_config_.name = name_buf;
    }

    // Mode selector
    ImGui::TextColored(TEXT_SECONDARY, "Mode");
    ImGui::SameLine(80);
    const char* mode_names[] = {"DELEGATE", "ROUTE", "COLLABORATE"};
    int mode_idx = (int)team_config_.mode;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##team_mode", &mode_idx, mode_names, 3)) {
        team_config_.mode = (TeamMode)mode_idx;
    }

    // Leader
    if (!team_config_.members.empty()) {
        ImGui::TextColored(TEXT_SECONDARY, "Leader");
        ImGui::SameLine(80);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##team_leader",
                team_config_.leader_index < (int)team_config_.members.size()
                    ? team_config_.members[team_config_.leader_index].name.c_str() : "")) {
            for (int i = 0; i < (int)team_config_.members.size(); i++) {
                if (ImGui::Selectable(team_config_.members[i].name.c_str(),
                        i == team_config_.leader_index)) {
                    team_config_.leader_index = i;
                }
            }
            ImGui::EndCombo();
        }
    }

    ImGui::Checkbox("Show member responses", &team_config_.show_member_responses);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ACCENT, "EXECUTION");
    ImGui::Separator();

    ImGui::InputTextMultiline("##team_query", team_query_, sizeof(team_query_), ImVec2(-1, 60));

    bool can_run = !team_executing_ && !team_config_.members.empty() && std::strlen(team_query_) > 0;
    if (!can_run) ImGui::BeginDisabled();
    if (theme::AccentButton("RUN TEAM", ImVec2(-1, 0))) {
        std::string api_key;
        auto llm_configs = db::ops::get_llm_configs();
        for (auto& c : llm_configs) {
            if (c.is_active && c.api_key) { api_key = *c.api_key; break; }
        }
        team_config_.coordinator_provider = team_config_.members.empty() ? "openai"
            : team_config_.members[team_config_.leader_index].provider;
        team_config_.coordinator_model = team_config_.members.empty() ? "gpt-4o"
            : team_config_.members[team_config_.leader_index].model_id;

        team_result_future_ = AgentService::instance().run_team(
            team_config_, team_query_, api_key);
        team_executing_ = true;
        last_team_result_ = {};
    }
    if (!can_run) ImGui::EndDisabled();

    if (team_executing_) {
        theme::LoadingSpinner("Team executing...");
    }

    if (!last_team_result_.response.empty() || !last_team_result_.error.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ACCENT, "RESULT");
        ImGui::Separator();
        if (last_team_result_.success) {
            ImGui::TextWrapped("%s", last_team_result_.response.c_str());
        } else {
            ImGui::TextColored(ERROR_RED, "Error: %s", last_team_result_.error.c_str());
        }
        ImGui::TextColored(TEXT_DIM, "Time: %.1fms", last_team_result_.execution_time_ms);
    }

    ImGui::EndChild();
}

// ============================================================================
// Planner View
// ============================================================================
void AgentStudioScreen::render_planner_view(float width, float height) {
    using namespace theme::colors;

    float left_w = 200.0f;
    float right_w = 250.0f;
    float center_w = width - left_w - right_w - 16;

    // ── Left: Templates + Custom + History ──
    ImGui::BeginChild("##plan_left", ImVec2(left_w, height), ImGuiChildFlags_Borders);
    ImGui::TextColored(ACCENT, "TEMPLATES");
    ImGui::Separator();

    auto templates = builtin_plan_templates();
    for (auto& tmpl : templates) {
        if (ImGui::Selectable(tmpl.name.c_str())) {
            current_plan_.name = tmpl.name;
            current_plan_.description = tmpl.description;
            current_plan_.steps.clear();
            // Generate basic steps from template
            PlanStep s1; s1.id = "step_1"; s1.name = "Gather Data"; s1.step_type = "data";
            PlanStep s2; s2.id = "step_2"; s2.name = "Analyze"; s2.step_type = "agent";
            s2.dependencies.push_back("step_1");
            PlanStep s3; s3.id = "step_3"; s3.name = "Generate Report"; s3.step_type = "output";
            s3.dependencies.push_back("step_2");
            current_plan_.steps = {s1, s2, s3};
            current_plan_.status = "draft";
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", tmpl.description.c_str());
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ACCENT, "CUSTOM QUERY");
    ImGui::Separator();

    ImGui::InputTextMultiline("##plan_query", planner_query_, sizeof(planner_query_),
        ImVec2(-1, 60));

    bool can_gen = !plan_generating_ && std::strlen(planner_query_) > 0;
    if (!can_gen) ImGui::BeginDisabled();
    if (theme::AccentButton("GENERATE PLAN", ImVec2(-1, 0))) {
        AgentConfig config;
        auto llm_configs = db::ops::get_llm_configs();
        for (auto& c : llm_configs) {
            if (c.is_active) { config.provider = c.provider; config.model_id = c.model; break; }
        }
        plan_future_ = AgentService::instance().generate_plan(planner_query_, config);
        plan_generating_ = true;
    }
    if (!can_gen) ImGui::EndDisabled();

    if (!plan_history_.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ACCENT, "HISTORY");
        ImGui::Separator();
        for (int i = (int)plan_history_.size() - 1; i >= 0; i--) {
            auto& p = plan_history_[i];
            ImVec4 col = p.status == "completed" ? SUCCESS :
                         p.status == "failed" ? ERROR_RED : TEXT_DIM;
            ImGui::TextColored(col, "%s", p.name.c_str());
        }
    }

    ImGui::EndChild();
    ImGui::SameLine(0, 4);

    // ── Center: Plan Steps ──
    ImGui::BeginChild("##plan_center", ImVec2(center_w, height), ImGuiChildFlags_Borders);
    ImGui::TextColored(ACCENT, "EXECUTION PLAN");
    ImGui::Separator();

    if (plan_generating_) {
        theme::LoadingSpinner("Generating plan...");
    } else if (current_plan_.steps.empty()) {
        ImGui::TextColored(TEXT_DIM, "Select a template or generate a plan from a query.");
    } else {
        ImGui::TextColored(TEXT_PRIMARY, "%s", current_plan_.name.c_str());
        ImGui::TextColored(TEXT_DIM, "%s", current_plan_.description.c_str());
        ImGui::Spacing();

        for (int i = 0; i < (int)current_plan_.steps.size(); i++) {
            auto& step = current_plan_.steps[i];
            ImVec4 col = TEXT_DIM;
            const char* badge = "PENDING";
            if (step.status == "running")   { col = WARNING; badge = "RUNNING"; }
            else if (step.status == "completed") { col = SUCCESS; badge = "DONE"; }
            else if (step.status == "failed")    { col = ERROR_RED; badge = "FAILED"; }

            ImGui::PushID(i);
            bool selected = (i == selected_plan_step_);
            if (ImGui::Selectable("", selected, 0, ImVec2(0, 24))) {
                selected_plan_step_ = i;
            }
            ImGui::SameLine(8);
            ImGui::TextColored(TEXT_PRIMARY, "Step %d: %s", i + 1, step.name.c_str());
            ImGui::SameLine(center_w - 80);
            ImGui::TextColored(col, "[%s]", badge);
            ImGui::PopID();
        }

        ImGui::Spacing();
        bool can_exec = !plan_executing_ && current_plan_.status != "completed";
        if (!can_exec) ImGui::BeginDisabled();
        if (theme::AccentButton("EXECUTE PLAN", ImVec2(-1, 0))) {
            AgentConfig config;
            auto llm_configs = db::ops::get_llm_configs();
            for (auto& c : llm_configs) {
                if (c.is_active) { config.provider = c.provider; config.model_id = c.model; break; }
            }
            plan_future_ = AgentService::instance().execute_plan(current_plan_, config);
            plan_executing_ = true;
        }
        if (!can_exec) ImGui::EndDisabled();
    }

    ImGui::EndChild();
    ImGui::SameLine(0, 4);

    // ── Right: Step Results ──
    ImGui::BeginChild("##plan_right", ImVec2(right_w, height), ImGuiChildFlags_Borders);
    ImGui::TextColored(ACCENT, "RESULTS");
    ImGui::Separator();

    if (selected_plan_step_ >= 0 && selected_plan_step_ < (int)current_plan_.steps.size()) {
        auto& step = current_plan_.steps[selected_plan_step_];
        ImGui::TextColored(TEXT_PRIMARY, "%s", step.name.c_str());
        ImGui::TextColored(TEXT_DIM, "Type: %s | Status: %s", step.step_type.c_str(), step.status.c_str());
        ImGui::Spacing();
        if (!step.result.empty()) {
            ImGui::TextWrapped("%s", step.result.c_str());
        }
        if (!step.error.empty()) {
            ImGui::TextColored(ERROR_RED, "%s", step.error.c_str());
        }
    } else {
        ImGui::TextColored(TEXT_DIM, "Select a step to view its results.");
    }

    ImGui::EndChild();
}

// ============================================================================
// Create Agent View
// ============================================================================
void AgentStudioScreen::render_create_agent_view(float width, float /*height*/) {
    using namespace theme::colors;

    ImGui::TextColored(ACCENT, "CREATE CUSTOM AGENT");
    ImGui::Separator();
    ImGui::Spacing();

    float label_w = 110.0f;
    float input_w = std::min(width - label_w - 20, 500.0f);

    // Name
    ImGui::TextColored(TEXT_SECONDARY, "Name");
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(input_w);
    ImGui::InputText("##create_name", create_name_, sizeof(create_name_));

    // Description
    ImGui::TextColored(TEXT_SECONDARY, "Description");
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(input_w);
    ImGui::InputText("##create_desc", create_desc_, sizeof(create_desc_));

    // Category
    ImGui::TextColored(TEXT_SECONDARY, "Category");
    ImGui::SameLine(label_w);
    auto categories = builtin_categories();
    const char* cat_names[16];
    int cat_count = 0;
    for (auto& c : categories) {
        if (c.id != "all") cat_names[cat_count++] = c.name.c_str();
    }
    ImGui::SetNextItemWidth(input_w);
    ImGui::Combo("##create_cat", &create_category_, cat_names, cat_count);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ACCENT, "MODEL CONFIGURATION");
    ImGui::Spacing();

    // Provider
    ImGui::TextColored(TEXT_SECONDARY, "Provider");
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(input_w);
    ImGui::InputText("##create_prov", create_provider_, sizeof(create_provider_));

    // Model
    ImGui::TextColored(TEXT_SECONDARY, "Model");
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(input_w);
    ImGui::InputText("##create_model", create_model_, sizeof(create_model_));

    // Temperature
    ImGui::TextColored(TEXT_SECONDARY, "Temperature");
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(input_w);
    ImGui::SliderFloat("##create_temp", &create_temperature_, 0.0f, 2.0f, "%.2f");

    // Max Tokens
    ImGui::TextColored(TEXT_SECONDARY, "Max Tokens");
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(input_w);
    ImGui::InputInt("##create_tokens", &create_max_tokens_, 100, 1000);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ACCENT, "SYSTEM PROMPT");
    ImGui::Spacing();

    ImGui::InputTextMultiline("##create_instr", create_instructions_,
        sizeof(create_instructions_), ImVec2(input_w + label_w, 150));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ACCENT, "FEATURES");
    ImGui::Spacing();

    ImGui::Checkbox("Memory", &create_memory_);
    ImGui::SameLine();
    ImGui::Checkbox("Reasoning", &create_reasoning_);
    ImGui::SameLine();
    ImGui::Checkbox("Guardrails", &create_guardrails_);

    ImGui::Spacing();

    // Save button
    if (theme::AccentButton("SAVE AGENT", ImVec2(120, 0))) {
        if (std::strlen(create_name_) > 0) {
            db::AgentConfigRow row;
            row.id = "custom_" + std::to_string(time(nullptr));
            row.name = create_name_;
            row.description = create_desc_;

            // Map category index to id
            int idx = 0;
            for (auto& c : categories) {
                if (c.id == "all") continue;
                if (idx == create_category_) { row.category = c.id; break; }
                idx++;
            }

            AgentConfig config;
            config.provider = create_provider_;
            config.model_id = create_model_;
            config.temperature = create_temperature_;
            config.max_tokens = create_max_tokens_;
            config.instructions = create_instructions_;
            config.enable_memory = create_memory_;
            config.enable_reasoning = create_reasoning_;
            config.enable_guardrails = create_guardrails_;

            row.config_json = config.to_json().dump();
            db::ops::save_agent_config(row);

            // Refresh discovery
            AgentDiscovery::instance().refresh();
            agents_ = AgentDiscovery::instance().get_all_agents();

            create_status_ = "Agent '" + std::string(create_name_) + "' saved successfully";
            create_status_time_ = ImGui::GetTime();

            // Clear form
            std::memset(create_name_, 0, sizeof(create_name_));
            std::memset(create_desc_, 0, sizeof(create_desc_));
            std::memset(create_instructions_, 0, sizeof(create_instructions_));
        } else {
            create_status_ = "Error: Name is required";
            create_status_time_ = ImGui::GetTime();
        }
    }
    ImGui::SameLine();
    if (theme::SecondaryButton("RESET")) {
        std::memset(create_name_, 0, sizeof(create_name_));
        std::memset(create_desc_, 0, sizeof(create_desc_));
        std::memset(create_instructions_, 0, sizeof(create_instructions_));
        create_temperature_ = 0.7f;
        create_max_tokens_ = 4096;
        create_memory_ = false;
        create_reasoning_ = false;
        create_guardrails_ = false;
    }

    // Status message
    if (!create_status_.empty()) {
        double elapsed = ImGui::GetTime() - create_status_time_;
        if (elapsed < 5.0) {
            ImVec4 col = create_status_.find("Error") == 0 ? ERROR_RED : SUCCESS;
            ImGui::TextColored(col, "%s", create_status_.c_str());
        } else {
            create_status_.clear();
        }
    }
}

// ============================================================================
// Tools View
// ============================================================================
void AgentStudioScreen::render_tools_view(float /*width*/, float /*height*/) {
    using namespace theme::colors;

    ImGui::TextColored(ACCENT, "AVAILABLE TOOLS");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 200);
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##tools_search", "Search tools...", tools_search_, sizeof(tools_search_));
    ImGui::Separator();

    auto tools = builtin_tools();
    std::string search_str(tools_search_);
    std::transform(search_str.begin(), search_str.end(), search_str.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });

    // Group by category
    std::string last_cat;
    int total = 0, blocked = 0;

    for (auto& tool : tools) {
        // Filter
        if (!search_str.empty()) {
            std::string name = tool.name;
            std::transform(name.begin(), name.end(), name.begin(),
                           [](unsigned char c) { return (char)std::tolower(c); });
            if (name.find(search_str) == std::string::npos) continue;
        }

        // Category header
        if (tool.category != last_cat) {
            if (!last_cat.empty()) ImGui::Spacing();
            std::string cat_upper = tool.category;
            std::transform(cat_upper.begin(), cat_upper.end(), cat_upper.begin(),
                           [](unsigned char c) { return (char)std::toupper(c); });
            ImGui::TextColored(ACCENT, "%s", cat_upper.c_str());
            ImGui::Separator();
            last_cat = tool.category;
        }

        total++;
        if (tool.blocked) blocked++;

        if (tool.blocked) {
            ImGui::TextColored(ERROR_RED, "  [!] %s", tool.name.c_str());
            ImGui::SameLine(250);
            ImGui::TextColored(ERROR_RED, "BLOCKED");
            ImGui::SameLine(320);
            ImGui::TextColored(TEXT_DIM, "%s", tool.description.c_str());
        } else {
            ImGui::TextColored(SUCCESS, "  [*] %s", tool.name.c_str());
            ImGui::SameLine(250);
            ImGui::TextColored(SUCCESS, "ENABLED");
            ImGui::SameLine(320);
            ImGui::TextColored(TEXT_SECONDARY, "%s", tool.description.c_str());
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(TEXT_DIM, "%d tools available | %d blocked", total, blocked);
}

// ============================================================================
// System View
// ============================================================================
void AgentStudioScreen::render_system_view(float /*width*/, float /*height*/) {
    using namespace theme::colors;

    ImGui::TextColored(ACCENT, "SYSTEM INFORMATION");
    ImGui::Separator();
    ImGui::Spacing();

    // Runtime
    ImGui::TextColored(ACCENT, "RUNTIME");
    ImGui::Separator();

    bool py_avail = python::is_python_available();
    ImGui::TextColored(TEXT_SECONDARY, "Python:");
    ImGui::SameLine(150);
    ImGui::TextColored(py_avail ? SUCCESS : ERROR_RED, "%s",
        py_avail ? "Available" : "Not found");

    ImGui::TextColored(TEXT_SECONDARY, "Agent Framework:");
    ImGui::SameLine(150);
    ImGui::TextColored(SUCCESS, "finagent_core (Agno-based)");

    ImGui::TextColored(TEXT_SECONDARY, "Database:");
    ImGui::SameLine(150);
    ImGui::TextColored(db::Database::instance().is_open() ? SUCCESS : ERROR_RED,
        "%s", db::Database::instance().is_open() ? "Connected" : "Disconnected");

    ImGui::Spacing();
    ImGui::TextColored(ACCENT, "LLM PROVIDERS");
    ImGui::Separator();

    auto configs = db::ops::get_llm_configs();
    if (configs.empty()) {
        ImGui::TextColored(TEXT_DIM, "No providers configured. Go to Settings > LLM Configuration.");
    } else {
        if (ImGui::BeginTable("##llm_providers", 4,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Provider", ImGuiTableColumnFlags_WidthFixed, 120);
            ImGui::TableSetupColumn("Model", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("API Key", ImGuiTableColumnFlags_WidthFixed, 120);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableHeadersRow();

            for (auto& c : configs) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(TEXT_PRIMARY, "%s", c.provider.c_str());
                ImGui::TableNextColumn();
                ImGui::TextColored(TEXT_SECONDARY, "%s", c.model.c_str());
                ImGui::TableNextColumn();
                if (c.api_key && !c.api_key->empty()) {
                    std::string masked = "****" + c.api_key->substr(
                        c.api_key->length() > 4 ? c.api_key->length() - 4 : 0);
                    ImGui::TextColored(TEXT_DIM, "%s", masked.c_str());
                } else {
                    ImGui::TextColored(ERROR_RED, "(not set)");
                }
                ImGui::TableNextColumn();
                if (c.is_active)
                    ImGui::TextColored(SUCCESS, "ACTIVE");
                else if (c.api_key && !c.api_key->empty())
                    ImGui::TextColored(INFO, "READY");
                else
                    ImGui::TextColored(TEXT_DIM, "NOT SET");
            }
            ImGui::EndTable();
        }
    }

    ImGui::Spacing();
    ImGui::TextColored(ACCENT, "AGENT STATISTICS");
    ImGui::Separator();

    auto& disc = AgentDiscovery::instance();
    ImGui::TextColored(TEXT_SECONDARY, "Discovered Agents:");
    ImGui::SameLine(200);
    ImGui::TextColored(TEXT_PRIMARY, "%d", disc.total_count());

    ImGui::TextColored(TEXT_SECONDARY, "Custom Agents:");
    ImGui::SameLine(200);
    ImGui::TextColored(TEXT_PRIMARY, "%d", disc.custom_count());

    ImGui::TextColored(TEXT_SECONDARY, "Categories:");
    ImGui::SameLine(200);
    ImGui::TextColored(TEXT_PRIMARY, "%d", (int)disc.get_categories().size());

    ImGui::TextColored(TEXT_SECONDARY, "Available Tools:");
    ImGui::SameLine(200);
    auto tools = builtin_tools();
    int enabled = 0;
    for (auto& t : tools) { if (!t.blocked) enabled++; }
    ImGui::TextColored(TEXT_PRIMARY, "%d (%d blocked)", enabled, (int)tools.size() - enabled);

    ImGui::Spacing();
    ImGui::TextColored(ACCENT, "CAPABILITIES");
    ImGui::Separator();

    const char* caps[] = {
        "Single agent execution", "Team orchestration",
        "Auto-routing (SuperAgent)", "Execution plans",
        "Direct HTTP to LLM", "Python subprocess",
        "Chat interface", "Custom agent creation"
    };
    for (int i = 0; i < 8; i += 2) {
        ImGui::TextColored(SUCCESS, " [*] %s", caps[i]);
        if (i + 1 < 8) {
            ImGui::SameLine(300);
            ImGui::TextColored(SUCCESS, " [*] %s", caps[i + 1]);
        }
    }
}

// ============================================================================
// Workflows View — bridge to Node Editor
// ============================================================================
void AgentStudioScreen::render_workflows_view(float /*width*/, float /*height*/) {
    using namespace theme::colors;

    ImGui::TextColored(ACCENT, "AGENT WORKFLOWS");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(TEXT_SECONDARY, "Workflows containing agent nodes from the Node Editor:");
    ImGui::Spacing();

    // Load workflows from the node editor workflow store
    auto stored = db::ops::get_setting("nodeEditorWorkflows");
    if (stored.has_value() && !stored->empty()) {
        try {
            auto arr = nlohmann::json::parse(*stored);
            if (arr.is_array()) {
                for (auto& wf : arr) {
                    std::string name = wf.value("name", "Untitled");
                    std::string status = wf.value("status", "draft");

                    // Count agent nodes
                    int agent_count = 0;
                    if (wf.contains("nodes") && wf["nodes"].is_array()) {
                        for (auto& n : wf["nodes"]) {
                            std::string type = n.value("type", "");
                            if (type == "agent-mediator" || type == "agent-node" || type == "multi-agent") {
                                agent_count++;
                            }
                        }
                    }

                    if (agent_count > 0) {
                        ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
                        ImGui::BeginChild(("##wf_" + name).c_str(), ImVec2(-1, 50), ImGuiChildFlags_Borders);

                        ImGui::TextColored(TEXT_PRIMARY, "%s", name.c_str());
                        ImGui::SameLine(300);
                        ImVec4 sc = status == "deployed" ? SUCCESS : WARNING;
                        ImGui::TextColored(sc, "%s", status.c_str());
                        ImGui::SameLine(400);
                        ImGui::TextColored(TEXT_DIM, "%d agent nodes", agent_count);

                        ImGui::EndChild();
                        ImGui::PopStyleColor();
                    }
                }
            }
        } catch (...) {}
    }

    ImGui::Spacing();
    ImGui::TextColored(TEXT_DIM, "Use the Node Editor tab to create and manage workflows with agent nodes.");
}

} // namespace fincept::agent_studio
