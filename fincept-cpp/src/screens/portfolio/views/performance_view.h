#pragma once
#include "portfolio/portfolio_types.h"
#include "portfolio/portfolio_metrics.h"
#include <string>
#include <vector>
#include <map>

namespace fincept::portfolio {

class PerformanceView {
public:
    void render(const PortfolioSummary& summary, const ComputedMetrics& metrics);

private:
    // Period selection
    int period_idx_ = 1; // 0=1M, 1=3M, 2=6M, 3=1Y, 4=ALL
    static constexpr const char* PERIOD_LABELS[] = {"1M", "3M", "6M", "1Y", "ALL"};
    static constexpr int PERIOD_DAYS[] = {35, 95, 185, 370, 1825};

    // Historical data
    std::vector<double> nav_values_;
    std::vector<double> nav_times_;   // day indices for ImPlot
    std::vector<std::string> nav_dates_;
    bool data_loaded_ = false;
    bool data_loading_ = false;
    std::string loaded_for_portfolio_;
    int loaded_for_period_ = -1;

    // Benchmark data
    std::vector<double> bench_returns_;

    // Computed from historical
    double period_return_ = 0.0;
    double start_nav_ = 0.0;
    double current_nav_ = 0.0;
    int trading_days_ = 0;

    void fetch_historical(const PortfolioSummary& summary);
    void render_period_bar(const ComputedMetrics& metrics);
    void render_nav_chart();
    void render_risk_cards(const ComputedMetrics& metrics);
    void render_daily_returns();
    void render_recommendations(const ComputedMetrics& metrics);
};

} // namespace fincept::portfolio
