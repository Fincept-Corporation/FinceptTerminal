// src/services/gov_data/GovDataService.cpp
#include "services/gov_data/GovDataService.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>

namespace fincept::services {

namespace {

/// Mask credentials a script (or the HTTP library under it) echoes back in an error message — e.g. a
/// requests failure prints the full URL including "?api_key=...". Error text is shown in the panel,
/// logged and published on the hub, so it must never carry a key.
QString gov_svc_redact_secrets(QString text) {
    static const QRegularExpression kQueryKey(
        QStringLiteral("\\b(api[_-]?key|apikey|access[_-]?token|token|key|subscription-key|password|secret)=[^&\\s\"']+"),
        QRegularExpression::CaseInsensitiveOption);
    text.replace(kQueryKey, QStringLiteral("\\1=***"));
    return text;
}

/// A connector script signals failure as {"success": false, "error": "..."} or a bare {"error": "..."};
/// a good payload may still carry "error": null. Returns the failure message, or an empty string for a
/// normal result. (The panels only ever looked at result.success, which was true for every script that
/// exits 0 — so a rejected request was rendered as an empty table instead of an error.)
QString gov_svc_script_error(const QJsonObject& obj) {
    const QJsonValue err = obj.value(QStringLiteral("error"));
    QString msg;
    if (err.isString())
        msg = err.toString();
    else if (err.isObject())
        msg = err.toObject().value(QStringLiteral("message")).toString();

    const QJsonValue ok = obj.value(QStringLiteral("success"));
    const bool explicit_success = ok.isBool() && ok.toBool();
    const bool explicit_failure = ok.isBool() && !ok.toBool();

    if (explicit_failure) {
        if (msg.isEmpty())
            msg = obj.value(QStringLiteral("message")).toString();
        return msg.isEmpty() ? QStringLiteral("Request failed") : msg;
    }
    return explicit_success ? QString() : msg;
}

} // namespace

// ── Static provider list ─────────────────────────────────────────────────────

static const QVector<GovProviderInfo> kProviders = {
    {"us-treasury", "US Treasury", "U.S. Department of the Treasury",
     "Treasury securities prices, auctions & market data", "#3B82F6", "United States", "US", "government_us_data.py"},

    {"us-congress", "US Congress", "United States Congress", "Congressional bills, resolutions & legislative activity",
     "#8B5CF6", "United States", "US", "congress_gov_data.py"},

    {"canada-gov", "Canada Open Gov", "Government of Canada Open Data",
     "Open data from Canadian federal departments & agencies", "#EF4444", "Canada", "CA", "canada_gov_api.py"},

    {"swiss", "Swiss Open Data", "opendata.swiss", "Swiss federal open data portal (DE/FR/IT/EN)", "#E11D48",
     "Switzerland", "CH", "swiss_gov_api.py"},

    {"france", "France Open Data", "data.gouv.fr", "French government APIs: geographic, datasets, company registry",
     "#2563EB", "France", "FR", "french_gov_api.py"},

    {"hk", "Hong Kong Gov", "Data.gov.hk", "Hong Kong government open data portal", "#F43F5E", "Hong Kong", "HK",
     "data_gov_hk_api.py"},

    {"openafrica", "openAFRICA", "openAFRICA Open Data Portal",
     "African open data from organizations across the continent", "#F59E0B", "Africa", "AF",
     "openafrica_api.py"}, // native openAFRICA CKAN client (open.africa) — was wrongly canada_gov_api.py

    // NOTE: Spain (datos.gob.es) was REMOVED — it was mis-wired to canada_gov_api.py and
    // served CANADIAN datasets under the Spanish flag (a data-provenance bug). datos.gob.es
    // is not a CKAN portal; re-add once scripts/spain_data.py is aligned to the
    // GovDataProviderPanel CLI/response contract and validated against the live portal.

    {"universal-ckan", "CKAN Portals", "Universal CKAN Open Data Portals",
     "8 CKAN portals: US, UK, Australia, Italy, Brazil & more", "#10B981", "Multi-Country", "CKAN", "datagovuk_api.py"},

    {"australia", "Australia Gov", "data.gov.au", "Australian government open data portal", "#0EA5E9", "Australia",
     "AU", "datagov_au_api.py"},
};

const QVector<GovProviderInfo>& GovDataService::providers() {
    return kProviders;
}

const GovProviderInfo* GovDataService::provider_by_id(const QString& id) {
    for (const auto& p : kProviders) {
        if (p.id == id)
            return &p;
    }
    return nullptr;
}

// ── Singleton ────────────────────────────────────────────────────────────────

GovDataService& GovDataService::instance() {
    static GovDataService inst;
    return inst;
}

GovDataService::GovDataService(QObject* parent) : QObject(parent) {}

// ── Cache ────────────────────────────────────────────────────────────────────

QString GovDataService::cache_key(const QString& script, const QString& command, const QStringList& args) const {
    return "govdata:" + script + ":" + command + ":" + args.join(",");
}

// ── Execute ──────────────────────────────────────────────────────────────────

void GovDataService::execute(const QString& script, const QString& command, const QStringList& args,
                             const QString& request_id) {
    const QString key = cache_key(script, command, args);

    const QString topic = hub_topic(script, request_id);
    dispatch_records_.insert(topic, DispatchRecord{script, command, args, request_id});

    // Serve from cache if fresh
    const QVariant cached = fincept::CacheManager::instance().get(key);
    if (!cached.isNull()) {
        LOG_DEBUG("GovDataService", QString("Cache hit: %1").arg(key));
        GovDataResult result;
        result.success = true;
        result.data = QJsonDocument::fromJson(cached.toString().toUtf8()).object();
        emit result_ready(request_id, result);
        if (hub_registered_)
            fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(result));
        return;
    }

