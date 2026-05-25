#pragma once

#include "multiuser/contracts/PhaseOneAuditTypes.h"
#include "storage/repositories/BaseRepository.h"

namespace fincept::multiuser {

class PhaseOneAuditRepository : public fincept::BaseRepository<PhaseOneAuditEvent> {
  public:
    fincept::Result<void> append_event(const QString& user_identity, const QString& action_type, const QString& target,
                                       const QString& result_status) const;
    fincept::Result<void> append_portfolio_event(const QString& user_identity, const QString& action_type,
                                                 const QString& portfolio_id, const QString& result_status) const;
    fincept::Result<void> append_holding_event(const QString& user_identity, const QString& action_type,
                                               const QString& holding_target, const QString& result_status) const;
    fincept::Result<void> append_session_event(const QString& user_identity, const QString& action_type,
                                               const QString& session_id, const QString& result_status) const;
    fincept::Result<QVector<PhaseOneAuditEvent>> list_events(const PhaseOneAuditFilter& filter) const;

  private:
    static PhaseOneAuditEvent map_row(QSqlQuery& query);
};

} // namespace fincept::multiuser
