// src/services/ma_analytics/MAAnalyticsService.cpp
#include "services/ma_analytics/MAAnalyticsService.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"

#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace fincept::services::ma {

namespace {
inline void publish_ma_result(bool hub_registered, const QString& context, const QJsonObject& obj) {
    if (!hub_registered)
        return;
    fincept::datahub::DataHub::instance().publish(QStringLiteral("ma:") + context, QVariant(obj));
}

const QString kMaDealTracker = QStringLiteral("Analytics/corporateFinance/deal_database/deal_tracker.py");
const QString kMaDealParser = QStringLiteral("Analytics/corporateFinance/deal_database/deal_parser.py");

/// The `missing_inputs` keys a failed script named in its reply (empty when it named none).
QStringList ma_missing_inputs(const QString& output) {
    const QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8());
    QStringList keys;
    for (const QJsonValue& v : doc.object().value(QStringLiteral("missing_inputs")).toArray())
        if (v.isString())
            keys << v.toString();
    return keys;
}

/// Parse a reply off the GUI thread (the deal list and a scan's filing list grow with the database), then
/// call `done` on `owner`'s thread. Nothing is called if `owner` is gone.
void ma_parse_reply_async(QObject* owner, QString text, std::function<void(QJsonObject, bool)> done) {
    auto* watcher = new QFutureWatcher<QJsonDocument>(owner);
    QObject::connect(watcher, &QFutureWatcher<QJsonDocument>::finished, owner,
                     [watcher, done = std::move(done)]() {
                         const QJsonDocument doc = watcher->result();
                         watcher->deleteLater();
                         done(doc.object(), doc.isObject());
                     });
    watcher->setFuture(QtConcurrent::run([text = std::move(text)]() { return QJsonDocument::fromJson(text.toUtf8()); }));
}
} // namespace

// ── Singleton ────────────────────────────────────────────────────────────────
MAAnalyticsService& MAAnalyticsService::instance() {
    static MAAnalyticsService inst;
    return inst;
}

MAAnalyticsService::MAAnalyticsService(QObject* parent) : QObject(parent) {}

// ── Python helpers ───────────────────────────────────────────────────────────
void MAAnalyticsService::run_python_json(const QString& script, const QString& command, const QJsonObject& params,
                                         const QString& context, bool cacheable) {
    auto params_json = QJsonDocument(params).toJson(QJsonDocument::Compact);
    const QString cache_key = "ma:" + context + "_" + command + ":" + QString::fromUtf8(params_json);

    // The newest run of a context supersedes every earlier one: the earlier script is stopped and, should its
    // reply still arrive (it finished before the cancel landed), the reply is dropped below. Replies carry no
    // request id of their own, so without this a slow earlier run could be drawn as the result of the newer one.
    const quint64 seq = ++next_request_id_;
    if (const auto previous = cancel_tokens_.take(context))
        previous->store(true);
    context_seq_.insert(context, seq);

    const QVariant cached = cacheable ? fincept::CacheManager::instance().get(cache_key) : QVariant();
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

    // Cancellable (cancel(context)): a default run whose view was left is stopped, not left to finish.
    auto token = std::make_shared<std::atomic<bool>>(false);
    cancel_tokens_.insert(context, token);
    python::PythonRunner::RunOptions opts;
    opts.cancel_token = token;

    QPointer<MAAnalyticsService> self = this;
    python::PythonRunner::instance().run_with_options(script, args, opts, [self, context, cache_key, cacheable, token,
                                                                           seq](python::PythonResult result) {
        if (!self)
            return;
        if (self->cancel_tokens_.value(context) == token)
            self->cancel_tokens_.remove(context);
        if (self->context_seq_.value(context) != seq) {
            LOG_DEBUG("MAAnalytics", QString("Superseded reply dropped context=%1").arg(context));
            return; // a newer run of this analysis was started
        }
        if (!result.success && result.error == QLatin1String("Cancelled")) {
            // cancel(context): the screen stopped a default run whose view was left. Not a failure.
            LOG_DEBUG("MAAnalytics", QString("Cancelled [%1]").arg(context));
            emit self->error_occurred(context, result.error);
            return;
        }
        if (!result.success) {
            // A script that names the inputs it needs is asking for them, not failing.
            const QStringList missing = ma_missing_inputs(result.output);
            if (!missing.isEmpty()) {
                LOG_INFO("MAAnalytics", QString("Inputs required [%1]: %2").arg(context, missing.join(',')));
                emit self->input_required(context, result.error, missing);
                return;
            }
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

        // A payload that says it was not successful is still delivered (the panel shows it as an error) but is
        // never cached, or a transient failure would be replayed for the whole TTL. Neither is a "needs data"
        // empty state: the user is about to add the data it asks for.
        const QJsonValue success = obj.value(QStringLiteral("success"));
        const bool reported_failure = success.isBool() && !success.toBool();
        if (cacheable && !reported_failure && !obj.contains(QStringLiteral("needs_data"))) {
            fincept::CacheManager::instance().put(
                cache_key, QVariant(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))),
                kResultTtlSec, "ma_analytics");
        }
        LOG_DEBUG("MAAnalytics", QString("Result ready [%1]").arg(context));
        emit self->result_ready(context, obj);
        publish_ma_result(self->hub_registered_, context, obj);
    });
}