    // Build args: command first, then extra args
    QStringList full_args;
    full_args << command;
    full_args << args;

    LOG_INFO("GovDataService", QString("Executing %1 %2 [%3]").arg(script, command, args.join(", ")));

    QPointer<GovDataService> self = this;
    python::PythonRunner::instance().run(
        script, full_args, [self, script, request_id, key](python::PythonResult py_result) {
            if (!self)
                return;

            GovDataResult result;

            // Every failure path must report to the caller AND clear the hub topic's in_flight flag
            // (publish_error); otherwise a failed refresh leaves the topic "busy" until the scheduler's
            // refresh timeout and retries are silently refused.
            auto fail = [&](const QString& message) {
                result.success = false;
                result.error = gov_svc_redact_secrets(message);
                LOG_ERROR("GovDataService", QString("%1 failed: %2").arg(script, result.error.left(300)));
                emit self->result_ready(request_id, result);
                if (self->hub_registered_)
                    fincept::datahub::DataHub::instance().publish_error(GovDataService::hub_topic(script, request_id),
                                                                        result.error);
            };

            // Extract JSON from output (also on a non-zero exit: scripts print {"success": false,
            // "error": "..."} before exiting 1, which is far more useful than "exited with code 1").
            const QString json_str = python::extract_json(py_result.output);
            QJsonParseError parse_err;
            const QJsonDocument doc = json_str.isEmpty() ? QJsonDocument() : QJsonDocument::fromJson(json_str.toUtf8(), &parse_err);

            if (!py_result.success) {
                QString msg;
                if (doc.isObject())
                    msg = gov_svc_script_error(doc.object());
                if (msg.isEmpty())
                    msg = py_result.error.isEmpty() ? QString("Script exited with code %1").arg(py_result.exit_code)
                                                    : py_result.error;
                fail(msg);
                return;
            }

            if (json_str.isEmpty()) {
                fail(QStringLiteral("No JSON output from script"));
                return;
            }
            if (doc.isNull()) {
                fail(QString("JSON parse error: %1").arg(parse_err.errorString()));
                return;
            }

            if (doc.isObject()) {
                const QString script_error = gov_svc_script_error(doc.object());
                if (!script_error.isEmpty()) {
                    fail(script_error);
                    return;
                }
                result.data = doc.object();
            } else {
                // A bare JSON array: expose it under "data" (the key the panels read) instead of
                // collapsing it to an empty object.
                result.data = QJsonObject{{QStringLiteral("success"), true}, {QStringLiteral("data"), doc.array()}};
            }
            result.success = true;

            // Cache the result
            fincept::CacheManager::instance().put(
                key, QVariant(QString::fromUtf8(QJsonDocument(result.data).toJson(QJsonDocument::Compact))),
                kCacheTtlSec, "govdata");

            LOG_INFO("GovDataService", QString("Result ready: %1").arg(request_id));
            emit self->result_ready(request_id, result);
            if (self->hub_registered_) {
                const QString topic = GovDataService::hub_topic(script, request_id);
                fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(result));
            }
        });
}

// ── DataHub producer wiring ─────────────────────────────────────────────────

QString GovDataService::hub_topic(const QString& script, const QString& request_id) {
    // Map script filename → provider id when possible; fall back to the
    // script name stripped of extension.
    QString provider_key;
    for (const auto& p : kProviders) {
        if (p.script == script) {
            provider_key = p.id;
            break;
        }
    }
    if (provider_key.isEmpty()) {
        provider_key = script;
        if (provider_key.endsWith(QLatin1String(".py")))
            provider_key.chop(3);
    }
    return QStringLiteral("govdata:") + provider_key + QLatin1Char(':') + request_id;
}

QStringList GovDataService::topic_patterns() const {
    return {QStringLiteral("govdata:*")};
}

void GovDataService::refresh(const QStringList& topics) {
    for (const auto& topic : topics) {
        auto it = dispatch_records_.constFind(topic);
        if (it == dispatch_records_.constEnd()) {
            LOG_DEBUG("GovDataService", "refresh() for unknown topic (no prior execute): " + topic);
            continue;
        }
        const DispatchRecord rec = it.value();
        fincept::CacheManager::instance().remove(cache_key(rec.script, rec.command, rec.args));
        execute(rec.script, rec.command, rec.args, rec.request_id);
    }
}

int GovDataService::max_requests_per_sec() const {
    return 2;
}

void GovDataService::ensure_registered_with_hub() {
    if (hub_registered_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    // 1-hour TTL, 60s min_interval — same as economics (govdata updates slowly).
    fincept::datahub::TopicPolicy policy;
    policy.ttl_ms = 60 * 60 * 1000;
    policy.min_interval_ms = 60 * 1000;
    policy.push_only = false;
    hub.set_policy_pattern(QStringLiteral("govdata:*"), policy);

    hub_registered_ = true;
    LOG_INFO("GovDataService", "Registered with DataHub (govdata:*)");
}

} // namespace fincept::services
