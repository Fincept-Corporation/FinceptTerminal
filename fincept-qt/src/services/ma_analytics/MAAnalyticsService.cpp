// src/services/ma_analytics/MAAnalyticsService.cpp
#include "services/ma_analytics/MAAnalyticsService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"

#    include "datahub/DataHub.h"
#    include "datahub/DataHubMetaTypes.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>

namespace fincept::services::ma {

namespace {
inline void publish_ma_result(bool hub_registered, const QString& context, const QJsonObject& obj) {
    if (!hub_registered) return;
    fincept::datahub::DataHub::instance().publish(QStringLiteral("ma:") + context, QVariant(obj));
}
}  // namespace

// ── Singleton ────────────────────────────────────────────────────────────────
MAAnalyticsService& MAAnalyticsService::instance() {
    static MAAnalyticsService inst;
    return inst;
}

MAAnalyticsService::MAAnalyticsService(QObject* parent) : QObject(parent) {}

// ── Python helpers ───────────────────────────────────────────────────────────
void MAAnalyticsService::run_python(const QString& script, const QStringList& args, const QString& context) {
    QPointer<MAAnalyticsService> self = this;
    python::PythonRunner::instance().run(script, args, [self, context](python::PythonResult result) {
        if (!self)
            return;
        if (!result.success) {
            LOG_ERROR("MAAnalytics", QString("Python call failed [%1]: %2").arg(context, result.error));
            emit self->error_occurred(context, result.error);
            return;
        }
        auto json_str = python::extract_json(result.output);
        auto doc = QJsonDocument::fromJson(json_str.toUtf8());
        if (doc.isNull()) {
            LOG_ERROR("MAAnalytics", QString("Invalid JSON from [%1]").arg(context));
            emit self->error_occurred(context, "Invalid JSON response from Python");
            return;
        }
        auto obj = doc.object();
        LOG_INFO("MAAnalytics", QString("Result ready [%1]").arg(context));
        emit self->result_ready(context, obj);
        publish_ma_result(self->hub_registered_, context, obj);
    });
}

void MAAnalyticsService::run_python_json(const QString& script, const QString& command, const QJsonObject& params,
                                         const QString& context) {
    auto params_json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    const QString cache_key = "ma:" + context + "_" + command + ":" + QString::fromUtf8(params_json);

    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        auto doc = QJsonDocument::fromJson(cached.toString().toUtf8());
        if (!doc.isNull()) {
            LOG_DEBUG("MAAnalytics", QString("Cache hit [%1]").arg(context));
            const auto obj = doc.object();
            emit result_ready(context, obj);
            publish_ma_result(hub_registered_, context, obj);
            return;
        }
    }

    QStringList args = {command, QString::fromUtf8(params_json)};

    QPointer<MAAnalyticsService> self = this;
    python::PythonRunner::instance().run(script, args, [self, context, cache_key](python::PythonResult result) {
        if (!self)
            return;
        if (!result.success) {
            LOG_ERROR("MAAnalytics", QString("Python call failed [%1]: %2").arg(context, result.error));
            emit self->error_occurred(context, result.error);
            return;
        }
        auto json_str = python::extract_json(result.output);
        auto doc = QJsonDocument::fromJson(json_str.toUtf8());
        if (doc.isNull()) {
            LOG_ERROR("MAAnalytics", QString("Invalid JSON from [%1]").arg(context));
            emit self->error_occurred(context, "Invalid JSON response");
            return;
        }
        auto obj = doc.object();
        fincept::CacheManager::instance().put(
            cache_key, QVariant(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))), kResultTtlSec,
            "ma_analytics");
        LOG_INFO("MAAnalytics", QString("Result ready [%1]").arg(context));
        emit self->result_ready(context, obj);
        publish_ma_result(self->hub_registered_, context, obj);
    });
}

// ── Valuation ────────────────────────────────────────────────────────────────
void MAAnalyticsService::calculate_dcf(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/valuation/dcf_model.py", "calculate", params, "dcf");
}

void MAAnalyticsService::calculate_lbo_returns(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/lbo/returns_calculator.py", "calculate", params, "lbo_returns");
}

