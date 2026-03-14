// Portfolio Metrics — Risk/return computations
// Port of usePortfolioMetrics.ts

#include "portfolio_metrics.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace fincept::portfolio {

// Helper: compute daily returns from a value series
static std::vector<double> daily_returns(const std::vector<double>& values) {
    std::vector<double> ret;
    ret.reserve(values.size() - 1);
    for (size_t i = 1; i < values.size(); i++) {
        if (values[i - 1] > 0.0)
            ret.push_back((values[i] - values[i - 1]) / values[i - 1]);
        else
            ret.push_back(0.0);
    }
    return ret;
}

static double mean(const std::vector<double>& v) {
    if (v.empty()) return 0.0;
    return std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
}

static double stddev(const std::vector<double>& v) {
    if (v.size() < 2) return 0.0;
    double m = mean(v);
    double sum_sq = 0.0;
    for (double x : v) sum_sq += (x - m) * (x - m);
    return std::sqrt(sum_sq / static_cast<double>(v.size() - 1));
}

static double covariance(const std::vector<double>& a, const std::vector<double>& b) {
    size_t n = std::min(a.size(), b.size());
    if (n < 2) return 0.0;
    double ma = 0, mb = 0;
    for (size_t i = 0; i < n; i++) { ma += a[i]; mb += b[i]; }
    ma /= static_cast<double>(n);
    mb /= static_cast<double>(n);
    double cov = 0;
    for (size_t i = 0; i < n; i++) cov += (a[i] - ma) * (b[i] - mb);
    return cov / static_cast<double>(n - 1);
}

std::vector<double> build_daily_portfolio_values(
    const std::map<std::string, std::vector<std::pair<std::string, double>>>& historical_data,
    const std::map<std::string, double>& weights) {

    if (historical_data.empty() || weights.empty()) return {};

    // Find the minimum series length
    size_t min_len = SIZE_MAX;
    for (const auto& [sym, series] : historical_data) {
        if (weights.count(sym) && !series.empty())
            min_len = std::min(min_len, series.size());
    }
    if (min_len == 0 || min_len == SIZE_MAX) return {};

    // Build weighted portfolio values
    std::vector<double> portfolio_values(min_len, 0.0);
    for (const auto& [sym, series] : historical_data) {
        auto wit = weights.find(sym);
        if (wit == weights.end()) continue;
        double w = wit->second;

        // Normalize: each symbol contributes weight * (price / first_price)
        double base_price = series[0].second;
        if (base_price <= 0.0) continue;

        size_t offset = series.size() - min_len;
        for (size_t i = 0; i < min_len; i++) {
            portfolio_values[i] += w * (series[offset + i].second / base_price);
        }
    }

    // Scale to start at 100
    if (!portfolio_values.empty() && portfolio_values[0] > 0.0) {
        double scale = 100.0 / portfolio_values[0];
        for (auto& v : portfolio_values) v *= scale;
    }

    return portfolio_values;
}

ComputedMetrics compute_metrics(
    const std::map<std::string, std::vector<std::pair<std::string, double>>>& historical_data,
    const std::map<std::string, double>& weights,
    double risk_free_rate) {

    ComputedMetrics m;

    // Concentration (Herfindahl index)
    for (const auto& [sym, w] : weights) {
        m.concentration += w * w;
    }

    auto portfolio_values = build_daily_portfolio_values(historical_data, weights);
    if (portfolio_values.size() < 5) return m;

    auto returns = daily_returns(portfolio_values);
    if (returns.empty()) return m;

    // Volatility (annualized)
    double daily_std = stddev(returns);
    m.volatility = daily_std * std::sqrt(252.0) * 100.0;

    // Sharpe ratio
    double daily_mean = mean(returns);
    double daily_rf = risk_free_rate / 252.0;
    m.sharpe_ratio = (daily_std > 0.0)
        ? (daily_mean - daily_rf) / daily_std * std::sqrt(252.0) : 0.0;

    // Max drawdown
    double peak = portfolio_values[0];
    double max_dd = 0.0;
    for (double v : portfolio_values) {
        if (v > peak) peak = v;
        double dd = (peak - v) / peak;
        if (dd > max_dd) max_dd = dd;
    }
    m.max_drawdown = max_dd * 100.0;

    // VaR 95%
    std::vector<double> sorted_returns = returns;
    std::sort(sorted_returns.begin(), sorted_returns.end());
    size_t var_idx = static_cast<size_t>(0.05 * static_cast<double>(sorted_returns.size()));
    m.var_95 = std::abs(sorted_returns[var_idx]) * std::sqrt(252.0) * 100.0;

    // Beta vs SPY (if SPY data present)
    auto spy_it = historical_data.find("SPY");
    if (spy_it != historical_data.end() && spy_it->second.size() >= 5) {
        std::vector<double> spy_vals;
        for (const auto& [d, c] : spy_it->second)
            spy_vals.push_back(c);

        // Align lengths
        size_t n = std::min(portfolio_values.size(), spy_vals.size());
        std::vector<double> pv_aligned(portfolio_values.end() - static_cast<ptrdiff_t>(n), portfolio_values.end());
        std::vector<double> sv_aligned(spy_vals.end() - static_cast<ptrdiff_t>(n), spy_vals.end());

        auto pr = daily_returns(pv_aligned);
        auto sr = daily_returns(sv_aligned);

        double cov_ps = covariance(pr, sr);
        double var_s = stddev(sr);
        var_s *= var_s; // variance
        m.beta = (var_s > 0.0) ? cov_ps / var_s : 1.0;
    } else {
        m.beta = 1.0;
    }

    // Risk score (0-100): weighted combination
    double vol_score = std::min(m.volatility / 40.0, 1.0) * 30.0;
    double dd_score = std::min(m.max_drawdown / 30.0, 1.0) * 25.0;
    double var_score = std::min(m.var_95 / 25.0, 1.0) * 20.0;
    double conc_score = std::min(m.concentration / 0.5, 1.0) * 15.0;
    double beta_score = std::min(std::abs(m.beta) / 2.0, 1.0) * 10.0;
    m.risk_score = vol_score + dd_score + var_score + conc_score + beta_score;
    m.risk_score = std::min(m.risk_score, 100.0);

    return m;
}

} // namespace fincept::portfolio
