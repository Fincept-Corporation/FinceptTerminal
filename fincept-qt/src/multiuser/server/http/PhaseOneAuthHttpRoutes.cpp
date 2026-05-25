#include "multiuser/server/http/PhaseOneAuthHttpRoutes.h"

#include "multiuser/server/PhaseOneAuthServer.h"
#include "multiuser/server/http/PhaseOneHttpJsonResponse.h"
#include "multiuser/server/http/PhaseOneHttpRequestContext.h"
#include "multiuser/server/http/PhaseOneHttpServer.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace fincept::multiuser {

namespace {

PhaseOneHttpResponse json_response(int status_code, const QJsonObject& body) {
    return {status_code, {{"Content-Type", "application/json"}}, QJsonDocument(body).toJson(QJsonDocument::Compact)};
}

QJsonObject parse_auth_json_body(const PhaseOneHttpRequestContext& context, bool* ok) {
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

} // namespace

QString map_auth_error_code_to_message(const QString& error_code) {
    if (error_code == QStringLiteral("setup_required"))
        return QStringLiteral("Initial administrator setup is required.");
    if (error_code == QStringLiteral("invalid_credentials"))
        return QStringLiteral("Incorrect username or password.");
    if (error_code == QStringLiteral("session_invalid"))
        return QStringLiteral("Session is invalid.");
    if (error_code == QStringLiteral("session_expired"))
        return QStringLiteral("Session expired.");
    if (error_code == QStringLiteral("session_revoked"))
        return QStringLiteral("Session was revoked.");
    if (error_code == QStringLiteral("user_disabled"))
        return QStringLiteral("User is disabled.");
    if (error_code == QStringLiteral("forbidden"))
        return QStringLiteral("Forbidden.");
    return QStringLiteral("Request failed.");
}

void register_phase_one_auth_routes(PhaseOneHttpServer& http_server) {
    static PhaseOneAuthServer auth_server;
    register_phase_one_auth_routes(http_server, auth_server);
}

void register_phase_one_auth_routes(PhaseOneHttpServer& http_server, PhaseOneAuthServer& auth_server) {
    http_server.register_route("POST", QStringLiteral("/phase1/auth/login"),
                               [&auth_server](const PhaseOneHttpRequestContext& context) {
                                   bool body_ok = false;
                                   const QJsonObject body = parse_auth_json_body(context, &body_ok);
                                   if (!body_ok)
                                       return PhaseOneHttpJsonResponse::error(400, QStringLiteral("invalid_json"),
                                                                             QStringLiteral("Expected JSON object body."));

                                    const auto result = auth_server.login(body.value("username").toString(),
                                                                          body.value("password").toString());
                                    if (result.is_err())
                                        return json_response(401, { {"error_code", QString::fromStdString(result.error())},
                                                                    {"message", map_auth_error_code_to_message(QString::fromStdString(result.error()))} });
                                    return json_response(200, result.value().to_json());
                                });

    http_server.register_route("POST", QStringLiteral("/phase1/auth/logout"),
                               [&auth_server](const PhaseOneHttpRequestContext& context) {
                                    const QString session_id = context.bearer_session_token();
                                    const auto result = auth_server.logout(session_id);
                                    if (result.is_err())
                                        return json_response(401, { {"error_code", QString::fromStdString(result.error())},
                                                                    {"message", map_auth_error_code_to_message(QString::fromStdString(result.error()))} });
                                    return json_response(200, {});
                                });

    http_server.register_route("GET", QStringLiteral("/phase1/auth/session"),
                               [&auth_server](const PhaseOneHttpRequestContext& context) {
                                    const QString session_id = context.bearer_session_token();
                                    const auto result = auth_server.current_session(session_id);
                                    if (result.is_err())
                                        return json_response(401, { {"error_code", QString::fromStdString(result.error())},
                                                                    {"message", map_auth_error_code_to_message(QString::fromStdString(result.error()))} });
                                    return json_response(200, result.value().to_json());
                                });
}

} // namespace fincept::multiuser
