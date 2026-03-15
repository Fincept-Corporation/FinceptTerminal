#pragma once
#include "portfolio/portfolio_types.h"
#include <string>
#include <nlohmann/json.hpp>

namespace fincept::portfolio {

// QuantStats + FFN View — Quantitative statistics, FFN deep analytics
class QuantStatsView {
public:
    void render(const PortfolioSummary& summary);

private:
    int sub_tab_ = 0; // 0=QuantStats, 1=FFN
    nlohmann::json qs_result_;
    nlohmann::json ffn_result_;
    bool qs_loaded_ = false;
    bool ffn_loaded_ = false;
    bool loading_ = false;
    std::string loaded_for_portfolio_;
    std::string error_;

    void fetch_quantstats(const PortfolioSummary& summary);
    void fetch_ffn(const PortfolioSummary& summary);
    void render_stats_table(const nlohmann::json& data);
};

} // namespace fincept::portfolio
