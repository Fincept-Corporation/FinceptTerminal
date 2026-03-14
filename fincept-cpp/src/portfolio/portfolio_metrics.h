#pragma once
// Portfolio Metrics — Risk/return computations from historical data
// Port of usePortfolioMetrics.ts (267 lines)

#include "portfolio_types.h"
#include <vector>
#include <string>
#include <map>

namespace fincept::portfolio {

struct ComputedMetrics {
    double sharpe_ratio = 0.0;
    double beta = 0.0;          // vs SPY
    double volatility = 0.0;    // annualized
    double max_drawdown = 0.0;  // percentage
    double var_95 = 0.0;        // 95% Value at Risk
    double risk_score = 0.0;    // 0-100
    double concentration = 0.0; // Herfindahl index
};

// historical_data: symbol -> vector of {date, close} sorted oldest first
// weights: symbol -> portfolio weight (0-1)
ComputedMetrics compute_metrics(
    const std::map<std::string, std::vector<std::pair<std::string, double>>>& historical_data,
    const std::map<std::string, double>& weights,
    double risk_free_rate = 0.04);

// Build daily portfolio value series from per-symbol closes and weights
std::vector<double> build_daily_portfolio_values(
    const std::map<std::string, std::vector<std::pair<std::string, double>>>& historical_data,
    const std::map<std::string, double>& weights);

} // namespace fincept::portfolio
