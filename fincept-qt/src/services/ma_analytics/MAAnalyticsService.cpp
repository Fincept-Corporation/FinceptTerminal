// src/services/ma_analytics/MAAnalyticsService.cpp
#include "services/ma_analytics/MAAnalyticsService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>

namespace fincept::services::ma {

// ── Singleton ────────────────────────────────────────────────────────────────
MAAnalyticsService& MAAnalyticsService::instance() {
    static MAAnalyticsService inst;
    return inst;
}

MAAnalyticsService::MAAnalyticsService(QObject* parent) : QObject(parent) {}

// ── Python helpers ───────────────────────────────────────────────────────────
void MAAnalyticsService::run_python(const QString& script, const QStringList& args,
                                     const QString& context) {
    QPointer<MAAnalyticsService> self = this;
    python::PythonRunner::instance().run(script, args,
        [self, context](python::PythonResult result) {
            if (!self) return;
            if (!result.success) {
                LOG_ERROR("MAAnalytics", QString("Python call failed [%1]: %2")
                    .arg(context, result.error));
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
        });
}

void MAAnalyticsService::run_python_json(const QString& script, const QString& command,
                                          const QJsonObject& params, const QString& context) {
    // Check cache
    auto cache_key = context + "_" + command;
    auto now = QDateTime::currentSecsSinceEpoch();
    auto it = result_cache_.find(cache_key);
    if (it != result_cache_.end() && (now - it->ts) < kResultTtlSec) {
        LOG_DEBUG("MAAnalytics", QString("Cache hit [%1]").arg(context));
        emit result_ready(context, it->data);
        return;
    }

    auto json_str = QJsonDocument(params).toJson(QJsonDocument::Compact);
    QStringList args = {command, json_str};

    QPointer<MAAnalyticsService> self = this;
    python::PythonRunner::instance().run(script, args,
        [self, context, cache_key](python::PythonResult result) {
            if (!self) return;
            if (!result.success) {
                LOG_ERROR("MAAnalytics", QString("Python call failed [%1]: %2")
                    .arg(context, result.error));
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
            // Cache result
            self->result_cache_[cache_key] = {obj, QDateTime::currentSecsSinceEpoch()};
            LOG_INFO("MAAnalytics", QString("Result ready [%1]").arg(context));
            emit self->result_ready(context, obj);
        });
}

// ── Valuation ────────────────────────────────────────────────────────────────
void MAAnalyticsService::calculate_dcf(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/valuation/dcf_model.py",
                    "calculate", params, "dcf");
}

void MAAnalyticsService::calculate_lbo_returns(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/lbo/returns_calculator.py",
                    "calculate", params, "lbo_returns");
}

void MAAnalyticsService::build_lbo_model(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/lbo/lbo_model.py",
                    "build", params, "lbo_model");
}

void MAAnalyticsService::analyze_lbo_debt_schedule(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/lbo/debt_schedule.py",
                    "analyze", params, "lbo_debt_schedule");
}

void MAAnalyticsService::calculate_lbo_sensitivity(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/lbo/lbo_model.py",
                    "sensitivity", params, "lbo_sensitivity");
}

void MAAnalyticsService::calculate_trading_comps(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/valuation/trading_comps.py",
                    "calculate", params, "trading_comps");
}

void MAAnalyticsService::calculate_precedent_transactions(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/valuation/precedent_transactions.py",
                    "calculate", params, "precedent_txns");
}

void MAAnalyticsService::calculate_dcf_sensitivity(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/valuation/dcf_model.py",
                    "sensitivity", params, "dcf_sensitivity");
}

void MAAnalyticsService::generate_football_field(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/valuation/football_field.py",
                    "generate", params, "football_field");
}

// ── Merger Analysis ──────────────────────────────────────────────────────────
void MAAnalyticsService::build_merger_model(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/merger_models/merger_model.py",
                    "build", params, "merger_model");
}

void MAAnalyticsService::calculate_accretion_dilution(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/merger_models/merger_model.py",
                    "accretion_dilution", params, "accretion_dilution");
}

void MAAnalyticsService::build_pro_forma(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/merger_models/merger_model.py",
                    "pro_forma", params, "pro_forma");
}

void MAAnalyticsService::calculate_sources_uses(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/merger_models/sources_uses.py",
                    "calculate", params, "sources_uses");
}

void MAAnalyticsService::analyze_contribution(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/merger_models/contribution_analysis.py",
                    "analyze", params, "contribution");
}

void MAAnalyticsService::calculate_revenue_synergies(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/synergies/revenue_synergies.py",
                    "calculate", params, "revenue_synergies");
}

void MAAnalyticsService::calculate_cost_synergies(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/synergies/cost_synergies.py",
                    "calculate", params, "cost_synergies");
}

void MAAnalyticsService::value_synergies_dcf(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/synergies/synergy_valuation.py",
                    "value", params, "synergy_dcf");
}

// ── Deal Structure ───────────────────────────────────────────────────────────
void MAAnalyticsService::analyze_payment_structure(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/deal_structure/payment_structure.py",
                    "analyze", params, "payment_structure");
}

