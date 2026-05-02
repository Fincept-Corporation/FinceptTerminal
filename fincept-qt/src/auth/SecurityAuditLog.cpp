#include "auth/SecurityAuditLog.h"

#include "core/logging/Logger.h"
#include "storage/sqlite/Database.h"

#include <QSqlQuery>
#include <QVariantList>

namespace fincept::auth {

SecurityAuditLog& SecurityAuditLog::instance() {
    static SecurityAuditLog s;
    return s;
}

void SecurityAuditLog::record(const QString& event, const QString& detail) {
    auto& db = Database::instance();
    if (!db.is_open()) {
        // DB closed — pre-migration startup is the expected case (events
        // recorded during AuthManager::initialize() before the DB is open),
        // but it can also mean disk-full / migration failure / shutdown
        // race. The latter cases must NOT be silent — a missed
        // max_attempts_exceeded entry hides a brute-force attempt. Log
        // loudly so ops sees it; the normal Logger still has the event.
        LOG_ERROR("SecurityAudit",
                  QString("DB unavailable while recording '%1' — audit gap").arg(event));
        return;
    }

    const qint64 ts = QDateTime::currentSecsSinceEpoch();
    const auto r = db.execute(
        "INSERT INTO security_audit_log (ts, event, detail) VALUES (?, ?, ?)",
        {QVariant(ts), QVariant(event), QVariant(detail)});
    if (r.is_err()) {
        LOG_ERROR("SecurityAudit",
                  QString("Failed to record event '%1': %2").arg(event, QString::fromStdString(r.error())));
    }
}

QList<AuditEvent> SecurityAuditLog::recent(int limit) {
    QList<AuditEvent> out;
    auto& db = Database::instance();
    if (!db.is_open())
        return out;

    auto r = db.execute(
        "SELECT id, ts, event, detail FROM security_audit_log ORDER BY ts DESC LIMIT ?",
        {QVariant(limit)});
    if (r.is_err())
        return out;

    auto& q = r.value();
    while (q.next()) {
        AuditEvent e;
        e.id = q.value(0).toLongLong();
        e.timestamp = QDateTime::fromSecsSinceEpoch(q.value(1).toLongLong());
        e.event = q.value(2).toString();
        e.detail = q.value(3).toString();
        out.append(e);
    }
    return out;
}

} // namespace fincept::auth
