#pragma once

#include "multiuser/contracts/PhaseOneAuthTypes.h"
#include "storage/repositories/BaseRepository.h"

#include <QString>

#include <optional>

namespace fincept::multiuser {

struct PhaseOneStoredSession {
    int session_row_id = 0;
    int user_id = 0;
    QString username;
    QString role;
    QString session_id;
    QString issued_at;
    QString last_activity;
    QString expires_at;
    bool invalidated = false;

    PhaseOneSessionInfo to_session_info() const;
};

class PhaseOneSessionRepository : public fincept::BaseRepository<PhaseOneStoredSession> {
  public:
    static constexpr int kDefaultTimeoutSeconds = 8 * 60 * 60;

    std::optional<PhaseOneStoredSession> find_session(const QString& session_id) const;
    std::optional<PhaseOneStoredSession> find_active_session(const QString& session_id) const;
    fincept::Result<QVector<PhaseOneStoredSession>> list_active_sessions_for_user(int user_id) const;
    fincept::Result<PhaseOneStoredSession> create_session(int user_id, const QString& session_id) const;
    fincept::Result<void> invalidate_session(const QString& session_id) const;
    fincept::Result<void> invalidate_sessions_for_user(int user_id) const;
    fincept::Result<void> refresh_activity(const QString& session_id) const;

  private:
    static PhaseOneStoredSession map_row(QSqlQuery& query);
};

} // namespace fincept::multiuser
