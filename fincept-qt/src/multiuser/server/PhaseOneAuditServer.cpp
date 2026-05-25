#include "multiuser/server/PhaseOneAuditServer.h"

namespace fincept::multiuser {

PhaseOneAuditServer::PhaseOneAuditServer() : PhaseOneAuditServer(&owned_audit_repository_) {}

PhaseOneAuditServer::PhaseOneAuditServer(PhaseOneAuditRepository* audit_repository)
    : audit_repository_(audit_repository) {}

fincept::Result<PhaseOneAuditListResponse> PhaseOneAuditServer::list_audit_events(const PhaseOneAuditFilter& filter) const {
    const auto result = audit_repository_->list_events(filter);
    if (result.is_err())
        return fincept::Result<PhaseOneAuditListResponse>::err(result.error());

    PhaseOneAuditListResponse response;
    response.audit_events = result.value();
    return fincept::Result<PhaseOneAuditListResponse>::ok(response);
}

} // namespace fincept::multiuser