void MAAnalyticsService::value_earnout(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/deal_structure/earnout_calculator.py",
                    "calculate", params, "earnout");
}

void MAAnalyticsService::calculate_exchange_ratio(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/deal_structure/exchange_ratio.py",
                    "calculate", params, "exchange_ratio");
}

void MAAnalyticsService::analyze_collar_mechanism(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/deal_structure/collar_mechanisms.py",
                    "analyze", params, "collar");
}

void MAAnalyticsService::value_cvr(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/deal_structure/cvr_valuation.py",
                    "calculate", params, "cvr");
}

// ── Deal Database ────────────────────────────────────────────────────────────
void MAAnalyticsService::scan_filings(int days_back) {
    run_python("Analytics/corporateFinance/deal_database/deal_tracker.py",
               {"scan", QString::number(days_back)}, "scan_filings");
}

void MAAnalyticsService::get_all_deals() {
    run_python("Analytics/corporateFinance/deal_database/deal_tracker.py",
               {"list"}, "all_deals");
}

void MAAnalyticsService::search_deals(const QString& query) {
    run_python("Analytics/corporateFinance/deal_database/deal_tracker.py",
               {"search", query}, "search_deals");
}

void MAAnalyticsService::create_deal(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/deal_database/database_schema.py",
                    "create", params, "create_deal");
}

void MAAnalyticsService::update_deal(const QString& deal_id, const QJsonObject& updates) {
    QJsonObject params = updates;
    params["deal_id"] = deal_id;
    run_python_json("Analytics/corporateFinance/deal_database/database_schema.py",
                    "update", params, "update_deal");
}

void MAAnalyticsService::parse_filing(const QString& filing_url, const QString& filing_type) {
    run_python("Analytics/corporateFinance/deal_database/deal_parser.py",
               {"parse", filing_url, filing_type}, "parse_filing");
}

void MAAnalyticsService::estimate_integration_costs(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/synergies/integration_costs.py",
                    "estimate", params, "integration_costs");
}

// ── Startup Valuation ────────────────────────────────────────────────────────
void MAAnalyticsService::calculate_berkus(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/startup_valuation/berkus_method.py",
                    "calculate", params, "berkus");
}

void MAAnalyticsService::calculate_scorecard(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/startup_valuation/scorecard_method.py",
                    "calculate", params, "scorecard");
}

void MAAnalyticsService::calculate_vc_method(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/startup_valuation/vc_method.py",
                    "calculate", params, "vc_method");
}

void MAAnalyticsService::calculate_first_chicago(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/startup_valuation/first_chicago_method.py",
                    "calculate", params, "first_chicago");
}

void MAAnalyticsService::calculate_risk_factor(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/startup_valuation/risk_factor_summation.py",
                    "calculate", params, "risk_factor");
}

void MAAnalyticsService::calculate_comprehensive_startup(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/startup_valuation/startup_summary.py",
                    "comprehensive", params, "startup_comprehensive");
}

// ── Fairness Opinion ─────────────────────────────────────────────────────────
void MAAnalyticsService::generate_fairness_opinion(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/fairness_opinion/valuation_framework.py",
                    "generate", params, "fairness_opinion");
}

void MAAnalyticsService::analyze_premium(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/fairness_opinion/premium_analysis.py",
                    "analyze", params, "premium_analysis");
}

void MAAnalyticsService::assess_process_quality(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/fairness_opinion/process_quality.py",
                    "assess", params, "process_quality");
}

// ── Industry Metrics ─────────────────────────────────────────────────────────
void MAAnalyticsService::calculate_tech_metrics(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/industry_metrics/technology.py",
                    "calculate", params, "tech_metrics");
}

void MAAnalyticsService::calculate_healthcare_metrics(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/industry_metrics/healthcare.py",
                    "calculate", params, "healthcare_metrics");
}

void MAAnalyticsService::calculate_financial_services_metrics(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/industry_metrics/financial_services.py",
                    "calculate", params, "finserv_metrics");
}

// ── Advanced Analytics ───────────────────────────────────────────────────────
void MAAnalyticsService::run_monte_carlo(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/advanced_analytics/monte_carlo_valuation.py",
                    "run", params, "monte_carlo");
}

void MAAnalyticsService::run_regression(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/advanced_analytics/regression_analysis.py",
                    "run", params, "regression");
}

// ── Deal Comparison ──────────────────────────────────────────────────────────
void MAAnalyticsService::compare_deals(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/deal_comparison/deal_comparator.py",
                    "compare", params, "compare_deals");
}

void MAAnalyticsService::rank_deals(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/deal_comparison/deal_comparator.py",
                    "rank", params, "rank_deals");
}

void MAAnalyticsService::benchmark_deal_premium(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/deal_comparison/deal_comparator.py",
                    "benchmark", params, "benchmark_premium");
}

void MAAnalyticsService::analyze_payment_structures(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/deal_comparison/deal_comparator.py",
                    "payment_structures", params, "payment_structures");
}

void MAAnalyticsService::analyze_industry_deals(const QJsonObject& params) {
    run_python_json("Analytics/corporateFinance/deal_comparison/deal_comparator.py",
                    "industry", params, "industry_deals");
}

} // namespace fincept::services::ma
