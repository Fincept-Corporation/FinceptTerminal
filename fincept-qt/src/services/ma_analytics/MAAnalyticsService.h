// src/services/ma_analytics/MAAnalyticsService.h
#pragma once
#include "services/ma_analytics/MAAnalyticsTypes.h"

#include <QHash>
#include <QObject>

#include <functional>

namespace fincept::services::ma {

/// Singleton service for all M&A Analytics Python backend calls.
/// Each method is async — results delivered via signals.
class MAAnalyticsService : public QObject {
    Q_OBJECT
  public:
    static MAAnalyticsService& instance();

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

    // ── Cache ───────────────────────────────────────────────────────────────
    static constexpr qint64 kResultTtlSec = 120;
    struct CachedResult {
        QJsonObject data;
        qint64 ts = 0;
    };
    QHash<QString, CachedResult> result_cache_;
};

} // namespace fincept::services::ma
