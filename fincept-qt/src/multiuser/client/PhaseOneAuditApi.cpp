#include "multiuser/client/PhaseOneAuditApi.h"

#include "multiuser/client/PhaseOneClientTransport.h"
#include "multiuser/client/PhaseOneSessionStateGuard.h"

namespace fincept::multiuser {

PhaseOneAuditApi& PhaseOneAuditApi::instance() {
    static PhaseOneAuditApi api;
    return api;
}

void PhaseOneAuditApi::list_audit_events(const PhaseOneAuditFilter& filter, Callback cb) {
    QString path = "/phase1/admin/audit-events";
    const QString query = filter.to_query_string();
    if (!query.isEmpty())
        path += '?' + query;

    PhaseOneClientTransport::instance().get(path, PhaseOneClientTransport::instance().session_id(),
                                             [cb = std::move(cb)](const PhaseOneTransportResponse& response) {
                                                 cb(PhaseOneSessionStateGuard::map_authenticated_response(response));
                                             });
}

} // namespace fincept::multiuser
