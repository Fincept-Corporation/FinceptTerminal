#pragma once

#include <QString>

namespace fincept::multiuser {

class PhaseOneAuthServer;
class PhaseOneHttpServer;

void register_phase_one_auth_routes(PhaseOneHttpServer& http_server);
void register_phase_one_auth_routes(PhaseOneHttpServer& http_server, PhaseOneAuthServer& auth_server);
QString map_auth_error_code_to_message(const QString& error_code);

} // namespace fincept::multiuser
