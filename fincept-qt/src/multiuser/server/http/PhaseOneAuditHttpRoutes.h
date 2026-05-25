#pragma once

namespace fincept::multiuser {

class PhaseOneAuditServer;
class PhaseOneAuthServer;
class PhaseOneHttpServer;

void register_phase_one_audit_routes(PhaseOneHttpServer& http_server);
void register_phase_one_audit_routes(PhaseOneHttpServer& http_server, PhaseOneAuditServer& audit_server);
void register_phase_one_audit_routes(PhaseOneHttpServer& http_server, PhaseOneAuditServer& audit_server,
                                     PhaseOneAuthServer& auth_server);

} // namespace fincept::multiuser