void MAAnalyticsService::build_lbo_model(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/lbo/lbo_model.py", "build", params, "lbo_model");
}

void MAAnalyticsService::analyze_lbo_debt_schedule(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/lbo/debt_schedule.py", "analyze", params, "lbo_debt_schedule");
}

void MAAnalyticsService::calculate_lbo_sensitivity(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/lbo/lbo_model.py", "sensitivity", params, "lbo_sensitivity");
}

void MAAnalyticsService::calculate_trading_comps(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/valuation/trading_comps.py", "calculate", params, "trading_comps");
}

void MAAnalyticsService::calculate_precedent_transactions(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/valuation/precedent_transactions.py", "calculate", params,
                    "precedent_txns");
}

void MAAnalyticsService::calculate_dcf_sensitivity(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/valuation/dcf_model.py", "sensitivity", params, "dcf_sensitivity");
}

void MAAnalyticsService::generate_football_field(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/valuation/football_field.py", "generate", params, "football_field");
}

// ── Merger Analysis ──────────────────────────────────────────────────────────
void MAAnalyticsService::build_merger_model(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/merger_models/merger_model.py", "build", params, "merger_model");
}

void MAAnalyticsService::calculate_accretion_dilution(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/merger_models/merger_model.py", "accretion_dilution", params,
                    "accretion_dilution");
}

void MAAnalyticsService::build_pro_forma(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/merger_models/merger_model.py", "pro_forma", params, "pro_forma");
}

void MAAnalyticsService::calculate_sources_uses(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/merger_models/sources_uses.py", "calculate", params, "sources_uses");
}

void MAAnalyticsService::analyze_contribution(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/merger_models/contribution_analysis.py", "analyze", params,
                    "contribution");
}

void MAAnalyticsService::calculate_revenue_synergies(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/synergies/revenue_synergies.py", "calculate", params,
                    "revenue_synergies");
}

void MAAnalyticsService::calculate_cost_synergies(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/synergies/cost_synergies.py", "calculate", params, "cost_synergies");
}

void MAAnalyticsService::value_synergies_dcf(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/synergies/synergy_valuation.py", "value", params, "synergy_dcf");
}

// ── Deal Structure ───────────────────────────────────────────────────────────
void MAAnalyticsService::analyze_payment_structure(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/deal_structure/payment_structure.py", "analyze", params,
                    "payment_structure");
}

void MAAnalyticsService::value_earnout(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/deal_structure/earnout_calculator.py", "calculate", params, "earnout");
}

void MAAnalyticsService::calculate_exchange_ratio(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/deal_structure/exchange_ratio.py", "calculate", params,
                    "exchange_ratio");
}

void MAAnalyticsService::analyze_collar_mechanism(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/deal_structure/collar_mechanisms.py", "analyze", params, "collar");
}

void MAAnalyticsService::value_cvr(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/deal_structure/cvr_valuation.py", "calculate", params, "cvr");
}

// ── Deal Database ────────────────────────────────────────────────────────────
void MAAnalyticsService::scan_filings(int days_back) {
    run_python("Analytics/corporateFinance/deal_database/deal_tracker.py", {"scan", QString::number(days_back)},
               "scan_filings");
}

void MAAnalyticsService::get_all_deals() {
    run_python("Analytics/corporateFinance/deal_database/deal_tracker.py", {"list"}, "all_deals");
}

void MAAnalyticsService::search_deals(const QString& query) {
    run_python("Analytics/corporateFinance/deal_database/deal_tracker.py", {"search", query}, "search_deals");
}

void MAAnalyticsService::create_deal(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/deal_database/database_schema.py", "create", params, "create_deal");
}

void MAAnalyticsService::update_deal(const QString& deal_id, const QJsonObject& updates) {
    QJsonObject params = updates;
    params["deal_id"] = deal_id;
    run_python_json("Analytics/corporateFinance/deal_database/database_schema.py", "update", params, "update_deal");
}

void MAAnalyticsService::parse_filing(const QString& filing_url, const QString& filing_type) {
    run_python("Analytics/corporateFinance/deal_database/deal_parser.py", {"parse", filing_url, filing_type},
               "parse_filing");
}

