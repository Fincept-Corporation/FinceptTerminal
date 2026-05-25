#pragma once

#include "multiuser/contracts/PhaseOneUserAdminTypes.h"
#include "storage/repositories/BaseRepository.h"

#include <QString>

#include <optional>

namespace fincept::multiuser {

struct PhaseOneStoredUser {
    int user_id = 0;
    QString username;
    QString password_hash;
    QString role;
    QString status;
    QString created_at;
    QString updated_at;

    PhaseOneUserSummary to_summary() const;
};

class PhaseOneUserRepository : public fincept::BaseRepository<PhaseOneStoredUser> {
  public:
    fincept::Result<int> count_users() const;
    fincept::Result<int> count_active_users() const;
    fincept::Result<int> count_active_admins() const;
    std::optional<PhaseOneStoredUser> find_by_id(int user_id) const;
    std::optional<PhaseOneStoredUser> find_by_username(const QString& username) const;
    fincept::Result<QVector<PhaseOneStoredUser>> list_users() const;

    fincept::Result<qint64> create_user(const QString& username, const QString& role = QStringLiteral("standard"),
                                        const QString& status = QStringLiteral("active"),
                                        const QString& password_hash = {}) const;
    fincept::Result<void> set_initial_password(int user_id, const QString& password_hash) const;
    fincept::Result<void> update_status(int user_id, const QString& status) const;
    fincept::Result<void> clear_admin_role() const;
    fincept::Result<void> set_admin_role(int user_id) const;

  private:
    static PhaseOneStoredUser map_row(QSqlQuery& query);
    static QString canonicalize_username(const QString& username);
};

} // namespace fincept::multiuser
