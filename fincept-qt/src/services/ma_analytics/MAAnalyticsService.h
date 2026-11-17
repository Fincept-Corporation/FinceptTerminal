// src/services/ma_analytics/MAAnalyticsService.h
#pragma once
#include "datahub/Producer.h"
#include "services/ma_analytics/MAAnalyticsTypes.h"

#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QStringList>

#include <atomic>
#include <functional>
#include <memory>

namespace fincept::services::ma {

/// Singleton service for the M&A Analytics screen's Python backend calls.
/// Each method is async — results delivered via signals.
class MAAnalyticsService : public QObject, public fincept::datahub::Producer {
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

    // ── Merger Analysis ─────────────────────────────────────────────────────
    void calculate_accretion_dilution(const QJsonObject& params);
    void build_pro_forma(const QJsonObject& params);
    void calculate_sources_uses(const QJsonObject& params);
    void analyze_contribution(const QJsonObject& params);
    void value_synergies_dcf(const QJsonObject& params);

    // ── Deal Structure ──────────────────────────────────────────────────────
    void analyze_payment_structure(const QJsonObject& params);
    void value_earnout(const QJsonObject& params);
    void calculate_exchange_ratio(const QJsonObject& params);
    void analyze_collar_mechanism(const QJsonObject& params);
    void value_cvr(const QJsonObject& params);

    // ── Deal Database ───────────────────────────────────────────────────────
    // The deal database is a Python-side SQLite file (ma_deals.db in FINCEPT_DATA_DIR) owned by
    // scripts/Analytics/corporateFinance/deal_database. Every call returns a request id; the matching signal
    // carries it so a panel can drop a reply that a newer request has superseded.

    /// Search SEC EDGAR for M&A filings of the last `days` days (1-90), store them and extract (unverified)
    /// deals from up to `max_filings` high-confidence filings. One scan at a time; cancellable; 15-minute
    /// budget. Progress arrives on deal_scan_progress.
    quint64 start_deal_scan(int days, int max_filings, const QStringList& forms);
    /// Stop the running scan (filings and deals stored so far are kept).
    void cancel_deal_scan();
    bool deal_scan_running() const { return scan_request_ != 0; }
    /// Request id of the running scan (0 = none) — a panel built mid-scan follows it.
    quint64 deal_scan_request() const { return scan_request_; }
    quint64 load_deals();
    quint64 load_filings();
    /// Insert / update a deal the user entered or edited. Saving marks it verified unless fields has
    /// "verified": false.
    quint64 save_deal(const QJsonObject& fields);
    quint64 set_deal_verified(const QString& deal_id, bool verified);
    quint64 delete_deal(const QString& deal_id);
    /// Read one stored filing and extract its deal (stored as unverified), or a draft the user completes when
    /// the filing fails the quality guards.
    quint64 extract_deal(const QString& accession, const QString& form);

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

    /// Stop the analysis of `context` in flight (a default run whose view the user left): a queued run
    /// never starts, a running script is killed. Its reply then arrives as error_occurred(context,
    /// "Cancelled"). A no-op when nothing of that context runs.
    void cancel(const QString& context);

  signals:
    // Generic result signal — module panels connect to this filtered by context
    void result_ready(QString context, QJsonObject data);
    void error_occurred(QString context, QString message);
    /// A script refused to run because inputs it genuinely needs are missing. `message` is the script's
    /// plain-language explanation; `missing_inputs` are the script's parameter keys.
    void input_required(QString context, QString message, QStringList missing_inputs);

    // ── Deal database replies (request id from the call that started them) ──
    void deal_scan_progress(quint64 request_id, QString message, int done, int total, int filings,
                            int deals_created);
    void deal_scan_finished(quint64 request_id, QJsonObject result);
    /// `cancelled` = the user stopped it; `kept_filings` = filings stored before it ended (last progress).
    void deal_scan_failed(quint64 request_id, QString message, bool cancelled, int kept_filings);
    void deals_loaded(quint64 request_id, QJsonArray deals);
    void filings_loaded(quint64 request_id, QJsonArray filings);
    void deal_saved(quint64 request_id, QJsonObject deal);
    void deal_removed(quint64 request_id, QString deal_id);
    /// Reply of extract_deal: {"status": created|updated|kept_verified|rejected, "data": deal or draft,
    /// "reason": why it was rejected}.
    void deal_extracted(quint64 request_id, QJsonObject reply);
    void deal_op_failed(quint64 request_id, QString message, QStringList missing_inputs);

  private:
    explicit MAAnalyticsService(QObject* parent = nullptr);
    Q_DISABLE_COPY(MAAnalyticsService)

    /// Run `<script> <command> <compact-JSON params>`; successful results are cached for kResultTtlSec
    /// (never when `cacheable` is false, e.g. results read from the user's deal database).
    void run_python_json(const QString& script, const QString& command, const QJsonObject& params,
                         const QString& context, bool cacheable = true);

    /// One deal-database command (deal_tracker.py / deal_parser.py). `on_ok` gets the parsed reply object;
    /// failures go to deal_op_failed. Replies are parsed off the GUI thread.
    quint64 run_deal_db(const QString& script, const QStringList& args, int timeout_ms,
                        std::function<void(quint64, const QJsonObject&)> on_ok);

    static constexpr int kResultTtlSec = 120;
    static constexpr int kDealDbTimeoutMs = 60 * 1000;
    static constexpr int kDealScanTimeoutMs = 15 * 60 * 1000;

    bool hub_registered_ = false;
    /// Cancellation token of the latest run per context (removed when that run answers).
    QHash<QString, std::shared_ptr<std::atomic<bool>>> cancel_tokens_;
    /// Sequence of the latest run per context: a reply of any earlier run of that context is dropped.
    QHash<QString, quint64> context_seq_;
    quint64 next_request_id_ = 0;
    quint64 scan_request_ = 0; // the running scan, 0 = none
    std::shared_ptr<std::atomic<bool>> scan_cancel_;
};

} // namespace fincept::services::ma
