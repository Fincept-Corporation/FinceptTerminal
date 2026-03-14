#pragma once
#include "portfolio/portfolio_types.h"
#include "portfolio/portfolio_metrics.h"
#include <string>
#include <nlohmann/json.hpp>

namespace fincept::portfolio {

class RiskView {
public:
    void render(const PortfolioSummary& summary, const ComputedMetrics& metrics);

private:
    int sub_tab_ = 0; // 0=Overview, 1=Stress&Decay, 2=Fortitudo, 3=Frontiers

    // Cached results per sub-tab
    nlohmann::json overview_result_;
    nlohmann::json stress_result_;
    nlohmann::json fortitudo_result_;
    nlohmann::json frontier_result_;

    bool overview_loaded_ = false;
    bool stress_loaded_ = false;
    bool fortitudo_loaded_ = false;
    bool frontier_loaded_ = false;

    bool loading_ = false;
    std::string loaded_for_portfolio_;
    std::string error_;

    void fetch_overview(const PortfolioSummary& summary);
    void fetch_stress(const PortfolioSummary& summary);
    void fetch_fortitudo(const PortfolioSummary& summary);
    void fetch_frontiers(const PortfolioSummary& summary);

    void render_kv_table(const char* title, const nlohmann::json& data);
    void render_matrix_table(const char* title, const nlohmann::json& data);
    void render_json_section(const char* title, const nlohmann::json& data);
};

} // namespace fincept::portfolio