void MAAnalyticsService::estimate_integration_costs(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/synergies/integration_costs.py", "estimate", params,
                    "integration_costs");
}

// ── Startup Valuation ────────────────────────────────────────────────────────
void MAAnalyticsService::calculate_berkus(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/startup_valuation/berkus_method.py", "calculate", params, "berkus");
}

void MAAnalyticsService::calculate_scorecard(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/startup_valuation/scorecard_method.py", "calculate", params,
                    "scorecard");
}

void MAAnalyticsService::calculate_vc_method(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/startup_valuation/vc_method.py", "calculate", params, "vc_method");
}

void MAAnalyticsService::calculate_first_chicago(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/startup_valuation/first_chicago_method.py", "calculate", params,
                    "first_chicago");
}

void MAAnalyticsService::calculate_risk_factor(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/startup_valuation/risk_factor_summation.py", "calculate", params,
                    "risk_factor");
}

void MAAnalyticsService::calculate_comprehensive_startup(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/startup_valuation/startup_summary.py", "comprehensive", params,
                    "startup_comprehensive");
}

// ── Fairness Opinion ─────────────────────────────────────────────────────────
void MAAnalyticsService::generate_fairness_opinion(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/fairness_opinion/valuation_framework.py", "generate", params,
                    "fairness_opinion");
}

void MAAnalyticsService::analyze_premium(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/fairness_opinion/premium_analysis.py", "analyze", params,
                    "premium_analysis");
}

void MAAnalyticsService::assess_process_quality(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/fairness_opinion/process_quality.py", "assess", params,
                    "process_quality");
}

// ── Industry Metrics ─────────────────────────────────────────────────────────
void MAAnalyticsService::calculate_tech_metrics(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/industry_metrics/technology.py", "calculate", params, "tech_metrics");
}

void MAAnalyticsService::calculate_healthcare_metrics(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/industry_metrics/healthcare.py", "calculate", params,
                    "healthcare_metrics");
}

void MAAnalyticsService::calculate_financial_services_metrics(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/industry_metrics/financial_services.py", "calculate", params,
                    "finserv_metrics");
}

// ── Advanced Analytics ───────────────────────────────────────────────────────
void MAAnalyticsService::run_monte_carlo(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/advanced_analytics/monte_carlo_valuation.py", "run", params,
                    "monte_carlo");
}

void MAAnalyticsService::run_regression(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/advanced_analytics/regression_analysis.py", "run", params,
                    "regression");
}

// Financial Statement Analysis
void MAAnalyticsService::analyze_income_statement(const QJsonObject& params) {
    run_python_json(
        "Analytics/financial_analysis_cli.py",
        "analyze_income",
        params,
        "income_analysis");
}

void MAAnalyticsService::analyze_balance_sheet(const QJsonObject& params) {
    run_python_json(
        "Analytics/financial_analysis_cli.py",
        "analyze_balance",
        params,
        "balance_analysis");
}

void MAAnalyticsService::analyze_cashflow_statement(const QJsonObject& params) {
    run_python_json(
        "Analytics/financial_analysis_cli.py",
        "analyze_cashflow",
        params,
        "cashflow_analysis");
}

void MAAnalyticsService::analyze_comprehensive_financials(const QJsonObject& params) {
    run_python_json(
        "Analytics/financial_analysis_cli.py",
        "analyze_comprehensive",
        params,
        "financial_analysis");
}

void MAAnalyticsService::get_financial_key_metrics(const QJsonObject& params) {
    run_python_json(
        "Analytics/financial_analysis_cli.py",
        "get_key_metrics",
        params,
        "financial_key_metrics");
}

// QuantStats Analytics
void MAAnalyticsService::quantstats_stats(const QJsonObject& params) {
    run_python_json(
        "Analytics/quantstats_analytics.py",
        "stats",
        params,
        "quantstats_stats");
}

void MAAnalyticsService::quantstats_returns(const QJsonObject& params) {
    run_python_json(
        "Analytics/quantstats_analytics.py",
        "returns",
        params,
        "quantstats_returns");
}

