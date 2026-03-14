#pragma once
#include "portfolio/portfolio_types.h"
#include <string>
#include <nlohmann/json.hpp>

namespace fincept::portfolio {

// Portfolio Optimization — PyPortfolioOpt integration
class OptimizationView {
public:
    void render(const PortfolioSummary& summary);

private:
    int sub_tab_ = 0;
    // 0=Max Sharpe, 1=Min Vol, 2=Efficient Risk, 3=Efficient Return,
    // 4=Max Quadratic Utility, 5=Risk Parity, 6=Black-Litterman,
    // 7=HRP, 8=Custom Constraints
    static constexpr int NUM_TABS = 9;
    static constexpr const char* TAB_LABELS[] = {
        "Max Sharpe", "Min Vol", "Eff Risk", "Eff Return",
        "Max Utility", "Risk Parity", "Black-Litterman",
        "HRP", "Custom"
    };

    nlohmann::json results_[NUM_TABS];
    bool loaded_[NUM_TABS] = {};
    bool loading_ = false;
    std::string loaded_for_portfolio_;
    std::string error_;

    void fetch_optimization(const PortfolioSummary& summary, int tab);
    void render_result(const nlohmann::json& result);
};

} // namespace fincept::portfolio
