#include "multiuser/client/PhaseOnePortfolioApi.h"

#include "multiuser/client/PhaseOneClientTransport.h"
#include "multiuser/client/PhaseOneSessionStateGuard.h"

namespace fincept::multiuser {

namespace {

QString session_id() {
    return PhaseOneClientTransport::instance().session_id();
}

QString append_query(const QString& base_path, const QString& query) {
    return query.isEmpty() ? base_path : base_path + '?' + query;
}

} // namespace

PhaseOnePortfolioApi& PhaseOnePortfolioApi::instance() {
    static PhaseOnePortfolioApi api;
    return api;
}

void PhaseOnePortfolioApi::list_portfolios(Callback cb) {
    PhaseOneClientTransport::instance().get(QStringLiteral("/phase1/portfolios"), session_id(),
                                            [cb = std::move(cb)](const PhaseOneTransportResponse& response) {
                                                cb(PhaseOneSessionStateGuard::map_authenticated_response(response));
                                            });
}

void PhaseOnePortfolioApi::fetch_portfolio(const QString& portfolio_id, Callback cb) {
    PhaseOneHoldingQuery query;
    query.portfolio_id = portfolio_id;
    PhaseOneClientTransport::instance().get(append_query(QStringLiteral("/phase1/portfolios"), query.to_query_string()),
                                            session_id(), [cb = std::move(cb)](const PhaseOneTransportResponse& response) {
                                                cb(PhaseOneSessionStateGuard::map_authenticated_response(response));
                                            });
}

void PhaseOnePortfolioApi::create_portfolio(const PhaseOneCreatePortfolioRequest& request, Callback cb) {
    PhaseOneClientTransport::instance().post(QStringLiteral("/phase1/portfolios/create"), request.to_json(), session_id(),
                                             [cb = std::move(cb)](const PhaseOneTransportResponse& response) {
                                                 cb(PhaseOneSessionStateGuard::map_authenticated_response(response));
                                             });
}

void PhaseOnePortfolioApi::update_portfolio(const PhaseOneUpdatePortfolioRequest& request, Callback cb) {
    PhaseOneClientTransport::instance().post(QStringLiteral("/phase1/portfolios/update"), request.to_json(), session_id(),
                                             [cb = std::move(cb)](const PhaseOneTransportResponse& response) {
                                                 cb(PhaseOneSessionStateGuard::map_authenticated_response(response));
                                             });
}

void PhaseOnePortfolioApi::delete_portfolio(const QString& portfolio_id, Callback cb) {
    PhaseOneDeletePortfolioRequest request{portfolio_id};
    PhaseOneClientTransport::instance().post(QStringLiteral("/phase1/portfolios/delete"), request.to_json(), session_id(),
                                             [cb = std::move(cb)](const PhaseOneTransportResponse& response) {
                                                 cb(PhaseOneSessionStateGuard::map_authenticated_response(response));
                                             });
}

void PhaseOnePortfolioApi::list_holdings(const QString& portfolio_id, Callback cb) {
    PhaseOneHoldingQuery query;
    query.portfolio_id = portfolio_id;
    PhaseOneClientTransport::instance().get(append_query(QStringLiteral("/phase1/holdings"), query.to_query_string()),
                                            session_id(), [cb = std::move(cb)](const PhaseOneTransportResponse& response) {
                                                cb(PhaseOneSessionStateGuard::map_authenticated_response(response));
                                            });
}

void PhaseOnePortfolioApi::create_holding(const PhaseOneCreateHoldingRequest& request, Callback cb) {
    PhaseOneClientTransport::instance().post(QStringLiteral("/phase1/holdings/create"), request.to_json(), session_id(),
                                             [cb = std::move(cb)](const PhaseOneTransportResponse& response) {
                                                 cb(PhaseOneSessionStateGuard::map_authenticated_response(response));
                                             });
}

void PhaseOnePortfolioApi::update_holding(const PhaseOneUpdateHoldingRequest& request, Callback cb) {
    PhaseOneClientTransport::instance().post(QStringLiteral("/phase1/holdings/update"), request.to_json(), session_id(),
                                             [cb = std::move(cb)](const PhaseOneTransportResponse& response) {
                                                 cb(PhaseOneSessionStateGuard::map_authenticated_response(response));
                                             });
}

void PhaseOnePortfolioApi::remove_holding(const PhaseOneRemoveHoldingRequest& request, Callback cb) {
    PhaseOneClientTransport::instance().post(QStringLiteral("/phase1/holdings/remove"), request.to_json(), session_id(),
                                             [cb = std::move(cb)](const PhaseOneTransportResponse& response) {
                                                 cb(PhaseOneSessionStateGuard::map_authenticated_response(response));
                                             });
}

} // namespace fincept::multiuser
