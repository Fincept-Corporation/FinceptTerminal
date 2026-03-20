#pragma once
#include "portfolio/portfolio_types.h"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace fincept::portfolio {

// Financial goal stored in SQLite
struct FinancialGoal {
    std::string id;
    std::string portfolio_id;
    std::string name;
    std::string category;    // "savings", "investment", "debt", "emergency", "custom"
    double target_amount = 0.0;
    double current_amount = 0.0;
    std::string deadline;    // YYYY-MM-DD
    std::string priority;    // "high", "medium", "low"
    bool is_completed = false;
    std::string created_at;
    std::string updated_at;
};

// Planning Panel — Asset allocation, retirement planning, goal tracking
class PlanningView {
public:
    void render(const PortfolioSummary& summary);

private:
    int sub_tab_ = 0; // 0=Asset Allocation, 1=Retirement, 2=Goals
    nlohmann::json allocation_result_;
    nlohmann::json retirement_result_;
    bool allocation_loaded_ = false;
    bool retirement_loaded_ = false;
    bool loading_ = false;
    std::string loaded_for_portfolio_;
    std::string error_;

    // Retirement planner inputs
    char current_age_[8] = "30";
    char retire_age_[8] = "65";
    char annual_contribution_[16] = "10000";
    char expected_return_[8] = "7.0";
    char inflation_[8] = "3.0";

    // Goals state
    std::vector<FinancialGoal> goals_;
    bool goals_loaded_ = false;
    bool show_create_goal_ = false;
    bool show_edit_goal_ = false;
    std::string edit_goal_id_;
    char goal_name_[128] = {};
    char goal_target_[32] = {};
    char goal_current_[32] = {};
    char goal_deadline_[16] = {};
    int goal_category_idx_ = 0;
    int goal_priority_idx_ = 1; // default "medium"

    void fetch_allocation(const PortfolioSummary& summary);
    void fetch_retirement();

    // Goals methods
    void ensure_goals_table();
    void refresh_goals(const std::string& portfolio_id);
    void create_goal(const std::string& portfolio_id);
    void update_goal();
    void delete_goal(const std::string& id);
    void toggle_goal_complete(const std::string& id, bool completed);
    void render_goals(const PortfolioSummary& summary);
    void render_goal_modal(const std::string& portfolio_id);
};

} // namespace fincept::portfolio
