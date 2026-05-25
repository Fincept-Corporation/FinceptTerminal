#pragma once

#include "core/result/Result.h"
#include "multiuser/contracts/PhaseOneAuthTypes.h"
#include "multiuser/server/storage/PhaseOneAuditRepository.h"
#include "multiuser/server/storage/PhaseOneSessionRepository.h"
#include "multiuser/server/storage/PhaseOneUserRepository.h"

namespace fincept::multiuser {

class PhaseOneAuthServer {
  public:
    PhaseOneAuthServer();
    PhaseOneAuthServer(PhaseOneUserRepository* user_repository, PhaseOneSessionRepository* session_repository,
                       PhaseOneAuditRepository* audit_repository);

    fincept::Result<PhaseOneSessionInfo> login(const QString& username, const QString& password);
    fincept::Result<void> logout(const QString& session_id);
    fincept::Result<PhaseOneSessionInfo> current_session(const QString& session_id) const;

  private:
    fincept::Result<void> write_audit_event(const QString& actor, const QString& action_type, const QString& target,
                                            const QString& result_status) const;
    fincept::Result<void> write_session_event(const QString& actor, const QString& action_type, const QString& session_id,
                                              const QString& result_status) const;
    fincept::Result<PhaseOneSessionInfo> replace_session_for_user(const PhaseOneStoredUser& user) const;
    fincept::Result<void> invalidate_session_with_audit(const PhaseOneStoredSession& session, const QString& actor,
                                                        const QString& reason) const;

    PhaseOneUserRepository owned_user_repository_;
    PhaseOneSessionRepository owned_session_repository_;
    PhaseOneAuditRepository owned_audit_repository_;
    PhaseOneUserRepository* user_repository_ = nullptr;
    PhaseOneSessionRepository* session_repository_ = nullptr;
    PhaseOneAuditRepository* audit_repository_ = nullptr;
};

} // namespace fincept::multiuser
