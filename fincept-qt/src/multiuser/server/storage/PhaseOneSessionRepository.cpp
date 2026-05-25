#include "multiuser/server/storage/PhaseOneSessionRepository.h"

#include <QDateTime>
#include <QSqlQuery>

namespace fincept::multiuser {

PhaseOneSessionInfo PhaseOneStoredSession::to_session_info() const {
    return {user_id, username, role, session_id, expires_at};
}

std::optional<PhaseOneStoredSession> PhaseOneSessionRepository::find_session(const QString& session_id) const {
    return query_optional(QStringLiteral("SELECT s.id, s.user_id, u.username, u.role, s.session_id, s.issued_at, "
                                         "s.last_activity, s.expires_at, s.invalidated "
                                         "FROM sessions s "
                                         "JOIN users u ON u.id = s.user_id "
                                         "WHERE s.session_id = ? LIMIT 1"),
                          {session_id}, map_row);
}

std::optional<PhaseOneStoredSession> PhaseOneSessionRepository::find_active_session(const QString& session_id) const {
    return query_optional(QStringLiteral("SELECT s.id, s.user_id, u.username, u.role, s.session_id, s.issued_at, "
                                         "s.last_activity, s.expires_at, s.invalidated "
                                         "FROM sessions s "
                                         "JOIN users u ON u.id = s.user_id "
                                         "WHERE s.session_id = ? AND s.invalidated = 0 LIMIT 1"),
                          {session_id}, map_row);
}

fincept::Result<QVector<PhaseOneStoredSession>> PhaseOneSessionRepository::list_active_sessions_for_user(int user_id) const {
    return query_list(QStringLiteral("SELECT s.id, s.user_id, u.username, u.role, s.session_id, s.issued_at, "
                                     "s.last_activity, s.expires_at, s.invalidated "
                                     "FROM sessions s "
                                     "JOIN users u ON u.id = s.user_id "
                                     "WHERE s.user_id = ? AND s.invalidated = 0 ORDER BY s.id ASC"),
                      {user_id}, map_row);
}

fincept::Result<PhaseOneStoredSession> PhaseOneSessionRepository::create_session(int user_id, const QString& session_id) const {
    const QString expires_at = QDateTime::currentDateTimeUtc().addSecs(kDefaultTimeoutSeconds).toString(Qt::ISODate);
    const auto result = exec_insert(QStringLiteral("INSERT INTO sessions (session_id, user_id, expires_at, invalidated) "
                                                   "VALUES (?, ?, ?, 0)"),
                                    {session_id, user_id, expires_at});
    if (result.is_err())
        return fincept::Result<PhaseOneStoredSession>::err(result.error());

    const auto session = find_active_session(session_id);
    if (!session.has_value())
        return fincept::Result<PhaseOneStoredSession>::err("Failed to reload created session");
    return fincept::Result<PhaseOneStoredSession>::ok(*session);
}

fincept::Result<void> PhaseOneSessionRepository::invalidate_session(const QString& session_id) const {
    return exec_write(QStringLiteral("UPDATE sessions SET invalidated = 1, last_activity = datetime('now') "
                                     "WHERE session_id = ?"),
                      {session_id});
}

fincept::Result<void> PhaseOneSessionRepository::invalidate_sessions_for_user(int user_id) const {
    return exec_write(QStringLiteral("UPDATE sessions SET invalidated = 1, last_activity = datetime('now') "
                                     "WHERE user_id = ? AND invalidated = 0"),
                      {user_id});
}

fincept::Result<void> PhaseOneSessionRepository::refresh_activity(const QString& session_id) const {
    const QString expires_at = QDateTime::currentDateTimeUtc().addSecs(kDefaultTimeoutSeconds).toString(Qt::ISODate);
    return exec_write(QStringLiteral("UPDATE sessions SET last_activity = datetime('now'), expires_at = ? "
                                     "WHERE session_id = ? AND invalidated = 0"),
                      {expires_at, session_id});
}

PhaseOneStoredSession PhaseOneSessionRepository::map_row(QSqlQuery& query) {
    PhaseOneStoredSession session;
    session.session_row_id = query.value(0).toInt();
    session.user_id = query.value(1).toInt();
    session.username = query.value(2).toString();
    session.role = query.value(3).toString();
    session.session_id = query.value(4).toString();
    session.issued_at = query.value(5).toString();
    session.last_activity = query.value(6).toString();
    session.expires_at = query.value(7).toString();
    session.invalidated = query.value(8).toInt() != 0;
    return session;
}

} // namespace fincept::multiuser
