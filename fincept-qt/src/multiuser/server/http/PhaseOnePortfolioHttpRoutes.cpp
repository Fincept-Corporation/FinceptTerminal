#include "multiuser/server/http/PhaseOnePortfolioHttpRoutes.h"

#include "multiuser/server/PhaseOneAuthServer.h"
#include "multiuser/server/PhaseOnePortfolioServer.h"
#include "multiuser/server/http/PhaseOneAuthHttpRoutes.h"
#include "multiuser/server/http/PhaseOneHttpJsonResponse.h"
#include "multiuser/server/http/PhaseOneHttpRequestContext.h"
#include "multiuser/server/http/PhaseOneHttpServer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QUrlQuery>

namespace fincept::multiuser {

namespace {

PhaseOneHttpResponse portfolio_json_response(int status_code, const QJsonObject& body) {
    return {status_code, {{"Content-Type", "application/json"}}, QJsonDocument(body).toJson(QJsonDocument::Compact)};
}

QJsonObject parse_json_body(const PhaseOneHttpRequestContext& context, bool* ok) {
    *ok = false;
    if (context.body().isEmpty())
        return {};

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(context.body(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return {};

    *ok = true;
    return document.object();
}

QJsonObject to_json(const PhaseOnePortfolioRecord& record) {
    return record.to_json();
}

QJsonObject to_json(const PhaseOnePortfolioListResponse& response) {
    QJsonArray portfolios;
    for (const auto& record : response.portfolios)
        portfolios.append(record.to_json());
    return QJsonObject{{"portfolios", portfolios}};
}

QJsonObject to_json(const PhaseOneHoldingRecord& record) {
    return record.to_json();
}

QJsonObject to_json(const PhaseOneHoldingsListResponse& response) {
    QJsonArray holdings;
    for (const auto& record : response.holdings)
        holdings.append(record.to_json());
    return QJsonObject{{"holdings", holdings}};
}

std::optional<PhaseOneSessionInfo> require_authenticated(const PhaseOneHttpRequestContext& context,
                                                         PhaseOneAuthServer& auth_server,
                                                         PhaseOneHttpResponse* response) {
    const auto session = auth_server.current_session(context.bearer_session_token());
    if (session.is_err()) {
        *response = portfolio_json_response(401, QJsonObject{{"error_code", QString::fromStdString(session.error())},
                                                             {"message", map_auth_error_code_to_message(QString::fromStdString(session.error()))}});
        return std::nullopt;
    }
    return session.value();
}

QString query_value(const QString& path, const QString& key) {
    const QString query_string = path.section('?', 1);
    if (query_string.isEmpty())
        return {};
    return QUrlQuery(query_string).queryItemValue(key);
}

} // namespace

void register_phase_one_portfolio_routes(PhaseOneHttpServer& http_server) {
    static PhaseOnePortfolioServer portfolio_server;
    static PhaseOneAuthServer auth_server;
    register_phase_one_portfolio_routes(http_server, portfolio_server, auth_server);
}

void register_phase_one_portfolio_routes(PhaseOneHttpServer& http_server, PhaseOnePortfolioServer& portfolio_server) {
    static PhaseOneAuthServer auth_server;
    register_phase_one_portfolio_routes(http_server, portfolio_server, auth_server);
}

void register_phase_one_portfolio_routes(PhaseOneHttpServer& http_server, PhaseOnePortfolioServer& portfolio_server,
                                         PhaseOneAuthServer& auth_server) {
    http_server.register_route("GET", QStringLiteral("/phase1/portfolios"),
                               [&portfolio_server, &auth_server](const PhaseOneHttpRequestContext& context) {
                                   PhaseOneHttpResponse denied;
                                   if (!require_authenticated(context, auth_server, &denied).has_value())
                                       return denied;

                                   const QString portfolio_id = query_value(context.path(), QStringLiteral("portfolio_id"));
                                   if (portfolio_id.isEmpty()) {
                                       const auto result = portfolio_server.list_portfolios();
                                       if (result.is_err()) {
                                           return portfolio_json_response(500, {{"error_code", QStringLiteral("portfolio_list_failed")},
                                                                                {"message", QString::fromStdString(result.error())}});
                                       }
                                       return portfolio_json_response(200, to_json(result.value()));
                                   }

                                   const auto result = portfolio_server.fetch_portfolio(portfolio_id);
                                   if (result.is_err()) {
                                       return portfolio_json_response(404, {{"error_code", QStringLiteral("portfolio_not_found")},
                                                                            {"message", QString::fromStdString(result.error())}});
                                   }
                                   return portfolio_json_response(200, {{"portfolio", to_json(result.value())}});
                               });

    http_server.register_route("POST", QStringLiteral("/phase1/portfolios/create"),
                               [&portfolio_server, &auth_server](const PhaseOneHttpRequestContext& context) {
                                   PhaseOneHttpResponse denied;
                                   const auto actor = require_authenticated(context, auth_server, &denied);
                                   if (!actor.has_value())
                                       return denied;
                                   bool body_ok = false;
                                   const QJsonObject body = parse_json_body(context, &body_ok);
                                   if (!body_ok)
                                       return PhaseOneHttpJsonResponse::error(400, QStringLiteral("invalid_json"),
                                                                             QStringLiteral("Expected JSON object body."));
                                   PhaseOneCreatePortfolioRequest request;
                                   request.name = body.value("name").toString();
                                   request.owner = body.value("owner").toString();
                                   request.currency = body.value("currency").toString(QStringLiteral("USD"));
                                   request.description = body.value("description").toString();
                                   request.broker_account_id = body.value("broker_account_id").toString();
                                   const auto result = portfolio_server.create_portfolio(*actor, request);
                                   if (result.is_err()) {
                                       return portfolio_json_response(400, {{"error_code", QStringLiteral("portfolio_create_failed")},
                                                                            {"message", QString::fromStdString(result.error())}});
                                   }
                                   return portfolio_json_response(200, {{"portfolio", to_json(result.value())}});
                               });

    http_server.register_route("POST", QStringLiteral("/phase1/portfolios/update"),
                               [&portfolio_server, &auth_server](const PhaseOneHttpRequestContext& context) {
                                   PhaseOneHttpResponse denied;
                                   const auto actor = require_authenticated(context, auth_server, &denied);
                                   if (!actor.has_value())
                                       return denied;
                                   bool body_ok = false;
                                   const QJsonObject body = parse_json_body(context, &body_ok);
                                   if (!body_ok)
                                       return PhaseOneHttpJsonResponse::error(400, QStringLiteral("invalid_json"),
                                                                             QStringLiteral("Expected JSON object body."));
                                   PhaseOneUpdatePortfolioRequest request;
                                   request.id = body.value("id").toString();
                                   request.name = body.value("name").toString();
                                   request.owner = body.value("owner").toString();
                                   request.currency = body.value("currency").toString(QStringLiteral("USD"));
                                   request.description = body.value("description").toString();
                                   const auto result = portfolio_server.update_portfolio(*actor, request);
                                   if (result.is_err()) {
                                       return portfolio_json_response(404, {{"error_code", QStringLiteral("portfolio_update_failed")},
                                                                            {"message", QString::fromStdString(result.error())}});
                                   }
                                   return portfolio_json_response(200, {{"portfolio", to_json(result.value())}});
                               });

    http_server.register_route("POST", QStringLiteral("/phase1/portfolios/delete"),
                               [&portfolio_server, &auth_server](const PhaseOneHttpRequestContext& context) {
                                   PhaseOneHttpResponse denied;
                                   const auto actor = require_authenticated(context, auth_server, &denied);
                                   if (!actor.has_value())
                                       return denied;
                                   bool body_ok = false;
                                   const QJsonObject body = parse_json_body(context, &body_ok);
                                   if (!body_ok)
                                       return PhaseOneHttpJsonResponse::error(400, QStringLiteral("invalid_json"),
                                                                             QStringLiteral("Expected JSON object body."));
                                   const auto result = portfolio_server.delete_portfolio(*actor, body.value("id").toString());
                                   if (result.is_err()) {
                                       return portfolio_json_response(404, {{"error_code", QStringLiteral("portfolio_delete_failed")},
                                                                            {"message", QString::fromStdString(result.error())}});
                                   }
                                   return portfolio_json_response(200, {});
                               });

    http_server.register_route("GET", QStringLiteral("/phase1/holdings"),
                               [&portfolio_server, &auth_server](const PhaseOneHttpRequestContext& context) {
                                   PhaseOneHttpResponse denied;
                                   if (!require_authenticated(context, auth_server, &denied).has_value())
                                       return denied;
                                    const auto result =
                                        portfolio_server.list_holdings(query_value(context.path(), QStringLiteral("portfolio_id")));
                                    if (result.is_err()) {
                                        const int status = result.error() == std::string("portfolio_not_found")
                                                               ? 404
                                                               : (result.error() == std::string("portfolio_id_required") ? 400 : 500);
                                        return portfolio_json_response(status, {{"error_code", QStringLiteral("holdings_list_failed")},
                                                                             {"message", QString::fromStdString(result.error())}});
                                    }
                                    return portfolio_json_response(200, to_json(result.value()));
                               });

    http_server.register_route("POST", QStringLiteral("/phase1/holdings/create"),
                               [&portfolio_server, &auth_server](const PhaseOneHttpRequestContext& context) {
                                   PhaseOneHttpResponse denied;
                                   const auto actor = require_authenticated(context, auth_server, &denied);
                                   if (!actor.has_value())
                                       return denied;
                                   bool body_ok = false;
                                   const QJsonObject body = parse_json_body(context, &body_ok);
                                   if (!body_ok)
                                       return PhaseOneHttpJsonResponse::error(400, QStringLiteral("invalid_json"),
                                                                             QStringLiteral("Expected JSON object body."));
                                   PhaseOneCreateHoldingRequest request;
                                   request.portfolio_id = body.value("portfolio_id").toString();
                                   request.symbol = body.value("symbol").toString();
                                   request.name = body.value("name").toString();
                                   request.shares = body.value("shares").toDouble();
                                   request.avg_cost = body.value("avg_cost").toDouble();
                                   request.acquired_at = body.value("acquired_at").toString();
                                   request.sector = body.value("sector").toString();
                                   request.broker_symbol = body.value("broker_symbol").toString();
                                   request.exchange = body.value("exchange").toString();
                                    const auto result = portfolio_server.create_holding(*actor, request);
                                    if (result.is_err()) {
                                        const int status = result.error() == std::string("portfolio_not_found") ? 404 : 400;
                                        return portfolio_json_response(status, {{"error_code", QStringLiteral("holding_create_failed")},
                                                                             {"message", QString::fromStdString(result.error())}});
                                    }
                                    return portfolio_json_response(200, {{"holding", to_json(result.value())}});
                               });

    http_server.register_route("POST", QStringLiteral("/phase1/holdings/update"),
                               [&portfolio_server, &auth_server](const PhaseOneHttpRequestContext& context) {
                                   PhaseOneHttpResponse denied;
                                   const auto actor = require_authenticated(context, auth_server, &denied);
                                   if (!actor.has_value())
                                       return denied;
                                   bool body_ok = false;
                                   const QJsonObject body = parse_json_body(context, &body_ok);
                                   if (!body_ok)
                                       return PhaseOneHttpJsonResponse::error(400, QStringLiteral("invalid_json"),
                                                                             QStringLiteral("Expected JSON object body."));
                                   PhaseOneUpdateHoldingRequest request;
                                   request.id = body.value("id").toInt();
                                   request.portfolio_id = body.value("portfolio_id").toString();
                                   request.shares = body.value("shares").toDouble();
                                   request.avg_cost = body.value("avg_cost").toDouble();
                                   request.sector = body.value("sector").toString();
                                    const auto result = portfolio_server.update_holding(*actor, request);
                                    if (result.is_err()) {
                                        const int status = result.error() == std::string("portfolio_id_required") ? 400 : 404;
                                        return portfolio_json_response(status, {{"error_code", QStringLiteral("holding_update_failed")},
                                                                             {"message", QString::fromStdString(result.error())}});
                                    }
                                    return portfolio_json_response(200, {{"holding", to_json(result.value())}});
                               });

    http_server.register_route("POST", QStringLiteral("/phase1/holdings/remove"),
                               [&portfolio_server, &auth_server](const PhaseOneHttpRequestContext& context) {
                                   PhaseOneHttpResponse denied;
                                   const auto actor = require_authenticated(context, auth_server, &denied);
                                   if (!actor.has_value())
                                       return denied;
                                   bool body_ok = false;
                                   const QJsonObject body = parse_json_body(context, &body_ok);
                                   if (!body_ok)
                                       return PhaseOneHttpJsonResponse::error(400, QStringLiteral("invalid_json"),
                                                                             QStringLiteral("Expected JSON object body."));
                                   PhaseOneRemoveHoldingRequest request;
                                   request.id = body.value("id").toInt();
                                   request.portfolio_id = body.value("portfolio_id").toString();
                                    const auto result = portfolio_server.remove_holding(*actor, request);
                                    if (result.is_err()) {
                                        const int status = result.error() == std::string("portfolio_id_required") ? 400 : 404;
                                        return portfolio_json_response(status, {{"error_code", QStringLiteral("holding_delete_failed")},
                                                                             {"message", QString::fromStdString(result.error())}});
                                    }
                                    return portfolio_json_response(200, {});
                               });
}

} // namespace fincept::multiuser
