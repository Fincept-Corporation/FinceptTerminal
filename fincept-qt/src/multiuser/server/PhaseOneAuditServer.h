#pragma once

#include "core/result/Result.h"
#include "multiuser/contracts/PhaseOneAuditTypes.h"
#include "multiuser/server/storage/PhaseOneAuditRepository.h"

namespace fincept::multiuser {

class PhaseOneAuditServer {
  public:
    PhaseOneAuditServer();
    explicit PhaseOneAuditServer(PhaseOneAuditRepository* audit_repository);

    fincept::Result<PhaseOneAuditListResponse> list_audit_events(const PhaseOneAuditFilter& filter) const;

  private:
    PhaseOneAuditRepository owned_audit_repository_;
    PhaseOneAuditRepository* audit_repository_ = nullptr;
};

} // namespace fincept::multiuser
