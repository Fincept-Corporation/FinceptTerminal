// Alpha Arena — AI Trading Competition Platform
// Port of AlphaArenaTab.tsx — configure agents, create competitions,
// run cycles, live leaderboard. Uses Python alpha_arena service.

#include "alpha_arena_screen.h"
#include "ui/yoga_helpers.h"
#include "ui/theme.h"
#include "python/python_runner.h"
#include "core/logger.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <chrono>
#include <cstdio>

namespace fincept::alpha_arena {

using namespace theme::colors;

// ============================================================================
// Constants
// ============================================================================

static const char* PROVIDERS[] = {"openai", "anthropic", "google", "deepseek", "groq", "ollama"};
static constexpr int PROVIDER_COUNT = 6;

static const char* STYLES[] = {"conservative", "balanced", "aggressive"};
static constexpr int STYLE_COUNT = 3;

static const char* MODES[] = {"baseline", "monk", "situational", "max_leverage"};
static const char* MODE_LABELS[] = {"Baseline", "Monk (Conservative)", "Situational", "Max Leverage"};
static constexpr int MODE_COUNT = 4;

struct SymbolDef { const char* value; const char* label; };
static const SymbolDef SYMBOLS[] = {
    {"BTC/USD", "Bitcoin"},    {"ETH/USD", "Ethereum"},   {"BNB/USD", "BNB"},
    {"SOL/USD", "Solana"},     {"XRP/USD", "Ripple"},     {"ADA/USD", "Cardano"},
    {"DOGE/USD", "Dogecoin"},  {"AVAX/USD", "Avalanche"}, {"DOT/USD", "Polkadot"},
    {"LTC/USD", "Litecoin"},   {"LINK/USD", "Chainlink"}, {"UNI/USD", "Uniswap"},
    {"ATOM/USD", "Cosmos"},    {"NEAR/USD", "NEAR"},      {"APT/USD", "Aptos"},
};
static constexpr int SYMBOL_COUNT = 15;

// Model colors for leaderboard
static ImVec4 model_color(int rank) {
    static const ImVec4 colors[] = {
        {0.06f, 0.73f, 0.51f, 1.0f},  // emerald
        {0.23f, 0.51f, 0.96f, 1.0f},  // blue
        {0.98f, 0.45f, 0.09f, 1.0f},  // orange
        {0.55f, 0.36f, 0.96f, 1.0f},  // purple
        {0.93f, 0.29f, 0.60f, 1.0f},  // pink
        {0.08f, 0.72f, 0.65f, 1.0f},  // teal
        {0.96f, 0.62f, 0.04f, 1.0f},  // amber
    };
    return colors[rank % 7];
}

// ============================================================================
// Async helpers
// ============================================================================

bool AlphaArenaScreen::is_async_busy() const {
    return async_task_.valid() &&
           async_task_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready;
}

void AlphaArenaScreen::check_async() {
    if (async_task_.valid() &&
        async_task_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        try { async_task_.get(); } catch (...) {}
    }
}

// ============================================================================
// Init
// ============================================================================

void AlphaArenaScreen::init() {
    // Default agents
    agents_.push_back({"GPT-4o Mini", "openai", "gpt-4o-mini", 10000, "balanced", 0.7f, 2000, true});
    agents_.push_back({"Claude Sonnet", "anthropic", "claude-sonnet-4-20250514", 10000, "balanced", 0.7f, 2000, true});
    agents_.push_back({"Gemini Flash", "google", "gemini-2.0-flash", 10000, "balanced", 0.7f, 2000, true});

    // Default symbol selection (BTC, ETH, SOL)
    symbol_selection_[0] = 1; // BTC
    symbol_selection_[1] = 1; // ETH
    symbol_selection_[3] = 1; // SOL
}

// ============================================================================
// Actions — call Python alpha_arena service
// ============================================================================

void AlphaArenaScreen::create_competition() {
    if (is_async_busy()) return;
    status_ = AsyncStatus::Loading;
    error_.clear();

    // Build config JSON
    nlohmann::json config;
    config["competition_name"] = comp_name_;
    config["mode"] = MODES[mode_idx_];
    config["initial_capital"] = initial_capital_;
    config["cycle_interval_seconds"] = cycle_interval_;
    config["max_cycles"] = max_cycles_;

    nlohmann::json models = nlohmann::json::array();
    for (auto& a : agents_) {
        if (!a.enabled) continue;
        models.push_back({
            {"name", a.name}, {"provider", a.provider}, {"model_id", a.model_id},
            {"initial_capital", a.initial_capital}, {"trading_style", a.trading_style},
            {"advanced_config", {{"temperature", a.temperature}, {"max_tokens", a.max_tokens}}}
        });
    }
    config["models"] = models;

    nlohmann::json syms = nlohmann::json::array();
    for (int i = 0; i < SYMBOL_COUNT; i++) {
        if (symbol_selection_[i]) syms.push_back(SYMBOLS[i].value);
    }
    config["symbols"] = syms;

    std::string config_str = config.dump();
    async_task_ = std::async(std::launch::async, [this, config_str]() {
        auto result = python::execute("alpha_arena_service.py",
                                       {"create", "--config", config_str});
        if (!result.success) {
            status_ = AsyncStatus::Error;
            error_ = result.error.empty() ? "Failed to create competition" : result.error;
            return;
        }
        try {
            auto j = nlohmann::json::parse(result.output);
            if (j.value("success", false)) {
                competition_.id = j.value("competition_id", "");
                competition_.status = "created";
                competition_.cycle_count = 0;
                competition_.mode = MODES[mode_idx_];
                status_ = AsyncStatus::Success;
            } else {
                status_ = AsyncStatus::Error;
                error_ = j.value("error", "Unknown error");
            }
        } catch (const std::exception& ex) {
            status_ = AsyncStatus::Error;
            error_ = ex.what();
        }
    });
}

void AlphaArenaScreen::start_competition() {
    if (is_async_busy() || competition_.id.empty()) return;
    status_ = AsyncStatus::Loading;
    std::string cid = competition_.id;
    async_task_ = std::async(std::launch::async, [this, cid]() {
        auto result = python::execute("alpha_arena_service.py", {"start", "--id", cid});
        if (result.success) {
            competition_.status = "running";
            status_ = AsyncStatus::Success;
        } else {
            status_ = AsyncStatus::Error;
            error_ = result.error;
        }
    });
}

void AlphaArenaScreen::pause_competition() {
    if (is_async_busy() || competition_.id.empty()) return;
    std::string cid = competition_.id;
    async_task_ = std::async(std::launch::async, [this, cid]() {
        python::execute("alpha_arena_service.py", {"pause", "--id", cid});
        competition_.status = "paused";
    });
}

void AlphaArenaScreen::stop_competition() {
    if (is_async_busy() || competition_.id.empty()) return;
    std::string cid = competition_.id;
    async_task_ = std::async(std::launch::async, [this, cid]() {
        python::execute("alpha_arena_service.py", {"stop", "--id", cid});
        competition_.status = "completed";
    });
}

void AlphaArenaScreen::run_cycle() {
    if (is_async_busy() || competition_.id.empty()) return;
    status_ = AsyncStatus::Loading;
    std::string cid = competition_.id;
    async_task_ = std::async(std::launch::async, [this, cid]() {
        auto result = python::execute("alpha_arena_service.py", {"cycle", "--id", cid});
        if (!result.success) { error_ = result.error; status_ = AsyncStatus::Error; return; }
        try {
            auto j = nlohmann::json::parse(result.output);
            competition_.cycle_count = j.value("cycle_number", competition_.cycle_count + 1);
            // Parse leaderboard if present
            if (j.contains("leaderboard") && j["leaderboard"].is_array()) {
                leaderboard_.clear();
                int rank = 1;
                for (auto& e : j["leaderboard"]) {
                    LeaderboardEntry le;
                    le.rank = rank++;
                    le.model_name = e.value("model_name", "");
                    le.portfolio_value = e.value("portfolio_value", 0.0f);
                    le.total_pnl = e.value("total_pnl", 0.0f);
                    le.return_pct = e.value("return_pct", 0.0f);
                    le.trades_count = e.value("trades_count", 0);
                    le.cash = e.value("cash", 0.0f);
                    le.win_rate = e.value("win_rate", 0.0f);
                    le.sharpe_ratio = e.value("sharpe_ratio", 0.0f);
                    leaderboard_.push_back(std::move(le));
                }
            }
            status_ = AsyncStatus::Success;
        } catch (...) { status_ = AsyncStatus::Error; error_ = "Parse error"; }
    });
}

void AlphaArenaScreen::refresh_leaderboard() {
    if (is_async_busy() || competition_.id.empty()) return;
    std::string cid = competition_.id;
    async_task_ = std::async(std::launch::async, [this, cid]() {
        auto result = python::execute("alpha_arena_service.py", {"leaderboard", "--id", cid});
        if (!result.success) return;
        try {
            auto j = nlohmann::json::parse(result.output);
            if (j.contains("leaderboard") && j["leaderboard"].is_array()) {
                leaderboard_.clear();
                int rank = 1;
                for (auto& e : j["leaderboard"]) {
                    LeaderboardEntry le;
                    le.rank = rank++;
                    le.model_name = e.value("model_name", "");
                    le.portfolio_value = e.value("portfolio_value", 0.0f);
                    le.total_pnl = e.value("total_pnl", 0.0f);
                    le.return_pct = e.value("return_pct", 0.0f);
                    le.trades_count = e.value("trades_count", 0);
                    le.win_rate = e.value("win_rate", 0.0f);
                    le.sharpe_ratio = e.value("sharpe_ratio", 0.0f);
                    leaderboard_.push_back(std::move(le));
                }
            }
        } catch (...) {}
    });
}

void AlphaArenaScreen::refresh_decisions() {
    if (is_async_busy() || competition_.id.empty()) return;
    std::string cid = competition_.id;
    async_task_ = std::async(std::launch::async, [this, cid]() {
        auto result = python::execute("alpha_arena_service.py", {"decisions", "--id", cid});
        if (!result.success) return;
        try {
            auto j = nlohmann::json::parse(result.output);
            if (j.contains("decisions") && j["decisions"].is_array()) {
                decisions_.clear();
                for (auto& d : j["decisions"]) {
                    Decision dec;
                    dec.model_name = d.value("model_name", "");
                    dec.cycle_number = d.value("cycle_number", 0);
                    dec.action = d.value("action", "hold");
                    dec.symbol = d.value("symbol", "");
                    dec.quantity = d.value("quantity", 0.0f);
                    dec.confidence = d.value("confidence", 0.0f);
                    dec.reasoning = d.value("reasoning", "");
                    dec.timestamp = d.value("timestamp", "");
                    dec.price = d.value("price_at_decision", 0.0f);
                    decisions_.push_back(std::move(dec));
                }
            }
        } catch (...) {}
    });
}

void AlphaArenaScreen::add_agent() {
    AgentConfig a;
    a.name = new_name_;
    a.provider = PROVIDERS[std::clamp(new_provider_, 0, PROVIDER_COUNT - 1)];
    a.model_id = new_model_id_;
    a.initial_capital = new_capital_;
    a.trading_style = STYLES[std::clamp(new_style_, 0, STYLE_COUNT - 1)];
    agents_.push_back(std::move(a));
    show_add_agent_ = false;
}

void AlphaArenaScreen::remove_agent(int idx) {
    if (idx >= 0 && idx < (int)agents_.size())
        agents_.erase(agents_.begin() + idx);
}

// ============================================================================
// Main render
// ============================================================================

void AlphaArenaScreen::render() {
    if (!initialized_) { init(); initialized_ = true; }
    check_async();

    ui::ScreenFrame frame("##alpha_arena", ImVec2(0, 0), BG_DARK);
    if (!frame.begin()) { frame.end(); return; }

    float w = frame.width();
    float h = frame.height();

    render_header(w);
    render_competition_controls(w);
    render_panel_tabs(w);

    float content_h = h - 120.0f;
    switch (active_panel_) {
        case Panel::Config:      render_config_panel(w, content_h); break;
        case Panel::Leaderboard: render_leaderboard_panel(w, content_h); break;
        case Panel::Decisions:   render_decisions_panel(w, content_h); break;
        case Panel::Agents:      render_agents_panel(w, content_h); break;
    }

    if (show_add_agent_) render_add_agent_modal();

    render_status_bar(w);
    frame.end();
}

// ============================================================================
// Header
// ============================================================================

void AlphaArenaScreen::render_header(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##aa_header", ImVec2(w, 32), false);

    ImGui::SetCursorPos(ImVec2(8, 6));
    ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
    ImGui::SmallButton("ALPHA ARENA");
    ImGui::PopStyleColor(2);
    ImGui::SameLine();
    ImGui::TextColored(TEXT_SECONDARY, "AI Trading Competition");

    if (!competition_.id.empty()) {
        ImGui::SameLine(0, 20);
        ImVec4 sc = (competition_.status == "running") ? SUCCESS :
                    (competition_.status == "paused") ? WARNING :
                    (competition_.status == "completed") ? INFO : TEXT_DIM;
        ImGui::TextColored(sc, "[%s]", competition_.status.c_str());
        ImGui::SameLine();
        ImGui::TextColored(TEXT_DIM, "Cycle: %d", competition_.cycle_count);
    }

    ImGui::SameLine(w - 160);
    ImGui::TextColored(TEXT_DIM, "%d agents configured", (int)agents_.size());

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Competition controls
// ============================================================================

void AlphaArenaScreen::render_competition_controls(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##aa_controls", ImVec2(w, 30), false);
    ImGui::SetCursorPos(ImVec2(4, 3));

    bool busy = is_async_busy();
    bool has_comp = !competition_.id.empty();
    bool is_running = (competition_.status == "running");

    // Create
    if (!has_comp || competition_.status == "completed" || competition_.status == "failed") {
        ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
        if (ImGui::SmallButton("Create Competition") && !busy)
            create_competition();
        ImGui::PopStyleColor(2);
        ImGui::SameLine(0, 8);
    }

    if (has_comp) {
        // Start
        if (competition_.status == "created" || competition_.status == "paused") {
            ImGui::PushStyleColor(ImGuiCol_Button, SUCCESS);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
            if (ImGui::SmallButton("Start") && !busy) start_competition();
            ImGui::PopStyleColor(2);
            ImGui::SameLine(0, 4);
        }
        // Run Cycle
        if (is_running) {
            if (ImGui::SmallButton("Run Cycle") && !busy) run_cycle();
            ImGui::SameLine(0, 4);
        }
        // Pause
        if (is_running) {
            ImGui::PushStyleColor(ImGuiCol_Button, WARNING);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
            if (ImGui::SmallButton("Pause") && !busy) pause_competition();
            ImGui::PopStyleColor(2);
            ImGui::SameLine(0, 4);
        }
        // Stop
        if (is_running || competition_.status == "paused") {
            ImGui::PushStyleColor(ImGuiCol_Button, ERROR_RED);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
            if (ImGui::SmallButton("Stop") && !busy) stop_competition();
            ImGui::PopStyleColor(2);
            ImGui::SameLine(0, 4);
        }
        // Refresh
        if (ImGui::SmallButton("Refresh") && !busy) {
            refresh_leaderboard();
            refresh_decisions();
        }
    }

    if (busy) {
        ImGui::SameLine(0, 12);
        ImGui::TextColored(WARNING, "Processing...");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Panel tabs
// ============================================================================

void AlphaArenaScreen::render_panel_tabs(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##aa_tabs", ImVec2(w, 26), false);
    ImGui::SetCursorPos(ImVec2(4, 3));

    struct Tab { const char* label; Panel panel; };
    Tab tabs[] = {
        {"Config",      Panel::Config},
        {"Leaderboard", Panel::Leaderboard},
        {"Decisions",   Panel::Decisions},
        {"Agents",      Panel::Agents},
    };

    for (auto& t : tabs) {
        bool active = (active_panel_ == t.panel);
        ImGui::PushStyleColor(ImGuiCol_Button, active ? ACCENT_BG : BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_Text, active ? ACCENT : TEXT_DIM);
        if (ImGui::SmallButton(t.label)) active_panel_ = t.panel;
        ImGui::PopStyleColor(2);
        ImGui::SameLine(0, 4);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Config panel
// ============================================================================

void AlphaArenaScreen::render_config_panel(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##aa_config", ImVec2(w, h), false);

    float col_w = w * 0.5f - 8;

    ImGui::BeginGroup();
    {
        // Left: Competition settings
        ImGui::BeginChild("##aa_cfg_left", ImVec2(col_w, h - 8), true);

        ImGui::TextColored(ACCENT, "Competition Settings");
        ImGui::Separator();

        ImGui::Text("Name:");
        ImGui::SetNextItemWidth(col_w - 20);
        ImGui::InputText("##comp_name", comp_name_, sizeof(comp_name_));

        ImGui::Text("Mode:");
        ImGui::SetNextItemWidth(col_w - 20);
        ImGui::Combo("##comp_mode", &mode_idx_, MODE_LABELS, MODE_COUNT);

        ImGui::Text("Initial Capital ($):");
        ImGui::SetNextItemWidth(col_w - 20);
        ImGui::DragFloat("##init_cap", &initial_capital_, 100, 1000, 1000000, "$%.0f");

        ImGui::Text("Cycle Interval (seconds):");
        ImGui::SetNextItemWidth(col_w - 20);
        ImGui::InputInt("##cycle_int", &cycle_interval_);

        ImGui::Text("Max Cycles:");
        ImGui::SetNextItemWidth(col_w - 20);
        ImGui::InputInt("##max_cyc", &max_cycles_);

        ImGui::EndChild();
    }
    ImGui::EndGroup();

    ImGui::SameLine(0, 4);

    ImGui::BeginGroup();
    {
        // Right: Symbol selection
        ImGui::BeginChild("##aa_cfg_right", ImVec2(col_w, h - 8), true);

        ImGui::TextColored(ACCENT, "Trading Symbols");
        ImGui::Separator();

        for (int i = 0; i < SYMBOL_COUNT; i++) {
            bool checked = symbol_selection_[i] != 0;
            char label[64];
            std::snprintf(label, sizeof(label), "%s (%s)", SYMBOLS[i].label, SYMBOLS[i].value);
            if (ImGui::Checkbox(label, &checked))
                symbol_selection_[i] = checked ? 1 : 0;
        }

        ImGui::EndChild();
    }
    ImGui::EndGroup();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Leaderboard panel
// ============================================================================

void AlphaArenaScreen::render_leaderboard_panel(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##aa_lb", ImVec2(w, h), false);

    if (leaderboard_.empty()) {
        ImGui::SetCursorPos(ImVec2(w / 2 - 120, h / 2));
        ImGui::TextColored(TEXT_DIM, "No leaderboard data. Create and run a competition.");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    ImGui::TextColored(ACCENT, "LEADERBOARD — Cycle %d", competition_.cycle_count);
    ImGui::Separator();

    // Table
    if (ImGui::BeginTable("##lb_table", 8,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
            ImVec2(w - 8, h - 40))) {

        ImGui::TableSetupColumn("Rank", ImGuiTableColumnFlags_WidthFixed, 40);
        ImGui::TableSetupColumn("Model", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Portfolio", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("PnL", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("Return", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Trades", ImGuiTableColumnFlags_WidthFixed, 55);
        ImGui::TableSetupColumn("Win Rate", ImGuiTableColumnFlags_WidthFixed, 65);
        ImGui::TableSetupColumn("Sharpe", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        for (auto& e : leaderboard_) {
            ImGui::TableNextRow();
            ImVec4 col = model_color(e.rank - 1);

            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(col, "#%d", e.rank);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(col, "%s", e.model_name.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("$%.2f", e.portfolio_value);

            ImGui::TableSetColumnIndex(3);
            ImVec4 pnl_col = e.total_pnl >= 0 ? SUCCESS : ERROR_RED;
            ImGui::TextColored(pnl_col, "%s$%.2f", e.total_pnl >= 0 ? "+" : "", e.total_pnl);

            ImGui::TableSetColumnIndex(4);
            ImGui::TextColored(pnl_col, "%+.2f%%", e.return_pct);

            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%d", e.trades_count);

            ImGui::TableSetColumnIndex(6);
            ImGui::Text("%.1f%%", e.win_rate * 100);

            ImGui::TableSetColumnIndex(7);
            ImGui::Text("%.2f", e.sharpe_ratio);
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Decisions panel
// ============================================================================

void AlphaArenaScreen::render_decisions_panel(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##aa_dec", ImVec2(w, h), false);

    if (decisions_.empty()) {
        ImGui::SetCursorPos(ImVec2(w / 2 - 100, h / 2));
        ImGui::TextColored(TEXT_DIM, "No decisions yet. Run competition cycles.");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    ImGui::TextColored(ACCENT, "DECISIONS LOG (%d)", (int)decisions_.size());
    ImGui::Separator();

    for (int i = (int)decisions_.size() - 1; i >= 0 && i >= (int)decisions_.size() - 50; i--) {
        auto& d = decisions_[i];
        ImGui::PushID(i);

        ImVec4 action_col = (d.action == "buy") ? SUCCESS :
                            (d.action == "sell" || d.action == "short") ? ERROR_RED : TEXT_DIM;

        ImGui::TextColored(TEXT_DIM, "[C%d]", d.cycle_number);
        ImGui::SameLine();
        ImGui::TextColored(INFO, "%s", d.model_name.c_str());
        ImGui::SameLine();
        ImGui::TextColored(action_col, "%s", d.action.c_str());
        ImGui::SameLine();
        ImGui::TextColored(TEXT_PRIMARY, "%s", d.symbol.c_str());
        ImGui::SameLine();
        ImGui::TextColored(TEXT_DIM, "qty:%.2f conf:%.0f%%", d.quantity, d.confidence * 100);

        if (!d.reasoning.empty()) {
            ImGui::PushTextWrapPos(w - 16);
            ImGui::TextColored(TEXT_SECONDARY, "  %s", d.reasoning.c_str());
            ImGui::PopTextWrapPos();
        }

        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Agents panel
// ============================================================================

void AlphaArenaScreen::render_agents_panel(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##aa_agents", ImVec2(w, h), false);

    ImGui::TextColored(ACCENT, "CONFIGURED AGENTS (%d)", (int)agents_.size());
    ImGui::SameLine(w - 120);
    ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
    if (ImGui::SmallButton("+ Add Agent")) show_add_agent_ = true;
    ImGui::PopStyleColor(2);
    ImGui::Separator();

    for (int i = 0; i < (int)agents_.size(); i++) {
        auto& a = agents_[i];
        ImGui::PushID(i);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
        ImGui::BeginChild(("##agent_" + std::to_string(i)).c_str(), ImVec2(w - 24, 60), true);

        ImGui::Checkbox("##en", &a.enabled);
        ImGui::SameLine();
        ImGui::TextColored(model_color(i), "%s", a.name.c_str());
        ImGui::SameLine();
        ImGui::TextColored(TEXT_DIM, "(%s / %s)", a.provider.c_str(), a.model_id.c_str());

        ImGui::SameLine(w - 80);
        ImGui::PushStyleColor(ImGuiCol_Button, ERROR_RED);
        if (ImGui::SmallButton("Remove")) remove_agent(i);
        ImGui::PopStyleColor();

        ImGui::TextColored(TEXT_SECONDARY, "Capital: $%.0f | Style: %s | Temp: %.1f",
            a.initial_capital, a.trading_style.c_str(), a.temperature);

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Add agent modal
// ============================================================================

void AlphaArenaScreen::render_add_agent_modal() {
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f - 200,
                                    ImGui::GetIO().DisplaySize.y * 0.5f - 150));
    ImGui::SetNextWindowSize(ImVec2(400, 300));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_WIDGET);

    if (ImGui::Begin("Add Agent", &show_add_agent_,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {

        ImGui::Text("Agent Name:");
        ImGui::SetNextItemWidth(360);
        ImGui::InputText("##ag_name", new_name_, sizeof(new_name_));

        ImGui::Text("Provider:");
        ImGui::SetNextItemWidth(360);
        ImGui::Combo("##ag_prov", &new_provider_, PROVIDERS, PROVIDER_COUNT);

        ImGui::Text("Model ID:");
        ImGui::SetNextItemWidth(360);
        ImGui::InputText("##ag_model", new_model_id_, sizeof(new_model_id_));

        ImGui::Text("Initial Capital ($):");
        ImGui::SetNextItemWidth(200);
        ImGui::DragFloat("##ag_cap", &new_capital_, 100, 1000, 1000000, "$%.0f");

        ImGui::Text("Trading Style:");
        ImGui::SetNextItemWidth(200);
        ImGui::Combo("##ag_style", &new_style_, STYLES, STYLE_COUNT);

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
        if (ImGui::Button("Add Agent", ImVec2(120, 28))) add_agent();
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 28))) show_add_agent_ = false;
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

// ============================================================================
// Status bar
// ============================================================================

void AlphaArenaScreen::render_status_bar(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##aa_status", ImVec2(w, 24), false);
    ImGui::SetCursorPos(ImVec2(8, 4));

    if (is_async_busy()) {
        ImGui::TextColored(WARNING, "Processing...");
    } else if (status_ == AsyncStatus::Error) {
        ImGui::TextColored(ERROR_RED, "Error: %s", error_.c_str());
    } else if (!competition_.id.empty()) {
        ImGui::TextColored(SUCCESS, "Competition: %s", competition_.id.c_str());
        ImGui::SameLine();
        ImGui::TextColored(TEXT_DIM, "| Status: %s | Cycles: %d | Agents: %d",
            competition_.status.c_str(), competition_.cycle_count, (int)agents_.size());
    } else {
        ImGui::TextColored(TEXT_DIM, "Configure agents and create a competition to begin");
    }

    ImGui::SameLine(w - 200);
    ImGui::TextColored(TEXT_DIM, "Alpha Arena v2.0 | Agno Framework");

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace fincept::alpha_arena