void MAAnalyticsService::cancel(const QString& context) {
    const auto token = cancel_tokens_.take(context);
    if (token)
        token->store(true);
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
    // Without pasted comparables the script reads the user's verified deals, which change between runs.
    const bool own_comps = !params.value(QStringLiteral("comparables")).toArray().isEmpty();
    run_python_json("Analytics/corporateFinance/valuation/precedent_transactions.py", "calculate", params,
                    "precedent_txns", own_comps);
}

// ── Merger Analysis ──────────────────────────────────────────────────────────
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
quint64 MAAnalyticsService::run_deal_db(const QString& script, const QStringList& args, int timeout_ms,
                                        std::function<void(quint64, const QJsonObject&)> on_ok) {
    const quint64 id = ++next_request_id_;
    python::PythonRunner::RunOptions opts;
    opts.timeout_ms = timeout_ms;
    QPointer<MAAnalyticsService> self = this;
    python::PythonRunner::instance().run_with_options(
        script, args, opts, [self, id, on_ok = std::move(on_ok)](python::PythonResult result) {
            if (!self)
                return;
            if (!result.success) {
                LOG_WARN("MAAnalytics", QString("Deal database call %1 failed: %2").arg(id).arg(result.error));
                emit self->deal_op_failed(id, result.error, ma_missing_inputs(result.output));
                return;
            }
            ma_parse_reply_async(self, result.output, [self, id, on_ok](QJsonObject obj, bool ok) {
                if (!self)
                    return;
                if (!ok) {
                    emit self->deal_op_failed(id, tr("The deal database answered with something unreadable."), {});
                    return;
                }
                on_ok(id, obj);
            });
        });
    return id;
}

quint64 MAAnalyticsService::start_deal_scan(int days, int max_filings, const QStringList& forms) {
    if (scan_request_ != 0)
        return scan_request_; // one scan at a time: the caller keeps following the running one
    const quint64 id = ++next_request_id_;
    scan_request_ = id;
    scan_cancel_ = std::make_shared<std::atomic<bool>>(false);

    python::PythonRunner::RunOptions opts;
    opts.timeout_ms = kDealScanTimeoutMs;
    opts.cancel_token = scan_cancel_;
    QStringList args = {QStringLiteral("scan"), QString::number(std::clamp(days, 1, 90)),
                        QString::number(std::clamp(max_filings, 0, 200))};
    if (!forms.isEmpty())
        args << forms.join(QLatin1Char(','));

    // Progress lines arrive on the runner's (GUI) thread while the scan runs.
    auto last_filings = std::make_shared<int>(0);
    auto throttled = std::make_shared<bool>(false);
    QPointer<MAAnalyticsService> self = this;
    auto on_line = [self, id, last_filings, throttled](const QString& line, bool is_stderr) {
        if (!self || !is_stderr)
            return;
        static const QString kPrefix = QStringLiteral("@@PROGRESS ");
        if (line.startsWith(kPrefix)) {
            const QJsonObject p = QJsonDocument::fromJson(line.mid(kPrefix.size()).toUtf8()).object();
            *last_filings = p.value(QStringLiteral("filings")).toInt(*last_filings);
            emit self->deal_scan_progress(id, p.value(QStringLiteral("message")).toString(),
                                          p.value(QStringLiteral("done")).toInt(), p.value(QStringLiteral("total")).toInt(),
                                          *last_filings, p.value(QStringLiteral("deals_created")).toInt());
        } else if (line.contains(QLatin1String("HTTP 403")) || line.contains(QLatin1String("HTTP 429"))) {
            *throttled = true;
        }
    };
    LOG_INFO("MAAnalytics", QString("Deal scan %1 started: %2 days, up to %3 filings").arg(id).arg(days).arg(max_filings));
    python::PythonRunner::instance().run_with_options(
        kMaDealTracker, args, opts,
        [self, id, last_filings, throttled](python::PythonResult result) {
            if (!self)
                return;
            self->scan_request_ = 0;
            self->scan_cancel_.reset();
            if (!result.success) {
                const bool cancelled = result.error == QLatin1String("Cancelled");
                QString message = result.error;
                if (*throttled)
                    message = tr("SEC did not answer - try again in a minute.");
                LOG_INFO("MAAnalytics", QString("Deal scan %1 ended: %2").arg(id).arg(result.error));
                emit self->deal_scan_failed(id, message, cancelled, *last_filings);
                return;
            }
            ma_parse_reply_async(self, result.output, [self, id, last_filings](QJsonObject obj, bool ok) {
                if (!self)
                    return;
                if (!ok) {
                    emit self->deal_scan_failed(id, tr("The scan answered with something unreadable."), false,
                                                *last_filings);
                    return;
                }
                emit self->deal_scan_finished(id, obj.value(QStringLiteral("data")).toObject());
            });
        },
        on_line);
    return id;
}

