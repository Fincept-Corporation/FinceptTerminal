#include "multiuser/server/PhaseOneUserAdminServer.h"

#include "core/logging/Logger.h"

namespace fincept::multiuser {

namespace {

constexpr auto kUserAdminServerTag = "Phase1UserAdminServer";

} // namespace

PhaseOneUserAdminServer::PhaseOneUserAdminServer()
    : PhaseOneUserAdminServer(&owned_user_repository_, &owned_audit_repository_) {}

PhaseOneUserAdminServer::PhaseOneUserAdminServer(PhaseOneUserRepository* user_repository,
                                                 PhaseOneAuditRepository* audit_repository)
    : user_repository_(user_repository), audit_repository_(audit_repository),
      commands_(std::make_unique<PhaseOneUserAdminCommands>(user_repository, audit_repository)) {}

fincept::Result<PhaseOneBootstrapStatus> PhaseOneUserAdminServer::bootstrap_status() const {
    const auto result = user_repository_->count_users();
    if (result.is_err()) {
        LOG_WARN(kUserAdminServerTag,
                 QStringLiteral("Failed to resolve bootstrap status: %1")
                     .arg(QString::fromStdString(result.error())));
        return fincept::Result<PhaseOneBootstrapStatus>::ok({false});
    }

    return fincept::Result<PhaseOneBootstrapStatus>::ok({result.value() == 0});
}

fincept::Result<void> PhaseOneUserAdminServer::bootstrap(const QString& username, const QString& password) {
    return commands_->bootstrap(username, password);
}

fincept::Result<PhaseOneUserListResponse> PhaseOneUserAdminServer::list_users() const {
    const auto result = user_repository_->list_users();
    if (result.is_err())
        return fincept::Result<PhaseOneUserListResponse>::err(result.error());

    PhaseOneUserListResponse response;
    response.users.reserve(result.value().size());
    for (const PhaseOneStoredUser& user : result.value())
        response.users.append(user.to_summary());
    return fincept::Result<PhaseOneUserListResponse>::ok(response);
}

fincept::Result<void> PhaseOneUserAdminServer::create_user(const QString& username, const QString& actor) {
    return commands_->create_user(username, actor);
}

fincept::Result<void> PhaseOneUserAdminServer::set_initial_password(int user_id, const QString& password,
                                                                    const QString& actor) {
    return commands_->set_initial_password(user_id, password, actor);
}

fincept::Result<void> PhaseOneUserAdminServer::disable_user(int user_id, const QString& actor) {
    return commands_->disable_user(user_id, actor);
}

fincept::Result<void> PhaseOneUserAdminServer::transfer_admin(int target_user_id, const QString& actor) {
    return commands_->transfer_admin(target_user_id, actor);
}

} // namespace fincept::multiuser
