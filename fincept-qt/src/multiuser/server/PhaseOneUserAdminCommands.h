#pragma once

#include "core/result/Result.h"
#include "multiuser/server/storage/PhaseOneAuditRepository.h"
#include "multiuser/server/storage/PhaseOneUserRepository.h"

namespace fincept::multiuser {

class PhaseOneUserAdminCommands {
  public:
    PhaseOneUserAdminCommands(PhaseOneUserRepository* user_repository, PhaseOneAuditRepository* audit_repository);

    fincept::Result<void> bootstrap(const QString& username, const QString& password) const;
    fincept::Result<void> create_user(const QString& username, const QString& actor = QStringLiteral("admin")) const;
    fincept::Result<void> set_initial_password(int user_id, const QString& password,
                                               const QString& actor = QStringLiteral("admin")) const;
    fincept::Result<void> disable_user(int user_id, const QString& actor = QStringLiteral("admin")) const;
    fincept::Result<void> transfer_admin(int target_user_id, const QString& actor = QStringLiteral("admin")) const;

  private:
    fincept::Result<void> write_audit(const QString& actor, const QString& action, const QString& target,
                                      const QString& result_status) const;
    QString canonicalize_actor(const QString& actor) const;

    PhaseOneUserRepository* user_repository_ = nullptr;
    PhaseOneAuditRepository* audit_repository_ = nullptr;
};

} // namespace fincept::multiuser
