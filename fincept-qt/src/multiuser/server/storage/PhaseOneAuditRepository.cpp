#include "multiuser/server/storage/PhaseOneAuditRepository.h"

#include <QSqlQuery>

namespace fincept::multiuser {

fincept::Result<void> PhaseOneAuditRepository::append_event(const QString& user_identity, const QString& action_type,
                                                            const QString& target, const QString& result_status) const {
    return exec_write(QStringLiteral("INSERT INTO audit_events (user_identity, action_type, target, result_status) "
                                     "VALUES (?, ?, ?, ?)"),
                      {user_identity, action_type, target, result_status});
}

fincept::Result<void> PhaseOneAuditRepository::append_portfolio_event(const QString& user_identity,
                                                                      const QString& action_type,
                                                                      const QString& portfolio_id,
                                                                      const QString& result_status) const {
    return append_event(user_identity, action_type, QStringLiteral("portfolio:%1").arg(portfolio_id), result_status);
}

fincept::Result<void> PhaseOneAuditRepository::append_holding_event(const QString& user_identity,
                                                                    const QString& action_type,
                                                                    const QString& holding_target,
                                                                    const QString& result_status) const {
    return append_event(user_identity, action_type, holding_target, result_status);
}

fincept::Result<void> PhaseOneAuditRepository::append_session_event(const QString& user_identity,
                                                                    const QString& action_type,
                                                                    const QString& session_id,
                                                                    const QString& result_status) const {
    return append_event(user_identity, action_type, QStringLiteral("session:%1").arg(session_id), result_status);
}

fincept::Result<QVector<PhaseOneAuditEvent>> PhaseOneAuditRepository::list_events(const PhaseOneAuditFilter& filter) const {
    QString sql = QStringLiteral("SELECT timestamp, user_identity, action_type, target, result_status "
                                 "FROM audit_events WHERE 1 = 1");
    QVariantList params;

    if (!filter.user_identity.isEmpty()) {
        sql += QStringLiteral(" AND user_identity = ?");
        params.append(filter.user_identity);
    }
    if (!filter.action_type.isEmpty()) {
        sql += QStringLiteral(" AND action_type = ?");
        params.append(filter.action_type);
    }

    sql += QStringLiteral(" ORDER BY timestamp DESC, id DESC");
    return query_list_as<PhaseOneAuditEvent>(sql, params, map_row);
}

PhaseOneAuditEvent PhaseOneAuditRepository::map_row(QSqlQuery& query) {
    PhaseOneAuditEvent event;
    event.timestamp = query.value(0).toString();
    event.user_identity = query.value(1).toString();
    event.action_type = query.value(2).toString();
    event.target = query.value(3).toString();
    event.result_status = query.value(4).toString();
    return event;
}

} // namespace fincept::multiuser
