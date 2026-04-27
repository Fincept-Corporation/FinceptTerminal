// src/services/economics/EconomicsService.cpp
#include "services/economics/EconomicsService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"

#    include "datahub/DataHub.h"
#    include "datahub/DataHubMetaTypes.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace fincept::services {

EconomicsService& EconomicsService::instance() {
    static EconomicsService inst;
    return inst;
}

EconomicsService::EconomicsService(QObject* parent) : QObject(parent) {}

// ── Cache ─────────────────────────────────────────────────────────────────────

QString EconomicsService::cache_key(const QString& script, const QString& command, const QStringList& args) const {
    return "economics:" + script + ":" + command + ":" + args.join(",");
}

void EconomicsService::invalidate(const QString& request_id) {
    fincept::CacheManager::instance().remove(request_id);
}

// ── Execute ───────────────────────────────────────────────────────────────────

void EconomicsService::execute(const QString& source_id, const QString& script, const QString& command,
                               const QStringList& args, const QString& request_id) {
    const QString key = cache_key(script, command, args);

    // Remember the dispatch so hub-driven refresh() can replay it later.
    const QString topic = hub_topic(source_id, request_id);
    dispatch_records_.insert(topic, DispatchRecord{source_id, script, command, args, request_id});

    {
        const QVariant cached = fincept::CacheManager::instance().get(key);
        if (!cached.isNull()) {
            LOG_DEBUG("EconomicsService", "Cache hit: " + key);
            EconomicsResult res;
            res.success = true;
            res.data = QJsonDocument::fromJson(cached.toString().toUtf8()).object();
            res.source_id = source_id;
            emit result_ready(request_id, res);
            if (hub_registered_)
                fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(res));
            return;
        }
    }

    QStringList full_args;
    full_args << command << args;

    LOG_INFO("EconomicsService", QString("Run %1 %2 [%3]").arg(script, command, args.join(", ")));

    QPointer<EconomicsService> self = this;
    python::PythonRunner::instance().run(
        script, full_args, [self, source_id, request_id, key](python::PythonResult py) {
            if (!self)
                return;

            EconomicsResult res;
            res.source_id = source_id;

            if (!py.success) {
                res.success = false;
                res.error = py.error.isEmpty() ? QString("Script exited with code %1").arg(py.exit_code) : py.error;
                LOG_ERROR("EconomicsService", "Script failed: " + res.error);
                emit self->result_ready(request_id, res);
                return;
            }

            QString json_str = python::extract_json(py.output);
            if (json_str.isEmpty()) {
                res.success = false;
                res.error = "No JSON output from script";
                LOG_ERROR("EconomicsService", res.error);
                emit self->result_ready(request_id, res);
                return;
            }

            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(json_str.toUtf8(), &err);
            if (doc.isNull()) {
                res.success = false;
                res.error = "JSON parse error: " + err.errorString();
                LOG_ERROR("EconomicsService", res.error);
                emit self->result_ready(request_id, res);
                return;
            }

            // If the root is an array, wrap it
            if (doc.isArray())
                res.data = QJsonObject{{"data", doc.array()}};
            else
                res.data = doc.object();

            // Treat script-level error field as failure
            if (res.data.contains("error") && !res.data["error"].isNull() && !res.data["error"].toString().isEmpty() &&
                !res.data.contains("data")) {
                res.success = false;
                const QString error_code = res.data.value("error_code").toString();
                const QString message = res.data["error"].toString();
                // Prefix with [CODE] so panels can branch on it without a schema change.
                res.error = error_code.isEmpty() ? message : (QStringLiteral("[") + error_code + QStringLiteral("] ") + message);
                emit self->result_ready(request_id, res);
                return;
            }

            res.success = true;
            fincept::CacheManager::instance().put(
                key, QVariant(QString::fromUtf8(QJsonDocument(res.data).toJson(QJsonDocument::Compact))), kCacheTtlSec,
                "economics");
            LOG_INFO("EconomicsService", "Result ready: " + request_id);
            emit self->result_ready(request_id, res);
            if (self->hub_registered_) {
                const QString topic = EconomicsService::hub_topic(source_id, request_id);
                fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(res));
            }
        });
}

// ── DataHub producer wiring ─────────────────────────────────────────────────

QString EconomicsService::hub_topic(const QString& source_id, const QString& request_id) {
    return QStringLiteral("econ:") + source_id + QLatin1Char(':') + request_id;
}

QStringList EconomicsService::topic_patterns() const {
    // Single broad pattern — individual source refactor to explicit
    // patterns can happen later once panels migrate. Keeps producer
    // registration simple across 32 upstream sources.
    return {QStringLiteral("econ:*")};
}

void EconomicsService::refresh(const QStringList& topics) {
    // Replay the last-known dispatch for each topic. If we've never
    // seen an execute() for this topic, nothing to do — we don't know
    // which script + args produce it.
    for (const auto& topic : topics) {
        auto it = dispatch_records_.constFind(topic);
        if (it == dispatch_records_.constEnd()) {
            LOG_DEBUG("EconomicsService",
                      "refresh() for unknown topic (no prior execute): " + topic);
            continue;
        }
        const DispatchRecord rec = it.value();
        // Force re-fetch: clear CacheManager entry so execute() goes
        // through the Python path rather than returning the cached copy.
        fincept::CacheManager::instance().remove(cache_key(rec.script, rec.command, rec.args));
        execute(rec.source_id, rec.script, rec.command, rec.args, rec.request_id);
    }
}

int EconomicsService::max_requests_per_sec() const {
    return 2;  // Python spawn pacing — upstream rate limits are usually higher
}

void EconomicsService::ensure_registered_with_hub() {
    if (hub_registered_) return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    // 1-hour TTL, 60s min interval — economics series move slowly.
    fincept::datahub::TopicPolicy policy;
    policy.ttl_ms = 60 * 60 * 1000;
    policy.min_interval_ms = 60 * 1000;
    policy.push_only = false;
    hub.set_policy_pattern(QStringLiteral("econ:*"), policy);

    hub_registered_ = true;
    LOG_INFO("EconomicsService", "Registered with DataHub (econ:*)");
}

} // namespace fincept::services
