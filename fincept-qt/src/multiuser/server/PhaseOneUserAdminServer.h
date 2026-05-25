#pragma once

#include "core/result/Result.h"
#include "multiuser/contracts/PhaseOneUserAdminTypes.h"
#include "multiuser/server/PhaseOneUserAdminCommands.h"
#include "multiuser/server/storage/PhaseOneAuditRepository.h"
#include "multiuser/server/storage/PhaseOneUserRepository.h"

#include <memory>

namespace fincept::multiuser {

class PhaseOneUserAdminServer {
  public:
    PhaseOneUserAdminServer();
    PhaseOneUserAdminServer(PhaseOneUserRepository* user_repository, PhaseOneAuditRepository* audit_repository);

    fincept::Result<PhaseOneBootstrapStatus> bootstrap_status() const;
    fincept::Result<void> bootstrap(const QString& username, const QString& password);
    fincept::Result<PhaseOneUserListResponse> list_users() const;
    fincept::Result<void> create_user(const QString& username);
    fincept::Result<void> set_initial_password(int user_id, const QString& password);
    fincept::Result<void> disable_user(int user_id);
    fincept::Result<void> transfer_admin(int target_user_id);

  private:
    PhaseOneUserRepository owned_user_repository_;
    PhaseOneAuditRepository owned_audit_repository_;
    PhaseOneUserRepository* user_repository_ = nullptr;
    PhaseOneAuditRepository* audit_repository_ = nullptr;
    std::unique_ptr<PhaseOneUserAdminCommands> commands_;
};

} // namespace fincept::multiuser
