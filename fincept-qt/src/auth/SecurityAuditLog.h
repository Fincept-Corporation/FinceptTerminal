#pragma once
#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

namespace fincept::auth {

/// One persisted security audit event. Mirrors the `security_audit_log`
/// table created by migration v020.
struct AuditEvent {
    qint64 id = 0;
    QDateTime timestamp;
    QString event;
    QString detail;
};

/// Persistent append-only record of PIN / lock-screen security events.
/// Writes go to the `security_audit_log` table; recent reads power the
/// Settings → Security audit-log view.
///
/// Callers record events with short stable identifiers so the stream remains
/// grep-able and the read path is trivial. Event vocabulary:
///   pin_set, pin_changed, pin_cleared,
///   pin_verify_ok, pin_verify_fail,
///   lockout_started, lockout_cleared, max_attempts_exceeded,
///   inactivity_lock, manual_lock, resume_lock, minimize_lock
class SecurityAuditLog {
  public:
    static SecurityAuditLog& instance();

    /// Record an event. Silent on DB errors — failing to audit must not
    /// break the calling flow (e.g. PIN verification). Errors go to LOG_ERROR.
    void record(const QString& event, const QString& detail = QString());

    /// Fetch the most recent events, newest first.
    QList<AuditEvent> recent(int limit = 100);

  private:
    SecurityAuditLog() = default;
};

} // namespace fincept::auth
