#include "core/telemetry/LocalTelemetrySink.h"

#include "app/TerminalShell.h"
#include "core/logging/Logger.h"
#include "storage/workspace/WorkspaceDb.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMutexLocker>

namespace fincept::telemetry {

LocalTelemetrySink::LocalTelemetrySink() = default;

void LocalTelemetrySink::record(const QString& event_id, const QVariantMap& payload) {
    auto* db = TerminalShell::instance().workspace_db();
    if (!db || !db->is_open()) {
        // Pre-init or post-shutdown — silently skip. Workspace.db isn't
        // available; nowhere to write. Not an error.
        return;
    }

    {
        QMutexLocker lock(&schema_mutex_);
        if (!schema_ensured_) {
            ensure_schema();
            schema_ensured_ = true;
        }
    }

    // Convert payload to JSON. Privacy: caller is responsible for what's
    // in the map; we don't sanitise here (decision 10.4 — no symbols,
    // no credentials).
    QJsonObject obj;
    for (auto it = payload.constBegin(); it != payload.constEnd(); ++it)
        obj.insert(it.key(), QJsonValue::fromVariant(it.value()));
    const QString json = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));

    const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
    const QString session = TerminalShell::instance().session_id();

    auto r = db->execute(
        "INSERT INTO telemetry_events(ts_ms, event_id, payload_json, session_id) "
        "VALUES (?, ?, ?, ?)",
        {now_ms, event_id, json, session});

    if (r.is_err()) {
        // Mark unhealthy; future record() calls still try (transient
        // disk full / busy timeout shouldn't permanently kill telemetry).
        healthy_ = false;
        LOG_WARN("LocalTelemetry", QString("record failed: %1")
                                       .arg(QString::fromStdString(r.error())));
    } else if (!healthy_) {
        healthy_ = true;
        LOG_INFO("LocalTelemetry", "Recovered from previous write failure");
    }
}

void LocalTelemetrySink::ensure_schema() {
    auto* db = TerminalShell::instance().workspace_db();
    if (!db || !db->is_open())
        return;

    auto r = db->exec("CREATE TABLE IF NOT EXISTS telemetry_events ("
                      "  id           INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "  ts_ms        INTEGER NOT NULL,"
                      "  event_id     TEXT NOT NULL,"
                      "  payload_json TEXT NOT NULL,"
                      "  session_id   TEXT"
                      ")");
    if (r.is_err()) {
        LOG_ERROR("LocalTelemetry",
                  QString("schema create failed: %1").arg(QString::fromStdString(r.error())));
        healthy_ = false;
        return;
    }
    auto idx = db->exec("CREATE INDEX IF NOT EXISTS idx_telemetry_ts "
                        "ON telemetry_events(ts_ms)");
    if (idx.is_err()) {
        LOG_WARN("LocalTelemetry",
                 QString("idx create failed: %1").arg(QString::fromStdString(idx.error())));
    }
    LOG_INFO("LocalTelemetry", "Schema ensured");
}

} // namespace fincept::telemetry