void MAAnalyticsService::quantstats_drawdown(const QJsonObject& params) {
    run_python_json(
        "Analytics/quantstats_analytics.py",
        "drawdown",
        params,
        "quantstats_drawdown");
}

void MAAnalyticsService::quantstats_rolling(const QJsonObject& params) {
    run_python_json(
        "Analytics/quantstats_analytics.py",
        "rolling",
        params,
        "quantstats_rolling");
}

void MAAnalyticsService::quantstats_full_report(const QJsonObject& params) {
    run_python_json(
        "Analytics/quantstats_analytics.py",
        "full_report",
        params,
        "quantstats_full_report");
}

// Statsmodels Analytics
void MAAnalyticsService::fit_arima(const QJsonObject& params) {
    run_python_json(
        "Analytics/statsmodels_cli.py",
        "arima",
        params,
        "arima");
}

void MAAnalyticsService::forecast_arima(const QJsonObject& params) {
    run_python_json(
        "Analytics/statsmodels_cli.py",
        "arima_forecast",
        params,
        "arima_forecast");
}

void MAAnalyticsService::fit_ols(const QJsonObject& params) {
    run_python_json(
        "Analytics/statsmodels_cli.py",
        "ols",
        params,
        "ols");
}

void MAAnalyticsService::descriptive_statistics(const QJsonObject& params) {
    run_python_json(
        "Analytics/statsmodels_cli.py",
        "descriptive",
        params,
        "descriptive_statistics");
}

// FFN Analytics
void MAAnalyticsService::calculate_ffn_performance(const QJsonObject& params) {
    run_python_json(
        "Analytics/ffn_wrapper/ffn_service.py",
        "calculate_performance",
        params,
        "ffn_performance");
}

void MAAnalyticsService::calculate_ffn_drawdowns(const QJsonObject& params) {
    run_python_json(
        "Analytics/ffn_wrapper/ffn_service.py",
        "calculate_drawdowns",
        params,
        "ffn_drawdowns");
}

void MAAnalyticsService::calculate_ffn_rolling_metrics(const QJsonObject& params) {
    run_python_json(
        "Analytics/ffn_wrapper/ffn_service.py",
        "calculate_rolling_metrics",
        params,
        "ffn_rolling_metrics");
}

void MAAnalyticsService::calculate_ffn_risk_metrics(const QJsonObject& params) {
    run_python_json(
        "Analytics/ffn_wrapper/ffn_service.py",
        "risk_metrics",
        params,
        "ffn_risk_metrics");
}

void MAAnalyticsService::optimize_ffn_portfolio(const QJsonObject& params) {
    run_python_json(
        "Analytics/ffn_wrapper/ffn_service.py",
        "portfolio_optimization",
        params,
        "ffn_portfolio");
}

void MAAnalyticsService::run_ffn_full_analysis(const QJsonObject& params) {
    run_python_json(
        "Analytics/ffn_wrapper/ffn_service.py",
        "full_analysis",
        params,
        "ffn_full_analysis");
}

void MAAnalyticsService::perform_pca(const QJsonObject& params) {
    run_python_json(
        "Analytics/statsmodels_cli.py",
        "pca",
        params,
        "pca");
}

// Functime Analytics
void MAAnalyticsService::functime_forecast(const QJsonObject& params) {
    run_python_json(
        "Analytics/functime_wrapper/functime_service.py",
        "forecast",
        params,
        "functime_forecast");
}

void MAAnalyticsService::functime_anomaly_detection(const QJsonObject& params) {
    run_python_json(
        "Analytics/functime_wrapper/functime_service.py",
        "anomaly_detection",
        params,
        "functime_anomaly_detection");
}

void MAAnalyticsService::functime_seasonality(const QJsonObject& params) {
    run_python_json(
        "Analytics/functime_wrapper/functime_service.py",
        "seasonality",
        params,
        "functime_seasonality");
}

void MAAnalyticsService::functime_metrics(const QJsonObject& params) {
    run_python_json(
        "Analytics/functime_wrapper/functime_service.py",
        "metrics",
        params,
        "functime_metrics");
}

