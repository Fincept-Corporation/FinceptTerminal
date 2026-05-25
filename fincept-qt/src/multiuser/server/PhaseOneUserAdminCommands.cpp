#include "multiuser/server/PhaseOneUserAdminCommands.h"

#include "multiuser/server/PhaseOnePasswordHasher.h"
#include "storage/sqlite/Database.h"

namespace fincept::multiuser {

namespace {

constexpr int kMaxActiveUsers = 3;

} // namespace

PhaseOneUserAdminCommands::PhaseOneUserAdminCommands(PhaseOneUserRepository* user_repository,
                                                     PhaseOneAuditRepository* audit_repository)
    : user_repository_(user_repository), audit_repository_(audit_repository) {}

fincept::Result<void> PhaseOneUserAdminCommands::bootstrap(const QString& username, const QString& password) const {
    if (user_repository_->count_users().value() > 0) {
        write_audit(canonicalize_actor(username), QStringLiteral("bootstrap_admin"), QStringLiteral("system:bootstrap"),
                    QStringLiteral("failure"));
        return fincept::Result<void>::err("bootstrap_closed");
    }

    const QString password_hash = PhaseOnePasswordHasher::hash_password(password);
    const auto created = user_repository_->create_user(username, QStringLiteral("admin"), QStringLiteral("active"), password_hash);
    if (created.is_err()) {
        write_audit(canonicalize_actor(username), QStringLiteral("bootstrap_admin"), QStringLiteral("system:bootstrap"),
                    QStringLiteral("failure"));
        return fincept::Result<void>::err(created.error());
    }

    return write_audit(canonicalize_actor(username), QStringLiteral("bootstrap_admin"), QStringLiteral("system:bootstrap"),
                       QStringLiteral("success"));
}

fincept::Result<void> PhaseOneUserAdminCommands::create_user(const QString& username, const QString& actor) const {
    const auto active_users = user_repository_->count_active_users();
    if (active_users.is_err())
        return fincept::Result<void>::err(active_users.error());
    if (active_users.value() >= kMaxActiveUsers) {
        write_audit(actor, QStringLiteral("create_user"), QStringLiteral("user:%1").arg(username.trimmed().toLower()),
                    QStringLiteral("failure"));
        return fincept::Result<void>::err("active_user_cap_reached");
    }

    const auto created = user_repository_->create_user(username);
    if (created.is_err()) {
        write_audit(actor, QStringLiteral("create_user"), QStringLiteral("user:%1").arg(username.trimmed().toLower()),
                    QStringLiteral("failure"));
        return fincept::Result<void>::err(created.error());
    }

    return write_audit(actor, QStringLiteral("create_user"), QStringLiteral("user:%1").arg(created.value()),
                       QStringLiteral("success"));
}

fincept::Result<void> PhaseOneUserAdminCommands::set_initial_password(int user_id, const QString& password,
                                                                      const QString& actor) const {
    const auto user = user_repository_->find_by_id(user_id);
    if (!user.has_value())
        return fincept::Result<void>::err("user_not_found");

    const auto result = user_repository_->set_initial_password(user_id, PhaseOnePasswordHasher::hash_password(password));
    if (result.is_err())
        return result;

    return write_audit(actor, QStringLiteral("set_initial_password"), QStringLiteral("user:%1").arg(user_id),
                       QStringLiteral("success"));
}

fincept::Result<void> PhaseOneUserAdminCommands::disable_user(int user_id, const QString& actor) const {
    const auto user = user_repository_->find_by_id(user_id);
    if (!user.has_value())
        return fincept::Result<void>::err("user_not_found");

    if (user->role == QStringLiteral("admin") && user->status == QStringLiteral("active")) {
        const auto admin_count = user_repository_->count_active_admins();
        if (admin_count.is_err())
            return fincept::Result<void>::err(admin_count.error());
        if (admin_count.value() <= 1) {
            write_audit(actor, QStringLiteral("disable_user"), QStringLiteral("user:%1").arg(user_id),
                        QStringLiteral("failure"));
            return fincept::Result<void>::err("sole_admin_disable_blocked");
        }
    }

    const auto result = user_repository_->update_status(user_id, QStringLiteral("disabled"));
    if (result.is_err())
        return result;

    return write_audit(actor, QStringLiteral("disable_user"), QStringLiteral("user:%1").arg(user_id),
                       QStringLiteral("success"));
}

fincept::Result<void> PhaseOneUserAdminCommands::transfer_admin(int target_user_id, const QString& actor) const {
    const auto target = user_repository_->find_by_id(target_user_id);
    if (!target.has_value() || target->status != QStringLiteral("active")) {
        write_audit(actor, QStringLiteral("transfer_admin"), QStringLiteral("user:%1").arg(target_user_id),
                    QStringLiteral("failure"));
        return fincept::Result<void>::err("invalid_admin_transfer");
    }

    auto& db = fincept::Database::instance();
    const auto begin = db.begin_transaction();
    if (begin.is_err())
        return fincept::Result<void>::err(begin.error());

    const auto clear = user_repository_->clear_admin_role();
    if (clear.is_err()) {
        db.rollback();
        return fincept::Result<void>::err(clear.error());
    }

    const auto set = user_repository_->set_admin_role(target_user_id);
    if (set.is_err()) {
        db.rollback();
        return fincept::Result<void>::err(set.error());
    }

    const auto commit = db.commit();
    if (commit.is_err()) {
        db.rollback();
        return fincept::Result<void>::err(commit.error());
    }

    return write_audit(actor, QStringLiteral("transfer_admin"), QStringLiteral("user:%1").arg(target_user_id),
                       QStringLiteral("success"));
}

fincept::Result<void> PhaseOneUserAdminCommands::write_audit(const QString& actor, const QString& action,
                                                             const QString& target, const QString& result_status) const {
    return audit_repository_->append_event(canonicalize_actor(actor), action, target, result_status);
}

QString PhaseOneUserAdminCommands::canonicalize_actor(const QString& actor) const {
    const QString canonical = actor.trimmed().toLower();
    return canonical.isEmpty() ? QStringLiteral("admin") : canonical;
}

} // namespace fincept::multiuser
