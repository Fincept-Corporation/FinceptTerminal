#pragma once
// Alpha Arena — AI Trading Competition Platform
// Port of AlphaArenaTab.tsx — LLM models compete in crypto/prediction markets
// Configure agents, run competition cycles, live leaderboard, decisions log

#include <imgui.h>
#include <string>
#include <vector>
#include <future>
#include <nlohmann/json.hpp>

namespace fincept::alpha_arena {

// ============================================================================
// Types
// ============================================================================

struct AgentConfig {
    std::string name;
    std::string provider;   // openai, anthropic, google, deepseek, groq, ollama
    std::string model_id;
    float initial_capital = 10000.0f;
    std::string trading_style; // conservative, balanced, aggressive
    float temperature = 0.7f;
    int max_tokens = 2000;
    bool enabled = true;
};

struct LeaderboardEntry {
    int rank = 0;
    std::string model_name;
    float portfolio_value = 0;
    float total_pnl = 0;
    float return_pct = 0;
    int trades_count = 0;
    float cash = 0;
    int positions_count = 0;
    float win_rate = 0;
    float sharpe_ratio = 0;
};

struct Decision {
    std::string model_name;
    int cycle_number = 0;
    std::string action;     // buy, sell, hold, short
    std::string symbol;
    float quantity = 0;
    float confidence = 0;
    std::string reasoning;
    std::string timestamp;
    float price = 0;
};

struct CompetitionState {
    std::string id;
    std::string status;     // created, running, paused, completed, failed
    int cycle_count = 0;
    std::string mode;       // baseline, monk, situational, max_leverage
    std::vector<std::string> symbols;
    float initial_capital = 10000.0f;
    int cycle_interval = 60;
};

// ============================================================================
// Screen
// ============================================================================

class AlphaArenaScreen {
public:
    void render();

private:
    bool initialized_ = false;
    void init();

    // --- Agents ---
    std::vector<AgentConfig> agents_;
    int editing_agent_idx_ = -1;

    // --- Competition ---
    CompetitionState competition_;
    std::vector<LeaderboardEntry> leaderboard_;
    std::vector<Decision> decisions_;

    // --- Config ---
    int mode_idx_ = 0;                // competition mode
    int symbol_selection_[30] = {};    // selected symbol checkboxes
    float initial_capital_ = 10000.0f;
    int cycle_interval_ = 60;
    int max_cycles_ = 50;
    char comp_name_[128] = "Alpha Arena Competition";

    // --- UI state ---
    enum class Panel { Config, Leaderboard, Decisions, Agents };
    Panel active_panel_ = Panel::Config;
    bool show_add_agent_ = false;

    // Add agent form
    char new_name_[64]     = "GPT-4o";
    int  new_provider_     = 0;
    char new_model_id_[64] = "gpt-4o";
    float new_capital_     = 10000.0f;
    int  new_style_        = 1; // 0=conservative, 1=balanced, 2=aggressive

    // --- Async ---
    enum class AsyncStatus { Idle, Loading, Success, Error };
    AsyncStatus status_ = AsyncStatus::Idle;
    std::string error_;
    std::future<void> async_task_;
    bool is_async_busy() const;
    void check_async();

    // --- Actions ---
    void create_competition();
    void start_competition();
    void pause_competition();
    void stop_competition();
    void run_cycle();
    void refresh_leaderboard();
    void refresh_decisions();
    void add_agent();
    void remove_agent(int idx);

    // --- Render ---
    void render_header(float w);
    void render_panel_tabs(float w);
    void render_config_panel(float w, float h);
    void render_leaderboard_panel(float w, float h);
    void render_decisions_panel(float w, float h);
    void render_agents_panel(float w, float h);
    void render_add_agent_modal();
    void render_competition_controls(float w);
    void render_status_bar(float w);
};

} // namespace fincept::alpha_arena