void MAAnalyticsService::functime_confidence_intervals(const QJsonObject& params) {
    run_python_json(
        "Analytics/functime_wrapper/functime_service.py",
        "confidence_intervals",
        params,
        "functime_confidence_intervals");
}

void MAAnalyticsService::functime_stationarity(const QJsonObject& params) {
    run_python_json(
        "Analytics/functime_wrapper/functime_service.py",
        "stationarity",
        params,
        "functime_stationarity");
}

// PyPortfolioOpt Analytics
void MAAnalyticsService::optimize_portfolio(const QJsonObject& params) {
    run_python_json(
        "Analytics/pyportfolioopt_wrapper/pyportfolioopt_service.py",
        "optimize",
        params,
        "portfolio_optimize");
}

void MAAnalyticsService::generate_efficient_frontier(const QJsonObject& params) {
    run_python_json(
        "Analytics/pyportfolioopt_wrapper/pyportfolioopt_service.py",
        "efficient_frontier",
        params,
        "efficient_frontier");
}

void MAAnalyticsService::calculate_discrete_allocation(const QJsonObject& params) {
    run_python_json(
        "Analytics/pyportfolioopt_wrapper/pyportfolioopt_service.py",
        "discrete_allocation",
        params,
        "discrete_allocation");
}

void MAAnalyticsService::run_portfolio_backtest(const QJsonObject& params) {
    run_python_json(
        "Analytics/pyportfolioopt_wrapper/pyportfolioopt_service.py",
        "backtest",
        params,
        "portfolio_backtest");
}

void MAAnalyticsService::calculate_risk_decomposition(const QJsonObject& params) {
    run_python_json(
        "Analytics/pyportfolioopt_wrapper/pyportfolioopt_service.py",
        "risk_decomposition",
        params,
        "risk_decomposition");
}

void MAAnalyticsService::optimize_black_litterman(const QJsonObject& params) {
    run_python_json(
        "Analytics/pyportfolioopt_wrapper/pyportfolioopt_service.py",
        "black_litterman",
        params,
        "black_litterman");
}

void MAAnalyticsService::optimize_hrp(const QJsonObject& params) {
    run_python_json(
        "Analytics/pyportfolioopt_wrapper/pyportfolioopt_service.py",
        "hrp",
        params,
        "hrp_optimization");
}

void MAAnalyticsService::generate_portfolio_report(const QJsonObject& params) {
    run_python_json(
        "Analytics/pyportfolioopt_wrapper/pyportfolioopt_service.py",
        "generate_report",
        params,
        "portfolio_report");
}

// GS Quant Analytics
void MAAnalyticsService::calculate_risk_metrics(const QJsonObject& params) {
    run_python_json(
        "Analytics/gs_quant_wrapper/gs_quant_service.py",
        "risk_metrics",
        params,
        "gs_risk_metrics");
}

void MAAnalyticsService::analyze_portfolio(const QJsonObject& params) {
    run_python_json(
        "Analytics/gs_quant_wrapper/gs_quant_service.py",
        "portfolio_analytics",
        params,
        "gs_portfolio");
}

void MAAnalyticsService::calculate_greeks(const QJsonObject& params) {
    run_python_json(
        "Analytics/gs_quant_wrapper/gs_quant_service.py",
        "greeks",
        params,
        "gs_greeks");
}

void MAAnalyticsService::perform_var_analysis(const QJsonObject& params) {
    run_python_json(
        "Analytics/gs_quant_wrapper/gs_quant_service.py",
        "var_analysis",
        params,
        "gs_var");
}

void MAAnalyticsService::perform_stress_test(const QJsonObject& params) {
    run_python_json(
        "Analytics/gs_quant_wrapper/gs_quant_service.py",
        "stress_test",
        params,
        "gs_stress_test");
}

void MAAnalyticsService::run_gs_backtest(const QJsonObject& params) {
    run_python_json(
        "Analytics/gs_quant_wrapper/gs_quant_service.py",
        "backtest",
        params,
        "gs_backtest");
}

