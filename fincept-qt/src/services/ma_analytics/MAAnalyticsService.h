// src/services/ma_analytics/MAAnalyticsService.h
#pragma once
#include "services/ma_analytics/MAAnalyticsTypes.h"

#include <QObject>

#    include "datahub/Producer.h"

namespace fincept::services::ma {

/// Singleton service for all M&A Analytics Python backend calls.
/// Each method is async — results delivered via signals.
class MAAnalyticsService : public QObject
    , public fincept::datahub::Producer
{
    Q_OBJECT
  public:
    static MAAnalyticsService& instance();

    /// Register with the hub + install ma:* policies. Idempotent.
    void ensure_registered_with_hub();

    // ── fincept::datahub::Producer ─────────────────────────────────────────
    QStringList topic_patterns() const override;
    void refresh(const QStringList& topics) override;
    int max_requests_per_sec() const override;

    // ── Valuation ───────────────────────────────────────────────────────────
    void calculate_dcf(const QJsonObject& params);
    void calculate_lbo_returns(const QJsonObject& params);
    void build_lbo_model(const QJsonObject& params);
    void analyze_lbo_debt_schedule(const QJsonObject& params);
    void calculate_lbo_sensitivity(const QJsonObject& params);
    void calculate_trading_comps(const QJsonObject& params);
    void calculate_precedent_transactions(const QJsonObject& params);
    void calculate_dcf_sensitivity(const QJsonObject& params);
    void generate_football_field(const QJsonObject& params);

    // ── Merger Analysis ─────────────────────────────────────────────────────
    void build_merger_model(const QJsonObject& params);
    void calculate_accretion_dilution(const QJsonObject& params);
    void build_pro_forma(const QJsonObject& params);
    void calculate_sources_uses(const QJsonObject& params);
    void analyze_contribution(const QJsonObject& params);
    void calculate_revenue_synergies(const QJsonObject& params);
    void calculate_cost_synergies(const QJsonObject& params);
    void value_synergies_dcf(const QJsonObject& params);

    // ── Deal Structure ──────────────────────────────────────────────────────
    void analyze_payment_structure(const QJsonObject& params);
    void value_earnout(const QJsonObject& params);
    void calculate_exchange_ratio(const QJsonObject& params);
    void analyze_collar_mechanism(const QJsonObject& params);
    void value_cvr(const QJsonObject& params);

    // ── Synergies (additional) ───────────────────────────────────────────────
    void estimate_integration_costs(const QJsonObject& params);

    // ── Deal Database ───────────────────────────────────────────────────────
    void scan_filings(int days_back);
    void get_all_deals();
    void search_deals(const QString& query);
    void create_deal(const QJsonObject& params);
    void update_deal(const QString& deal_id, const QJsonObject& updates);
    void parse_filing(const QString& filing_url, const QString& filing_type);

    // ── Startup Valuation ───────────────────────────────────────────────────
    void calculate_berkus(const QJsonObject& params);
    void calculate_scorecard(const QJsonObject& params);
    void calculate_vc_method(const QJsonObject& params);
    void calculate_first_chicago(const QJsonObject& params);
    void calculate_risk_factor(const QJsonObject& params);
    void calculate_comprehensive_startup(const QJsonObject& params);

    // ── Fairness Opinion ────────────────────────────────────────────────────
    void generate_fairness_opinion(const QJsonObject& params);
    void analyze_premium(const QJsonObject& params);
    void assess_process_quality(const QJsonObject& params);

    // ── Industry Metrics ────────────────────────────────────────────────────
    void calculate_tech_metrics(const QJsonObject& params);
    void calculate_healthcare_metrics(const QJsonObject& params);
    void calculate_financial_services_metrics(const QJsonObject& params);

    // ── Advanced Analytics ──────────────────────────────────────────────────
    void run_monte_carlo(const QJsonObject& params);
    void run_regression(const QJsonObject& params);

    // Financial Statement Analysis
    void analyze_income_statement(const QJsonObject& params);
    void analyze_balance_sheet(const QJsonObject& params);
    void analyze_cashflow_statement(const QJsonObject& params);
    void analyze_comprehensive_financials(const QJsonObject& params);
    void get_financial_key_metrics(const QJsonObject& params);

    // QuantStats Analytics
    void quantstats_stats(const QJsonObject& params);
    void quantstats_returns(const QJsonObject& params);
    void quantstats_drawdown(const QJsonObject& params);
    void quantstats_rolling(const QJsonObject& params);
    void quantstats_full_report(const QJsonObject& params);

    // Statsmodels Analytics
    void fit_arima(const QJsonObject& params);
    void forecast_arima(const QJsonObject& params);
    void fit_ols(const QJsonObject& params);
    void descriptive_statistics(const QJsonObject& params);
    void perform_pca(const QJsonObject& params);
    
    // FFN Analytics
    void calculate_ffn_performance(const QJsonObject& params);
    void calculate_ffn_drawdowns(const QJsonObject& params);
    void calculate_ffn_rolling_metrics(const QJsonObject& params);
    void calculate_ffn_risk_metrics(const QJsonObject& params);
    void optimize_ffn_portfolio(const QJsonObject& params);
    void run_ffn_full_analysis(const QJsonObject& params);

    // Functime Analytics
    void functime_anomaly_detection(const QJsonObject& params);
    void functime_forecast(const QJsonObject& params);
    void functime_seasonality(const QJsonObject& params);
    void functime_metrics(const QJsonObject& params);
    void functime_confidence_intervals(const QJsonObject& params);
    void functime_stationarity(const QJsonObject& params);

    // PyPortfolioOpt Analytics
    void optimize_portfolio(const QJsonObject& params);
    void generate_efficient_frontier(const QJsonObject& params);
    void calculate_discrete_allocation(const QJsonObject& params);
    void run_portfolio_backtest(const QJsonObject& params);
    void calculate_risk_decomposition(const QJsonObject& params);
    void optimize_black_litterman(const QJsonObject& params);
    void optimize_hrp(const QJsonObject& params);
    void generate_portfolio_report(const QJsonObject& params);

    // GS Quant Analytics
    void calculate_risk_metrics(const QJsonObject& params);
    void analyze_portfolio(const QJsonObject& params);
    void calculate_greeks(const QJsonObject& params);
    void perform_var_analysis(const QJsonObject& params);
    void perform_stress_test(const QJsonObject& params);
    void run_gs_backtest(const QJsonObject& params);
    void calculate_statistics(const QJsonObject& params);

    // Fortitudo Tech Analytics
    void fortitudo_check_status(const QJsonObject& params);
    void fortitudo_portfolio_metrics(const QJsonObject& params);
    void fortitudo_covariance_matrix(const QJsonObject& params);
    void fortitudo_mean_variance_optimize(const QJsonObject& params);
    void fortitudo_mean_cvar_optimize(const QJsonObject& params);
    void fortitudo_efficient_frontier(const QJsonObject& params);
    void fortitudo_exp_decay_probabilities(const QJsonObject& params);

    // PyPME Analytics
    void calculate_pme(const QJsonObject& params);
    void calculate_verbose_pme(const QJsonObject& params);
    void calculate_xpme(const QJsonObject& params);
    void calculate_verbose_xpme(const QJsonObject& params);
    void calculate_tessa_xpme(const QJsonObject& params);
    void calculate_tessa_verbose_xpme(const QJsonObject& params);

    // PyVollib Analytics
    void calculate_black_price(const QJsonObject& params);
    void calculate_black_greeks(const QJsonObject& params);
    void calculate_black_iv(const QJsonObject& params);

    void calculate_bs_price(const QJsonObject& params);
    void calculate_bs_greeks(const QJsonObject& params);
    void calculate_bs_iv(const QJsonObject& params);

    void calculate_bsm_price(const QJsonObject& params);
    void calculate_bsm_greeks(const QJsonObject& params);
    void calculate_bsm_iv(const QJsonObject& params);

    // GluonTS Analytics
    void gluonts_check_status(const QJsonObject& params);
    void gluonts_probabilistic_forecast(const QJsonObject& params);
    void gluonts_quantile_forecast(const QJsonObject& params);
    void gluonts_distribution_fit(const QJsonObject& params);
    void gluonts_evaluate_forecast(const QJsonObject& params);
    void gluonts_seasonal_naive(const QJsonObject& params);

    // Fixed Income Analytics
    void run_fixed_income_command(
        const QString& command,
        const QJsonObject& params);

    // skfolio Analytics (portfolio optimization / risk). Contexts are skfolio_-
    // prefixed because several commands (efficient_frontier, risk_metrics,
    // backtest, stress_test, generate_report) collide with other backends.
    void skfolio_optimize(const QJsonObject& params);
    void skfolio_efficient_frontier(const QJsonObject& params);
    void skfolio_risk_metrics(const QJsonObject& params);
    void skfolio_stress_test(const QJsonObject& params);
    void skfolio_backtest(const QJsonObject& params);
    void skfolio_compare_strategies(const QJsonObject& params);
    void skfolio_risk_attribution(const QJsonObject& params);
    void skfolio_hyperparameter_tune(const QJsonObject& params);
    void skfolio_measures(const QJsonObject& params);
    void skfolio_validate_model(const QJsonObject& params);
    void skfolio_scenario_analysis(const QJsonObject& params);
    void skfolio_generate_report(const QJsonObject& params);

    // Options Analytics (each script: command + JSON params → JSON; single op).
    void options_gamma_exposure(const QJsonObject& params);   // gex_calculator.py
    void options_iv_smile(const QJsonObject& params);         // iv_smile.py
    void options_iv_surface(const QJsonObject& params);       // iv_surface.py
    void options_open_interest(const QJsonObject& params);    // oi_tracker.py
    void options_straddle_sim(const QJsonObject& params);     // straddle_simulator.py
    void options_strategy_payoff(const QJsonObject& params);  // strategy_chart.py

    // Corporate-Finance Valuation Summary (valuation_summary.py)
    void valuation_comprehensive(const QJsonObject& params);
    void valuation_executive_summary(const QJsonObject& params);
    void valuation_football_field(const QJsonObject& params);

    // ── Deal Comparison ─────────────────────────────────────────────────────
    void compare_deals(const QJsonObject& params);
    void rank_deals(const QJsonObject& params);
    void benchmark_deal_premium(const QJsonObject& params);
    void analyze_payment_structures(const QJsonObject& params);
    void analyze_industry_deals(const QJsonObject& params);

  signals:
    // Generic result signal — module panels connect to this filtered by context
    void result_ready(QString context, QJsonObject data);
    void error_occurred(QString context, QString message);

  private:
    explicit MAAnalyticsService(QObject* parent = nullptr);
    Q_DISABLE_COPY(MAAnalyticsService)

    /// Run a Python script in the Analytics/corporateFinance/ tree.
    void run_python(const QString& script, const QStringList& args, const QString& context);

    /// Run with JSON payload piped via args
    void run_python_json(const QString& script, const QString& command, const QJsonObject& params,
                         const QString& context);

    static constexpr int kResultTtlSec = 120;

    bool hub_registered_ = false;
};

} // namespace fincept::services::ma
