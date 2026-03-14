#pragma once
#include "portfolio/portfolio_types.h"
#include <string>
#include <nlohmann/json.hpp>

namespace fincept::portfolio {

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

    // Retirement planner inputs
    char current_age_[8] = "30";
    char retire_age_[8] = "65";
    char annual_contribution_[16] = "10000";
    char expected_return_[8] = "7.0";
    char inflation_[8] = "3.0";

    void fetch_allocation(const PortfolioSummary& summary);
    void fetch_retirement();
};

} // namespace fincept::portfolio
