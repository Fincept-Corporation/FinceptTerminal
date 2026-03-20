// Planning Panel — Asset allocation, retirement planning, financial goals
// Port of PlanningPanel.tsx

#include "planning_view.h"
#include "python/python_runner.h"
#include "storage/cache_service.h"
#include "storage/database.h"
#include "ui/theme.h"
#include "core/logger.h"
#include <imgui.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <thread>

namespace fincept::portfolio {

static const char* GOAL_CATEGORIES[] = {"Savings", "Investment", "Debt", "Emergency", "Custom"};
static const char* GOAL_CATEGORY_KEYS[] = {"savings", "investment", "debt", "emergency", "custom"};
static constexpr int NUM_GOAL_CATEGORIES = 5;

static const char* GOAL_PRIORITIES[] = {"High", "Medium", "Low"};
static const char* GOAL_PRIORITY_KEYS[] = {"high", "medium", "low"};
static constexpr int NUM_GOAL_PRIORITIES = 3;

// ============================================================================
// Goals — Database setup
// ============================================================================

void PlanningView::ensure_goals_table() {
    auto& dbi = db::Database::instance();
    dbi.exec(R"(
        CREATE TABLE IF NOT EXISTS financial_goals (
            id TEXT PRIMARY KEY,
            portfolio_id TEXT NOT NULL,
            name TEXT NOT NULL,
            category TEXT DEFAULT 'savings',
            target_amount REAL DEFAULT 0.0,
            current_amount REAL DEFAULT 0.0,
            deadline TEXT DEFAULT '',
            priority TEXT DEFAULT 'medium',
            is_completed INTEGER DEFAULT 0,
            created_at TEXT DEFAULT (datetime('now')),
            updated_at TEXT DEFAULT (datetime('now'))
        )
    )");
}

void PlanningView::refresh_goals(const std::string& portfolio_id) {
    ensure_goals_table();
    goals_.clear();

    auto& dbi = db::Database::instance();
    auto stmt = dbi.prepare(
        "SELECT id, portfolio_id, name, category, target_amount, current_amount, "
        "deadline, priority, is_completed, created_at, updated_at "
        "FROM financial_goals WHERE portfolio_id = ? ORDER BY is_completed ASC, priority ASC, deadline ASC");
    stmt.bind_text(1, portfolio_id);

    while (stmt.step()) {
        FinancialGoal g;
        g.id = stmt.col_text(0);
        g.portfolio_id = stmt.col_text(1);
        g.name = stmt.col_text(2);
        g.category = stmt.col_text(3);
        g.target_amount = stmt.col_double(4);
        g.current_amount = stmt.col_double(5);
        g.deadline = stmt.col_text(6);
        g.priority = stmt.col_text(7);
        g.is_completed = stmt.col_int(8) != 0;
        g.created_at = stmt.col_text(9);
        g.updated_at = stmt.col_text(10);
        goals_.push_back(g);
    }

    goals_loaded_ = true;
}

void PlanningView::create_goal(const std::string& portfolio_id) {
    auto& dbi = db::Database::instance();
    std::string id = db::generate_uuid();
    double target = std::atof(goal_target_);
    double current = std::atof(goal_current_);

    auto stmt = dbi.prepare(
        "INSERT INTO financial_goals (id, portfolio_id, name, category, target_amount, "
        "current_amount, deadline, priority) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    stmt.bind_text(1, id);
    stmt.bind_text(2, portfolio_id);
    stmt.bind_text(3, std::string(goal_name_));
    stmt.bind_text(4, std::string(GOAL_CATEGORY_KEYS[goal_category_idx_]));
    stmt.bind_double(5, target);
    stmt.bind_double(6, current);
    stmt.bind_text(7, std::string(goal_deadline_));
    stmt.bind_text(8, std::string(GOAL_PRIORITY_KEYS[goal_priority_idx_]));
    stmt.execute();

    goals_loaded_ = false;
}

void PlanningView::update_goal() {
    auto& dbi = db::Database::instance();
    double target = std::atof(goal_target_);
    double current = std::atof(goal_current_);

    auto stmt = dbi.prepare(
        "UPDATE financial_goals SET name = ?, category = ?, target_amount = ?, "
        "current_amount = ?, deadline = ?, priority = ?, updated_at = datetime('now') WHERE id = ?");
    stmt.bind_text(1, std::string(goal_name_));
    stmt.bind_text(2, std::string(GOAL_CATEGORY_KEYS[goal_category_idx_]));
    stmt.bind_double(3, target);
    stmt.bind_double(4, current);
    stmt.bind_text(5, std::string(goal_deadline_));
    stmt.bind_text(6, std::string(GOAL_PRIORITY_KEYS[goal_priority_idx_]));
    stmt.bind_text(7, edit_goal_id_);
    stmt.execute();

    goals_loaded_ = false;
}

void PlanningView::delete_goal(const std::string& id) {
    auto& dbi = db::Database::instance();
    auto stmt = dbi.prepare("DELETE FROM financial_goals WHERE id = ?");
    stmt.bind_text(1, id);
    stmt.execute();

    goals_loaded_ = false;
}

void PlanningView::toggle_goal_complete(const std::string& id, bool completed) {
    auto& dbi = db::Database::instance();
    auto stmt = dbi.prepare(
        "UPDATE financial_goals SET is_completed = ?, updated_at = datetime('now') WHERE id = ?");
    stmt.bind_int(1, completed ? 1 : 0);
    stmt.bind_text(2, id);
    stmt.execute();

    goals_loaded_ = false;
}

// ============================================================================
// Main render
// ============================================================================

void PlanningView::render(const PortfolioSummary& summary) {
    if (loaded_for_portfolio_ != summary.portfolio.id) {
        allocation_loaded_ = false;
        goals_loaded_ = false;
        loaded_for_portfolio_ = summary.portfolio.id;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    const char* tabs[] = {"Asset Allocation", "Retirement Planner", "Goals"};
    for (int i = 0; i < 3; i++) {
        if (i > 0) ImGui::SameLine();
        bool active = (sub_tab_ == i);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, theme::colors::ACCENT_BG);
        if (ImGui::Button(tabs[i], ImVec2(140, 0))) sub_tab_ = i;
        if (active) ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();
    ImGui::Separator();

    if (!error_.empty()) {
        ImGui::TextColored(theme::colors::MARKET_RED, "[Error] %s", error_.c_str());
        ImGui::Spacing();
    }

    if (sub_tab_ == 0) {
        // Asset allocation from current holdings
        if (!allocation_loaded_ && !loading_) {
            if (theme::AccentButton("Analyze Allocation")) {
                fetch_allocation(summary);
            }
            ImGui::SameLine();
            ImGui::TextColored(theme::colors::TEXT_DIM, "Analyze current allocation vs targets");
        } else if (loading_) {
            theme::LoadingSpinner("Analyzing...");
        } else {
            theme::SectionHeader("Current Allocation by Asset Type");

            // Show allocation breakdown from holdings
            if (ImGui::BeginTable("##alloc", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Symbol");
                ImGui::TableSetupColumn("Weight");
                ImGui::TableSetupColumn("Value");
                ImGui::TableSetupColumn("Bar");
                ImGui::TableHeadersRow();

                for (const auto& h : summary.holdings) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(h.asset.symbol.c_str());
                    ImGui::TableNextColumn(); ImGui::Text("%.1f%%", h.weight);
                    ImGui::TableNextColumn(); ImGui::Text("$%.2f", h.market_value);
                    ImGui::TableNextColumn();
                    // Progress bar
                    float frac = static_cast<float>(h.weight / 100.0);
                    ImGui::ProgressBar(frac, ImVec2(-1, 14), "");
                }
                ImGui::EndTable();
            }

            if (allocation_result_.is_object()) {
                ImGui::Spacing();
                theme::SectionHeader("Allocation Analysis");
                if (ImGui::BeginTable("##alloc_analysis", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
                    ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 250);
                    ImGui::TableSetupColumn("Value");
                    ImGui::TableHeadersRow();
                    for (auto& [k, v] : allocation_result_.items()) {
                        if (v.is_object() || v.is_array()) continue;
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::TextColored(theme::colors::TEXT_DIM, "%s", k.c_str());
                        ImGui::TableNextColumn();
                        if (v.is_number()) ImGui::Text("%.4f", v.get<double>());
                        else ImGui::TextUnformatted(v.dump().c_str());
                    }
                    ImGui::EndTable();
                }
            }

            ImGui::Spacing();
            if (theme::SecondaryButton("Refresh")) {
                allocation_loaded_ = false;
                fetch_allocation(summary);
            }
        }

    } else if (sub_tab_ == 1) {
        // Retirement planner — client-side calculation
        theme::SectionHeader("Retirement Planner");

        ImGui::TextColored(theme::colors::TEXT_DIM, "Current Age"); ImGui::SameLine(200);
        ImGui::SetNextItemWidth(100); ImGui::InputText("##cur_age", current_age_, sizeof(current_age_));

        ImGui::TextColored(theme::colors::TEXT_DIM, "Retirement Age"); ImGui::SameLine(200);
        ImGui::SetNextItemWidth(100); ImGui::InputText("##ret_age", retire_age_, sizeof(retire_age_));

        ImGui::TextColored(theme::colors::TEXT_DIM, "Annual Contribution ($)"); ImGui::SameLine(200);
        ImGui::SetNextItemWidth(100); ImGui::InputText("##annual", annual_contribution_, sizeof(annual_contribution_));

        ImGui::TextColored(theme::colors::TEXT_DIM, "Expected Return (%%)"); ImGui::SameLine(200);
        ImGui::SetNextItemWidth(100); ImGui::InputText("##exp_ret", expected_return_, sizeof(expected_return_));

        ImGui::TextColored(theme::colors::TEXT_DIM, "Inflation (%%)"); ImGui::SameLine(200);
        ImGui::SetNextItemWidth(100); ImGui::InputText("##infl", inflation_, sizeof(inflation_));

        ImGui::Spacing();
        if (theme::AccentButton("Calculate")) {
            int cur = std::atoi(current_age_);
            int ret = std::atoi(retire_age_);
            double annual = std::atof(annual_contribution_);
            double rate = std::atof(expected_return_) / 100.0;
            double infl = std::atof(inflation_) / 100.0;
            double real_rate = rate - infl;
            int years = ret - cur;

            if (years > 0 && annual > 0) {
                double current_portfolio = summary.total_market_value;
                double fv_portfolio = current_portfolio * std::pow(1 + real_rate, years);
                double fv_contributions = annual * ((std::pow(1 + real_rate, years) - 1) / real_rate);
                double total = fv_portfolio + fv_contributions;

                retirement_result_ = nlohmann::json{
                    {"years_to_retirement", years},
                    {"current_portfolio_value", current_portfolio},
                    {"future_portfolio_value", fv_portfolio},
                    {"future_contributions", fv_contributions},
                    {"total_retirement_value", total},
                    {"real_return_rate", real_rate * 100},
                    {"monthly_income_4pct_rule", total * 0.04 / 12}
                };
                retirement_loaded_ = true;
            }
        }

        if (retirement_loaded_ && retirement_result_.is_object()) {
            ImGui::Spacing();
            theme::SectionHeader("Retirement Projection");
            if (ImGui::BeginTable("##retire", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 250);
                ImGui::TableSetupColumn("Value");
                ImGui::TableHeadersRow();

                auto row = [](const char* label, const char* value) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextColored(ImVec4(1, 0.6f, 0, 1), "%s", label);
                    ImGui::TableNextColumn(); ImGui::Text("%s", value);
                };

                char buf[64];
                std::snprintf(buf, sizeof(buf), "%d years", retirement_result_["years_to_retirement"].get<int>());
                row("Years to Retirement", buf);
                std::snprintf(buf, sizeof(buf), "$%.2f", retirement_result_["current_portfolio_value"].get<double>());
                row("Current Portfolio", buf);
                std::snprintf(buf, sizeof(buf), "$%.2f", retirement_result_["future_portfolio_value"].get<double>());
                row("Portfolio Growth", buf);
                std::snprintf(buf, sizeof(buf), "$%.2f", retirement_result_["future_contributions"].get<double>());
                row("Contribution Growth", buf);
                std::snprintf(buf, sizeof(buf), "$%.2f", retirement_result_["total_retirement_value"].get<double>());
                row("Total at Retirement", buf);
                std::snprintf(buf, sizeof(buf), "$%.2f/mo", retirement_result_["monthly_income_4pct_rule"].get<double>());
                row("Monthly Income (4% Rule)", buf);

                ImGui::EndTable();
            }
        }

    } else {
        render_goals(summary);
    }

    render_goal_modal(summary.portfolio.id);
}

// ============================================================================
// Goals tab render
// ============================================================================

void PlanningView::render_goals(const PortfolioSummary& summary) {
    if (!goals_loaded_) {
        refresh_goals(summary.portfolio.id);
    }

    // Header with create button
    theme::SectionHeader("Financial Goals");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100);
    if (theme::AccentButton("+ New Goal")) {
        show_create_goal_ = true;
        show_edit_goal_ = false;
        std::memset(goal_name_, 0, sizeof(goal_name_));
        std::memset(goal_target_, 0, sizeof(goal_target_));
        std::memset(goal_current_, 0, sizeof(goal_current_));
        std::memset(goal_deadline_, 0, sizeof(goal_deadline_));
        goal_category_idx_ = 0;
        goal_priority_idx_ = 1;
    }

    if (goals_.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(theme::colors::TEXT_DIM,
            "No financial goals yet. Click '+ New Goal' to create one.");
        ImGui::Spacing();
        ImGui::TextColored(theme::colors::TEXT_DIM,
            "Track savings targets, investment milestones, debt payoff, and emergency fund goals.");
        return;
    }

    // Summary cards
    int total_goals = static_cast<int>(goals_.size());
    int completed = 0;
    double total_target = 0;
    double total_current = 0;
    for (const auto& g : goals_) {
        if (g.is_completed) completed++;
        total_target += g.target_amount;
        total_current += std::min(g.current_amount, g.target_amount);
    }
    double overall_progress = total_target > 0 ? (total_current / total_target) * 100.0 : 0.0;

    ImGui::Spacing();
    ImGui::BeginChild("##goal_summary", ImVec2(0, 50), ImGuiChildFlags_Borders);
    {
        char buf[64];
        ImGui::BeginGroup();
        ImGui::TextColored(theme::colors::TEXT_DIM, "Goals");
        std::snprintf(buf, sizeof(buf), "%d / %d", completed, total_goals);
        ImGui::TextColored(theme::colors::ACCENT, "%s", buf);
        ImGui::EndGroup();

        ImGui::SameLine(0, 40);
        ImGui::BeginGroup();
        ImGui::TextColored(theme::colors::TEXT_DIM, "Target Total");
        std::snprintf(buf, sizeof(buf), "$%.0f", total_target);
        ImGui::TextColored(theme::colors::TEXT_PRIMARY, "%s", buf);
        ImGui::EndGroup();

        ImGui::SameLine(0, 40);
        ImGui::BeginGroup();
        ImGui::TextColored(theme::colors::TEXT_DIM, "Current Total");
        std::snprintf(buf, sizeof(buf), "$%.0f", total_current);
        ImGui::TextColored(theme::colors::MARKET_GREEN, "%s", buf);
        ImGui::EndGroup();

        ImGui::SameLine(0, 40);
        ImGui::BeginGroup();
        ImGui::TextColored(theme::colors::TEXT_DIM, "Overall Progress");
        std::snprintf(buf, sizeof(buf), "%.1f%%", overall_progress);
        ImVec4 prog_col = overall_progress >= 75 ? theme::colors::MARKET_GREEN
            : overall_progress >= 40 ? ImVec4(1, 0.8f, 0, 1) : theme::colors::MARKET_RED;
        ImGui::TextColored(prog_col, "%s", buf);
        ImGui::EndGroup();

        // Link portfolio value
        ImGui::SameLine(0, 40);
        ImGui::BeginGroup();
        ImGui::TextColored(theme::colors::TEXT_DIM, "Portfolio Value");
        std::snprintf(buf, sizeof(buf), "$%.2f", summary.total_market_value);
        ImGui::TextColored(theme::colors::ACCENT, "%s", buf);
        ImGui::EndGroup();
    }
    ImGui::EndChild();

    ImGui::Spacing();

    // Goals table
    if (ImGui::BeginTable("##goals_table", 9,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
            ImGuiTableFlags_ScrollY, ImVec2(0, ImGui::GetContentRegionAvail().y - 4))) {

        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 24);  // checkbox
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 160);
        ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("Progress", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Deadline", ImGuiTableColumnFlags_WidthFixed, 85);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableHeadersRow();
        ImGui::TableSetupScrollFreeze(0, 1);

        for (const auto& g : goals_) {
            ImGui::TableNextRow();
            ImGui::PushID(g.id.c_str());

            // Checkbox
            ImGui::TableNextColumn();
            bool completed_flag = g.is_completed;
            if (ImGui::Checkbox("##done", &completed_flag)) {
                toggle_goal_complete(g.id, completed_flag);
            }

            // Name (dimmed if completed)
            ImGui::TableNextColumn();
            if (g.is_completed) {
                ImGui::TextColored(theme::colors::TEXT_DIM, "%s", g.name.c_str());
            } else {
                ImGui::TextUnformatted(g.name.c_str());
            }

            // Category with color
            ImGui::TableNextColumn();
            ImVec4 cat_col = theme::colors::TEXT_DIM;
            if (g.category == "savings") cat_col = ImVec4(0.2f, 0.7f, 0.9f, 1.0f);
            else if (g.category == "investment") cat_col = theme::colors::MARKET_GREEN;
            else if (g.category == "debt") cat_col = theme::colors::MARKET_RED;
            else if (g.category == "emergency") cat_col = ImVec4(1.0f, 0.6f, 0.0f, 1.0f);
            ImGui::TextColored(cat_col, "%s", g.category.c_str());

            // Priority
            ImGui::TableNextColumn();
            ImVec4 pri_col = theme::colors::TEXT_DIM;
            if (g.priority == "high") pri_col = theme::colors::MARKET_RED;
            else if (g.priority == "medium") pri_col = ImVec4(1, 0.8f, 0, 1);
            else if (g.priority == "low") pri_col = theme::colors::MARKET_GREEN;
            ImGui::TextColored(pri_col, "%s", g.priority.c_str());

            // Target
            ImGui::TableNextColumn();
            ImGui::Text("$%.0f", g.target_amount);

            // Current
            ImGui::TableNextColumn();
            ImGui::Text("$%.0f", g.current_amount);

            // Progress bar
            ImGui::TableNextColumn();
            float progress = g.target_amount > 0
                ? static_cast<float>(g.current_amount / g.target_amount) : 0.0f;
            if (progress > 1.0f) progress = 1.0f;

            // Color the progress bar
            ImVec4 bar_col = progress >= 1.0f ? theme::colors::MARKET_GREEN
                : progress >= 0.5f ? ImVec4(0.2f, 0.7f, 0.3f, 1.0f)
                : progress >= 0.25f ? ImVec4(1.0f, 0.8f, 0.0f, 1.0f)
                : ImVec4(0.9f, 0.3f, 0.2f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, bar_col);
            char overlay[32];
            std::snprintf(overlay, sizeof(overlay), "%.0f%%", progress * 100);
            ImGui::ProgressBar(progress, ImVec2(-1, 16), overlay);
            ImGui::PopStyleColor();

            // Deadline
            ImGui::TableNextColumn();
            if (!g.deadline.empty()) {
                ImGui::TextColored(theme::colors::TEXT_DIM, "%s", g.deadline.c_str());
            } else {
                ImGui::TextColored(theme::colors::TEXT_DIM, "--");
            }

            // Actions
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("Edit")) {
                show_edit_goal_ = true;
                show_create_goal_ = false;
                edit_goal_id_ = g.id;
                std::strncpy(goal_name_, g.name.c_str(), sizeof(goal_name_) - 1);
                std::snprintf(goal_target_, sizeof(goal_target_), "%.0f", g.target_amount);
                std::snprintf(goal_current_, sizeof(goal_current_), "%.0f", g.current_amount);
                std::strncpy(goal_deadline_, g.deadline.c_str(), sizeof(goal_deadline_) - 1);

                // Find category index
                goal_category_idx_ = 0;
                for (int i = 0; i < NUM_GOAL_CATEGORIES; i++) {
                    if (g.category == GOAL_CATEGORY_KEYS[i]) { goal_category_idx_ = i; break; }
                }
                // Find priority index
                goal_priority_idx_ = 1;
                for (int i = 0; i < NUM_GOAL_PRIORITIES; i++) {
                    if (g.priority == GOAL_PRIORITY_KEYS[i]) { goal_priority_idx_ = i; break; }
                }
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0, 0, 1));
            if (ImGui::SmallButton("X")) {
                delete_goal(g.id);
            }
            ImGui::PopStyleColor();

            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

// ============================================================================
// Goal create/edit modal
// ============================================================================

void PlanningView::render_goal_modal(const std::string& portfolio_id) {
    bool show = show_create_goal_ || show_edit_goal_;
    if (!show) return;

    const char* title = show_create_goal_ ? "Create Financial Goal" : "Edit Financial Goal";
    ImGui::OpenPopup(title);

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420, 320));

    bool open = true;
    if (ImGui::BeginPopupModal(title, &open, ImGuiWindowFlags_NoResize)) {
        ImGui::Text("Name"); ImGui::SameLine(120);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##gname", "e.g. Emergency Fund", goal_name_, sizeof(goal_name_));

        ImGui::Text("Category"); ImGui::SameLine(120);
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##gcat", &goal_category_idx_, GOAL_CATEGORIES, NUM_GOAL_CATEGORIES);

        ImGui::Text("Priority"); ImGui::SameLine(120);
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##gpri", &goal_priority_idx_, GOAL_PRIORITIES, NUM_GOAL_PRIORITIES);

        ImGui::Text("Target ($)"); ImGui::SameLine(120);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##gtarget", "50000", goal_target_, sizeof(goal_target_));

        ImGui::Text("Current ($)"); ImGui::SameLine(120);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##gcurrent", "12000", goal_current_, sizeof(goal_current_));

        ImGui::Text("Deadline"); ImGui::SameLine(120);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##gdeadline", "2027-12-31", goal_deadline_, sizeof(goal_deadline_));

        ImGui::Spacing();
        ImGui::Spacing();

        if (theme::AccentButton(show_create_goal_ ? "Create" : "Save")) {
            if (goal_name_[0] != '\0') {
                if (show_create_goal_) {
                    create_goal(portfolio_id);
                } else {
                    update_goal();
                }
                show_create_goal_ = false;
                show_edit_goal_ = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (theme::SecondaryButton("Cancel")) {
            show_create_goal_ = false;
            show_edit_goal_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (!open) {
        show_create_goal_ = false;
        show_edit_goal_ = false;
    }
}

// ============================================================================
// Allocation fetch (with fixed error handling)
// ============================================================================

void PlanningView::fetch_allocation(const PortfolioSummary& summary) {
    if (loading_) return;
    loading_ = true;
    error_.clear();

    nlohmann::json holdings = nlohmann::json::array();
    for (const auto& h : summary.holdings) {
        holdings.push_back({
            {"symbol", h.asset.symbol},
            {"weight", h.weight / 100.0},
            {"value", h.market_value}
        });
    }
    std::string sym_key;
    for (const auto& h : summary.holdings) sym_key += h.asset.symbol + "_";

    std::thread([this, holdings, sym_key]() {
        try {
            nlohmann::json req;
            req["holdings"] = holdings;

            auto& cache = CacheService::instance();
            std::string ck = CacheService::make_key("reference", "allocation-analysis",
                sym_key.substr(0, std::min((size_t)64, sym_key.size())));

            std::string output;
            auto cached = cache.get(ck);
            if (cached && !cached->empty()) { output = *cached; }
            else {
                auto result = python::execute_with_stdin(
                    "Analytics/portfolioManagement/portfolio_analytics.py",
                    {"allocation_analysis"}, req.dump());
                if (result.success && !result.output.empty()) {
                    output = result.output;
                    auto check = nlohmann::json::parse(output, nullptr, false);
                    if (!check.is_discarded() && !check.contains("error"))
                        cache.set(ck, output, "reference", CacheTTL::FIFTEEN_MIN);
                } else if (!result.error.empty()) {
                    error_ = result.error;
                }
            }

            if (!output.empty()) {
                auto j = nlohmann::json::parse(output, nullptr, false);
                if (!j.is_discarded()) {
                    if (j.contains("error"))
                        error_ = j["error"].get<std::string>();
                    else {
                        allocation_result_ = j;
                        allocation_loaded_ = true;
                    }
                }
            } else if (error_.empty()) {
                error_ = "Script returned empty output";
            }
            // On success with data, allocation_loaded_ is already set above.
            // On error (error_ is non-empty), leave allocation_loaded_ = false so user can retry.
        } catch (const std::exception& e) {
            error_ = e.what();
            LOG_ERROR("PlanningView", "Allocation analysis failed: %s", e.what());
            // Leave allocation_loaded_ = false so the Retry button shows
        }
        loading_ = false;
    }).detach();
}

void PlanningView::fetch_retirement() {
    // Client-side only — no Python needed
}

} // namespace fincept::portfolio