void MAAnalyticsService::calculate_statistics(const QJsonObject& params) {
    run_python_json(
        "Analytics/gs_quant_wrapper/gs_quant_service.py",
        "statistics",
        params,
        "gs_statistics");
}

// Fortitudo Tech Analytics
void MAAnalyticsService::fortitudo_check_status(const QJsonObject& params) {
    run_python_json(
        "Analytics/fortitudo_tech_wrapper/fortitudo_service.py",
        "check_status",
        params,
        "fortitudo_check_status");
}

void MAAnalyticsService::fortitudo_portfolio_metrics(const QJsonObject& params) {
    run_python_json(
        "Analytics/fortitudo_tech_wrapper/fortitudo_service.py",
        "portfolio_metrics",
        params,
        "fortitudo_portfolio_metrics");
}

void MAAnalyticsService::fortitudo_covariance_matrix(const QJsonObject& params) {
    run_python_json(
        "Analytics/fortitudo_tech_wrapper/fortitudo_service.py",
        "covariance_matrix",
        params,
        "fortitudo_covariance_matrix");
}

void MAAnalyticsService::fortitudo_mean_variance_optimize(const QJsonObject& params) {
    run_python_json(
        "Analytics/fortitudo_tech_wrapper/fortitudo_service.py",
        "mean_variance_optimize",
        params,
        "fortitudo_mean_variance_optimize");
}

void MAAnalyticsService::fortitudo_mean_cvar_optimize(const QJsonObject& params) {
    run_python_json(
        "Analytics/fortitudo_tech_wrapper/fortitudo_service.py",
        "mean_cvar_optimize",
        params,
        "fortitudo_mean_cvar_optimize");
}

void MAAnalyticsService::fortitudo_efficient_frontier(const QJsonObject& params) {
    run_python_json(
        "Analytics/fortitudo_tech_wrapper/fortitudo_service.py",
        "efficient_frontier",
        params,
        "fortitudo_efficient_frontier");
}

void MAAnalyticsService::fortitudo_exp_decay_probabilities(const QJsonObject& params) {
    run_python_json(
        "Analytics/fortitudo_tech_wrapper/fortitudo_service.py",
        "exp_decay_probabilities",
        params,
        "fortitudo_exp_decay_probabilities");
}

// PyPME Analytics
void MAAnalyticsService::calculate_pme(const QJsonObject& params) {
    run_python_json(
        "Analytics/pypme_wrapper/pypme_service.py",
        "pme",
        params,
        "pme");
}

void MAAnalyticsService::calculate_verbose_pme(const QJsonObject& params) {
    run_python_json(
        "Analytics/pypme_wrapper/pypme_service.py",
        "verbose_pme",
        params,
        "verbose_pme");
}

void MAAnalyticsService::calculate_xpme(const QJsonObject& params) {
    run_python_json(
        "Analytics/pypme_wrapper/pypme_service.py",
        "xpme",
        params,
        "xpme");
}

void MAAnalyticsService::calculate_verbose_xpme(const QJsonObject& params) {
    run_python_json(
        "Analytics/pypme_wrapper/pypme_service.py",
        "verbose_xpme",
        params,
        "verbose_xpme");
}

void MAAnalyticsService::calculate_tessa_xpme(const QJsonObject& params) {
    run_python_json(
        "Analytics/pypme_wrapper/pypme_service.py",
        "tessa_xpme",
        params,
        "tessa_xpme");
}

void MAAnalyticsService::calculate_tessa_verbose_xpme(const QJsonObject& params) {
    run_python_json(
        "Analytics/pypme_wrapper/pypme_service.py",
        "tessa_verbose_xpme",
        params,
        "tessa_verbose_xpme");
}

// PyVollib Analytics
void MAAnalyticsService::calculate_black_price(const QJsonObject& params) {
    run_python_json("Analytics/py_vollib_wrapper/vollib_service.py",
                    "black_price", params, "black_price");
}

void MAAnalyticsService::calculate_black_greeks(const QJsonObject& params) {
    run_python_json("Analytics/py_vollib_wrapper/vollib_service.py",
                    "black_greeks", params, "black_greeks");
}

