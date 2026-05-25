#pragma once

namespace fincept::multiuser {

class PhaseOneHttpServer;
class PhaseOneAuthServer;
class PhaseOneUserAdminServer;

void register_phase_one_user_admin_routes(PhaseOneHttpServer& http_server);
void register_phase_one_user_admin_routes(PhaseOneHttpServer& http_server, PhaseOneUserAdminServer& user_admin_server);
void register_phase_one_user_admin_routes(PhaseOneHttpServer& http_server, PhaseOneUserAdminServer& user_admin_server,
                                          PhaseOneAuthServer& auth_server);

} // namespace fincept::multiuser