void MAAnalyticsService::cancel_deal_scan() {
    if (scan_cancel_)
        scan_cancel_->store(true);
}

quint64 MAAnalyticsService::load_deals() {
    return run_deal_db(kMaDealTracker, {QStringLiteral("list")}, kDealDbTimeoutMs,
                       [this](quint64 id, const QJsonObject& obj) {
                           emit deals_loaded(id, obj.value(QStringLiteral("data")).toArray());
                       });
}

quint64 MAAnalyticsService::load_filings() {
    return run_deal_db(kMaDealTracker, {QStringLiteral("filings"), QStringLiteral("1000")}, kDealDbTimeoutMs,
                       [this](quint64 id, const QJsonObject& obj) {
                           emit filings_loaded(id, obj.value(QStringLiteral("data")).toArray());
                       });
}

quint64 MAAnalyticsService::save_deal(const QJsonObject& fields) {
    const QString json = QString::fromUtf8(QJsonDocument(fields).toJson(QJsonDocument::Compact));
    return run_deal_db(kMaDealTracker, {QStringLiteral("upsert"), json}, kDealDbTimeoutMs,
                       [this](quint64 id, const QJsonObject& obj) {
                           emit deal_saved(id, obj.value(QStringLiteral("data")).toObject());
                       });
}

quint64 MAAnalyticsService::set_deal_verified(const QString& deal_id, bool verified) {
    return run_deal_db(kMaDealTracker,
                       {QStringLiteral("verify"), deal_id, verified ? QStringLiteral("1") : QStringLiteral("0")},
                       kDealDbTimeoutMs, [this](quint64 id, const QJsonObject& obj) {
                           emit deal_saved(id, obj.value(QStringLiteral("data")).toObject());
                       });
}

quint64 MAAnalyticsService::delete_deal(const QString& deal_id) {
    return run_deal_db(kMaDealTracker, {QStringLiteral("delete"), deal_id}, kDealDbTimeoutMs,
                       [this, deal_id](quint64 id, const QJsonObject&) { emit deal_removed(id, deal_id); });
}

quint64 MAAnalyticsService::extract_deal(const QString& accession, const QString& form) {
    // One filing: one complete-submission download from SEC (rate-limited) plus local work.
    return run_deal_db(kMaDealParser, {QStringLiteral("parse"), accession, form}, 2 * 60 * 1000,
                       [this](quint64 id, const QJsonObject& obj) { emit deal_extracted(id, obj); });
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
    const bool own_comps = !params.value(QStringLiteral("comp_data")).toArray().isEmpty();
    run_python_json("Analytics/corporateFinance/advanced_analytics/regression_analysis.py", "run", params,
                    "regression", own_comps);
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
    return 1; // Heavy Python analytics; callers drive their own cadence.
}

void MAAnalyticsService::ensure_registered_with_hub() {
    if (hub_registered_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    fincept::datahub::TopicPolicy policy;
    policy.push_only = true;
    policy.ttl_ms = kResultTtlSec * 1000; // 2 min cache for latecomers.
    hub.set_policy_pattern(QStringLiteral("ma:*"), policy);

    hub_registered_ = true;
    LOG_DEBUG("MAAnalyticsService", "Registered with DataHub (ma:*)");
}

} // namespace fincept::services::ma