void MAAnalyticsService::calculate_black_iv(const QJsonObject& params) {
    run_python_json("Analytics/py_vollib_wrapper/vollib_service.py",
                    "black_iv", params, "black_iv");
}

void MAAnalyticsService::calculate_bs_price(const QJsonObject& params) {
    run_python_json("Analytics/py_vollib_wrapper/vollib_service.py",
                    "bs_price", params, "bs_price");
}

void MAAnalyticsService::calculate_bs_greeks(const QJsonObject& params) {
    run_python_json("Analytics/py_vollib_wrapper/vollib_service.py",
                    "bs_greeks", params, "bs_greeks");
}

void MAAnalyticsService::calculate_bs_iv(const QJsonObject& params) {
    run_python_json("Analytics/py_vollib_wrapper/vollib_service.py",
                    "bs_iv", params, "bs_iv");
}

void MAAnalyticsService::calculate_bsm_price(const QJsonObject& params) {
    run_python_json("Analytics/py_vollib_wrapper/vollib_service.py",
                    "bsm_price", params, "bsm_price");
}

void MAAnalyticsService::calculate_bsm_greeks(const QJsonObject& params) {
    run_python_json("Analytics/py_vollib_wrapper/vollib_service.py",
                    "bsm_greeks", params, "bsm_greeks");
}

void MAAnalyticsService::calculate_bsm_iv(const QJsonObject& params) {
    run_python_json("Analytics/py_vollib_wrapper/vollib_service.py",
                    "bsm_iv", params, "bsm_iv");
}

// GluonTS Analytics
void MAAnalyticsService::gluonts_check_status(const QJsonObject& params) {
    run_python_json(
        "Analytics/gluonts_wrapper/gluonts_service.py",
        "check_status",
        params,
        "gluonts_check_status");
}

void MAAnalyticsService::gluonts_probabilistic_forecast(const QJsonObject& params) {
    run_python_json(
        "Analytics/gluonts_wrapper/gluonts_service.py",
        "probabilistic_forecast",
        params,
        "gluonts_probabilistic_forecast");
}

void MAAnalyticsService::gluonts_quantile_forecast(const QJsonObject& params) {
    run_python_json(
        "Analytics/gluonts_wrapper/gluonts_service.py",
        "quantile_forecast",
        params,
        "gluonts_quantile_forecast");
}

void MAAnalyticsService::gluonts_distribution_fit(const QJsonObject& params) {
    run_python_json(
        "Analytics/gluonts_wrapper/gluonts_service.py",
        "distribution_fit",
        params,
        "gluonts_distribution_fit");
}

void MAAnalyticsService::gluonts_evaluate_forecast(const QJsonObject& params) {
    run_python_json(
        "Analytics/gluonts_wrapper/gluonts_service.py",
        "evaluate_forecast",
        params,
        "gluonts_evaluate_forecast");
}

void MAAnalyticsService::gluonts_seasonal_naive(const QJsonObject& params) {
    run_python_json(
        "Analytics/gluonts_wrapper/gluonts_service.py",
        "seasonal_naive",
        params,
        "gluonts_seasonal_naive");
}

void MAAnalyticsService::run_fixed_income_command(
    const QString& command,
    const QJsonObject& params)
{
    run_python_json(
        "Analytics/fixedIncome/cli.py",
        command,
        params,
        "fixed_income_" + command);
}

// skfolio Analytics — script dispatches on the operation name (args[0]); the
// result context is skfolio_-prefixed so it never collides with the same-named
// commands in PyPortfolioOpt / GS Quant / Fortitudo backends.
static const char* kSkfolioScript = "Analytics/python_skfolio_lib/skfolio_service.py";

void MAAnalyticsService::skfolio_optimize(const QJsonObject& params) {
    run_python_json(kSkfolioScript, "optimize", params, "skfolio_optimize");
}

void MAAnalyticsService::skfolio_efficient_frontier(const QJsonObject& params) {
    run_python_json(kSkfolioScript, "efficient_frontier", params, "skfolio_efficient_frontier");
}

