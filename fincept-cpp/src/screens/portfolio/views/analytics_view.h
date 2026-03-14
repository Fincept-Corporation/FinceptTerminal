#pragma once
#include "portfolio/portfolio_types.h"
#include "portfolio/portfolio_metrics.h"
#include <string>
#include <nlohmann/json.hpp>

namespace fincept::portfolio {

class AnalyticsView {
public:
    void render(const PortfolioSummary& summary, const ComputedMetrics& metrics);

private:
    int sub_tab_ = 0; // 0=Overview, 1=Analytics, 2=Correlation

    // Cached Python results
    nlohmann::json analytics_result_;
    nlohmann::json correlation_result_;
    bool analytics_loaded_ = false;
    bool correlation_loaded_ = false;
    bool analytics_loading_ = false;
    bool correlation_loading_ = false;
    std::string loaded_for_portfolio_; // cache invalidation key

    void render_overview(const PortfolioSummary& summary, const ComputedMetrics& metrics);
    void render_analytics(const PortfolioSummary& summary);
    void render_correlation(const PortfolioSummary& summary);

    void fetch_analytics_data(const PortfolioSummary& summary);
    void fetch_correlation_data(const PortfolioSummary& summary);
};

} // namespace fincept::portfolio
