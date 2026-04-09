// src/services/economics/EconomicsService.cpp
#include "services/economics/EconomicsService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"

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

    {
        const QVariant cached = fincept::CacheManager::instance().get(key);
        if (!cached.isNull()) {
            LOG_DEBUG("EconomicsService", "Cache hit: " + key);
            EconomicsResult res;
            res.success = true;
            res.data = QJsonDocument::fromJson(cached.toString().toUtf8()).object();
            res.source_id = source_id;
            emit result_ready(request_id, res);
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
                res.error = res.data["error"].toString();
                emit self->result_ready(request_id, res);
                return;
            }

            res.success = true;
            fincept::CacheManager::instance().put(
                key, QVariant(QString::fromUtf8(QJsonDocument(res.data).toJson(QJsonDocument::Compact))), kCacheTtlSec,
                "economics");
            LOG_INFO("EconomicsService", "Result ready: " + request_id);
            emit self->result_ready(request_id, res);
        });
}

} // namespace fincept::services
