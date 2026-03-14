#pragma once
// Agent Studio Screen — AI Agent management, execution, and orchestration
// Layout: [Top: View Mode Tabs] [Content: Active View Panel]
// 8 views: Agents, Teams, Chat, Planner, Workflows, Tools, System, Create

#include "agent_types.h"
#include "agent_service.h"
#include "agent_discovery.h"
#include <future>
#include <deque>

namespace fincept::agent_studio {

class AgentStudioScreen {
public:
    void render();

private:
    void init();

    // View renderers
    void render_view_tabs(float width);
    void render_agents_view(float width, float height);
    void render_teams_view(float width, float height);
    void render_chat_view(float width, float height);
    void render_planner_view(float width, float height);
    void render_workflows_view(float width, float height);
    void render_tools_view(float width, float height);
    void render_system_view(float width, float height);
    void render_create_agent_view(float width, float height);

    // Poll async results on main thread
    void poll_futures();

    bool initialized_ = false;

    // Current view
    ViewMode view_mode_ = ViewMode::Agents;

    // ── Agents View State ──
    std::vector<AgentCard> agents_;
    std::string selected_category_ = "all";
    int selected_agent_idx_ = -1;
    char agent_search_[128] = {};
    char test_query_[1024] = {};
    // Editable config for selected agent
    char edit_provider_[64] = {};
    char edit_model_[64] = {};
    float edit_temperature_ = 0.7f;
    int edit_max_tokens_ = 4096;
    char edit_instructions_[4096] = {};
    bool edit_memory_ = false;
    bool edit_reasoning_ = false;
    bool edit_guardrails_ = false;
    bool edit_knowledge_ = false;
    // Execution
    std::future<AgentResult> agent_result_future_;
    AgentResult last_agent_result_;
    bool agent_executing_ = false;

    // ── Chat View State ──
    std::deque<AgentChatMessage> chat_messages_;
    char chat_input_[2048] = {};
    bool use_auto_routing_ = true;
    bool chat_scroll_to_bottom_ = false;
    std::future<AgentResult> chat_result_future_;
    bool chat_executing_ = false;

    // ── Teams View State ──
    TeamConfig team_config_;
    char team_query_[1024] = {};
    std::future<AgentResult> team_result_future_;
    AgentResult last_team_result_;
    bool team_executing_ = false;
    int team_selected_agent_ = -1; // for adding members

    // ── Planner View State ──
    ExecutionPlan current_plan_;
    char planner_query_[1024] = {};
    std::future<ExecutionPlan> plan_future_;
    bool plan_generating_ = false;
    bool plan_executing_ = false;
    std::vector<ExecutionPlan> plan_history_;
    int selected_plan_step_ = -1;

    // ── Create Agent View State ──
    char create_name_[256] = {};
    char create_desc_[512] = {};
    char create_instructions_[4096] = {};
    int create_category_ = 6;    // default: "custom"
    char create_provider_[64] = "openai";
    char create_model_[64] = "gpt-4o";
    float create_temperature_ = 0.7f;
    int create_max_tokens_ = 4096;
    bool create_memory_ = false;
    bool create_reasoning_ = false;
    bool create_guardrails_ = false;
    std::string create_status_;
    double create_status_time_ = 0;

    // ── Tools View State ──
    char tools_search_[128] = {};

    // ── Shared ──
    double last_discovery_time_ = 0;
};

} // namespace fincept::agent_studio
