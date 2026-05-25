#pragma once

namespace fincept::multiuser {

class PhaseOneAuthServer;
class PhaseOneHttpServer;
class PhaseOnePortfolioServer;

void register_phase_one_portfolio_routes(PhaseOneHttpServer& http_server);
void register_phase_one_portfolio_routes(PhaseOneHttpServer& http_server, PhaseOnePortfolioServer& portfolio_server);
void register_phase_one_portfolio_routes(PhaseOneHttpServer& http_server, PhaseOnePortfolioServer& portfolio_server,
                                         PhaseOneAuthServer& auth_server);

} // namespace fincept::multiuser