void MAAnalyticsService::skfolio_risk_metrics(const QJsonObject& params) {
    run_python_json(kSkfolioScript, "risk_metrics", params, "skfolio_risk_metrics");
}

void MAAnalyticsService::skfolio_stress_test(const QJsonObject& params) {
    run_python_json(kSkfolioScript, "stress_test", params, "skfolio_stress_test");
}

void MAAnalyticsService::skfolio_backtest(const QJsonObject& params) {
    run_python_json(kSkfolioScript, "backtest", params, "skfolio_backtest");
}

void MAAnalyticsService::skfolio_compare_strategies(const QJsonObject& params) {
    run_python_json(kSkfolioScript, "compare_strategies", params, "skfolio_compare_strategies");
}

void MAAnalyticsService::skfolio_risk_attribution(const QJsonObject& params) {
    run_python_json(kSkfolioScript, "risk_attribution", params, "skfolio_risk_attribution");
}

void MAAnalyticsService::skfolio_hyperparameter_tune(const QJsonObject& params) {
    run_python_json(kSkfolioScript, "hyperparameter_tune", params, "skfolio_hyperparameter_tune");
}

void MAAnalyticsService::skfolio_measures(const QJsonObject& params) {
    run_python_json(kSkfolioScript, "measures", params, "skfolio_measures");
}

void MAAnalyticsService::skfolio_validate_model(const QJsonObject& params) {
    run_python_json(kSkfolioScript, "validate_model", params, "skfolio_validate_model");
}

void MAAnalyticsService::skfolio_scenario_analysis(const QJsonObject& params) {
    run_python_json(kSkfolioScript, "scenario_analysis", params, "skfolio_scenario_analysis");
}

void MAAnalyticsService::skfolio_generate_report(const QJsonObject& params) {
    run_python_json(kSkfolioScript, "generate_report", params, "skfolio_generate_report");
}

// ── Deal Comparison ──────────────────────────────────────────────────────────
void MAAnalyticsService::compare_deals(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/deal_comparison/deal_comparator.py", "compare", params,
                    "compare_deals");
}

void MAAnalyticsService::rank_deals(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/deal_comparison/deal_comparator.py", "rank", params, "rank_deals");
}

void MAAnalyticsService::benchmark_deal_premium(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/deal_comparison/deal_comparator.py", "benchmark", params,
                    "benchmark_premium");
}

void MAAnalyticsService::analyze_payment_structures(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/deal_comparison/deal_comparator.py", "payment_structures", params,
                    "payment_structures");
}

void MAAnalyticsService::analyze_industry_deals(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/deal_comparison/deal_comparator.py", "industry", params,
                    "industry_deals");
}

// ═══════════════════════════════════════════════════════════════════════════════
// DATAHUB PRODUCER — ma:*
// ═══════════════════════════════════════════════════════════════════════════════
//
// All M&A methods funnel through run_python / run_python_json which emit
// `result_ready(context, obj)` and publish to `ma:<context>` in parallel.
// Because every analytic takes a caller-supplied `params` payload, the hub
// can't schedule a refresh without knowing the parameters — so topics are
// push-only. The hub still caches the most recent result for subscribers
// who join late, but won't drive new fetches.

QStringList MAAnalyticsService::topic_patterns() const {
    return {QStringLiteral("ma:*")};
}

void MAAnalyticsService::refresh(const QStringList& topics) {
    // All ma:* topics are push-only (params-dependent). Log advisory only.
    Q_UNUSED(topics);
}

int MAAnalyticsService::max_requests_per_sec() const {
    return 1;  // Heavy Python analytics; callers drive their own cadence.
}

void MAAnalyticsService::ensure_registered_with_hub() {
    if (hub_registered_) return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    fincept::datahub::TopicPolicy policy;
    policy.push_only = true;
    policy.ttl_ms = kResultTtlSec * 1000;  // 2 min cache for latecomers.
    hub.set_policy_pattern(QStringLiteral("ma:*"), policy);

    hub_registered_ = true;
    LOG_INFO("MAAnalyticsService", "Registered with DataHub (ma:*)");
}

} // namespace fincept::services::ma
