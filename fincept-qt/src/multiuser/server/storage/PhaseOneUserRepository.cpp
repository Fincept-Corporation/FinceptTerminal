#include "multiuser/server/storage/PhaseOneUserRepository.h"

#include <QSqlQuery>

namespace fincept::multiuser {

PhaseOneUserSummary PhaseOneStoredUser::to_summary() const {
    return {user_id, username, role, status};
}

fincept::Result<int> PhaseOneUserRepository::count_users() const {
    auto result = db().execute(QStringLiteral("SELECT COUNT(*) FROM users"));
    if (result.is_err())
        return fincept::Result<int>::err(result.error());

    auto& query = result.value();
    if (!query.next())
        return fincept::Result<int>::err("Failed to read user count");

    return fincept::Result<int>::ok(query.value(0).toInt());
}

fincept::Result<int> PhaseOneUserRepository::count_active_users() const {
    auto result = db().execute(QStringLiteral("SELECT COUNT(*) FROM users WHERE status = 'active'"));
    if (result.is_err())
        return fincept::Result<int>::err(result.error());

    auto& query = result.value();
    if (!query.next())
        return fincept::Result<int>::err("Failed to read active user count");
    return fincept::Result<int>::ok(query.value(0).toInt());
}

fincept::Result<int> PhaseOneUserRepository::count_active_admins() const {
    auto result = db().execute(QStringLiteral("SELECT COUNT(*) FROM users WHERE status = 'active' AND role = 'admin'"));
    if (result.is_err())
        return fincept::Result<int>::err(result.error());

    auto& query = result.value();
    if (!query.next())
        return fincept::Result<int>::err("Failed to read active admin count");
    return fincept::Result<int>::ok(query.value(0).toInt());
}

std::optional<PhaseOneStoredUser> PhaseOneUserRepository::find_by_id(int user_id) const {
    return query_optional(QStringLiteral("SELECT id, username, password_hash, role, status, created_at, updated_at "
                                         "FROM users WHERE id = ? LIMIT 1"),
                          {user_id}, map_row);
}

std::optional<PhaseOneStoredUser> PhaseOneUserRepository::find_by_username(const QString& username) const {
    return query_optional(QStringLiteral("SELECT id, username, password_hash, role, status, created_at, updated_at "
                                         "FROM users WHERE username = ? LIMIT 1"),
                          {canonicalize_username(username)}, map_row);
}

fincept::Result<QVector<PhaseOneStoredUser>> PhaseOneUserRepository::list_users() const {
    return query_list(QStringLiteral("SELECT id, username, password_hash, role, status, created_at, updated_at "
                                     "FROM users ORDER BY id ASC"),
                      {}, map_row);
}

fincept::Result<qint64> PhaseOneUserRepository::create_user(const QString& username, const QString& role,
                                                            const QString& status, const QString& password_hash) const {
    return exec_insert(QStringLiteral("INSERT INTO users (username, password_hash, role, status) VALUES (?, ?, ?, ?)"),
                       {canonicalize_username(username), password_hash.isEmpty() ? QVariant() : QVariant(password_hash),
                        role, status});
}

fincept::Result<void> PhaseOneUserRepository::set_initial_password(int user_id, const QString& password_hash) const {
    return exec_write(QStringLiteral("UPDATE users SET password_hash = ?, updated_at = datetime('now') WHERE id = ?"),
                      {password_hash, user_id});
}

fincept::Result<void> PhaseOneUserRepository::update_status(int user_id, const QString& status) const {
    return exec_write(QStringLiteral("UPDATE users SET status = ?, updated_at = datetime('now') WHERE id = ?"),
                      {status, user_id});
}

fincept::Result<void> PhaseOneUserRepository::clear_admin_role() const {
    return exec_write(QStringLiteral("UPDATE users SET role = 'standard', updated_at = datetime('now') "
                                     "WHERE role = 'admin'"));
}

fincept::Result<void> PhaseOneUserRepository::set_admin_role(int user_id) const {
    return exec_write(QStringLiteral("UPDATE users SET role = 'admin', updated_at = datetime('now') WHERE id = ?"),
                      {user_id});
}

PhaseOneStoredUser PhaseOneUserRepository::map_row(QSqlQuery& query) {
    PhaseOneStoredUser user;
    user.user_id = query.value(0).toInt();
    user.username = query.value(1).toString();
    user.password_hash = query.value(2).toString();
    user.role = query.value(3).toString();
    user.status = query.value(4).toString();
    user.created_at = query.value(5).toString();
    user.updated_at = query.value(6).toString();
    return user;
}

QString PhaseOneUserRepository::canonicalize_username(const QString& username) {
    return username.trimmed().toLower();
}

} // namespace fincept::multiuser
