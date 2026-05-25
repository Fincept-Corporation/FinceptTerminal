#include "multiuser/server/http/PhaseOneUserAdminHttpRoutes.h"

#include "multiuser/server/PhaseOneAuthServer.h"
#include "multiuser/server/PhaseOneUserAdminServer.h"
#include "multiuser/server/http/PhaseOneAuthHttpRoutes.h"
#include "multiuser/server/http/PhaseOneHttpJsonResponse.h"
#include "multiuser/server/http/PhaseOneHttpRequestContext.h"
#include "multiuser/server/http/PhaseOneHttpServer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

namespace fincept::multiuser {

namespace {

PhaseOneHttpResponse json_response(int status_code, const QJsonObject& body) {
    return {status_code, {{"Content-Type", "application/json"}}, QJsonDocument(body).toJson(QJsonDocument::Compact)};
}

QJsonObject parse_user_admin_json_body(const PhaseOneHttpRequestContext& context, bool* ok) {
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

QJsonObject to_json(const PhaseOneBootstrapStatus& status) {
    return QJsonObject{{"bootstrap_open", status.bootstrap_open}};
}

QJsonObject to_json(const PhaseOneUserSummary& user) {
    return QJsonObject{{"user_id", user.user_id},
                       {"username", user.username},
                       {"role", user.role},
                       {"status", user.status}};
}

QJsonObject to_json(const PhaseOneUserListResponse& response) {
    QJsonArray users;
    for (const PhaseOneUserSummary& user : response.users)
        users.append(to_json(user));
    return QJsonObject{{"users", users}};
}

std::optional<PhaseOneSessionInfo> require_admin(const PhaseOneHttpRequestContext& context, PhaseOneAuthServer& auth_server,
                                                 PhaseOneHttpResponse* response) {
    const auto session = auth_server.current_session(context.bearer_session_token());
    if (session.is_err()) {
        *response = json_response(401, QJsonObject{{"error_code", QString::fromStdString(session.error())},
                                                   {"message", map_auth_error_code_to_message(QString::fromStdString(session.error()))}});
        return std::nullopt;
    }
    if (session.value().role != QStringLiteral("admin")) {
        *response = json_response(403, QJsonObject{{"error_code", QStringLiteral("forbidden")},
                                                   {"message", map_auth_error_code_to_message(QStringLiteral("forbidden"))}});
        return std::nullopt;
    }
    return session.value();
}

} // namespace

void register_phase_one_user_admin_routes(PhaseOneHttpServer& http_server) {
    static PhaseOneUserAdminServer user_admin_server;
    static PhaseOneAuthServer auth_server;
    register_phase_one_user_admin_routes(http_server, user_admin_server, auth_server);
}

void register_phase_one_user_admin_routes(PhaseOneHttpServer& http_server, PhaseOneUserAdminServer& user_admin_server) {
    static PhaseOneAuthServer auth_server;
    register_phase_one_user_admin_routes(http_server, user_admin_server, auth_server);
}

void register_phase_one_user_admin_routes(PhaseOneHttpServer& http_server, PhaseOneUserAdminServer& user_admin_server,
                                          PhaseOneAuthServer& auth_server) {
    http_server.register_route("GET", QStringLiteral("/phase1/admin/bootstrap-status"),
                               [&user_admin_server](const PhaseOneHttpRequestContext&) {
                                    const auto result = user_admin_server.bootstrap_status();
                                    if (result.is_err()) {
                                        return json_response(500, {{"error_code", QStringLiteral("bootstrap_status_failed")},
                                                                   {"message", QString::fromStdString(result.error())}});
                                    }
                                    return json_response(200, to_json(result.value()));
                                });

    http_server.register_route("POST", QStringLiteral("/phase1/admin/bootstrap"),
                               [&user_admin_server](const PhaseOneHttpRequestContext& context) {
                                   bool body_ok = false;
                                   const QJsonObject body = parse_user_admin_json_body(context, &body_ok);
                                   if (!body_ok)
                                       return PhaseOneHttpJsonResponse::error(400, QStringLiteral("invalid_json"),
                                                                             QStringLiteral("Expected JSON object body."));

                                    const auto result = user_admin_server.bootstrap(body.value("username").toString(),
                                                                                    body.value("password").toString());
                                    if (result.is_err()) {
                                        const QString code = QString::fromStdString(result.error());
                                        const int status = code == QStringLiteral("bootstrap_closed") ? 409 : 400;
                                        return json_response(status, {{"error_code", code}, {"message", code}});
                                    }
                                    return json_response(200, {});
                                });

    http_server.register_route("GET", QStringLiteral("/phase1/admin/users"),
                               [&user_admin_server, &auth_server](const PhaseOneHttpRequestContext& context) {
                                    PhaseOneHttpResponse denied;
                                    if (!require_admin(context, auth_server, &denied).has_value())
                                        return denied;
                                    const auto result = user_admin_server.list_users();
                                    if (result.is_err()) {
                                        return json_response(500, {{"error_code", QStringLiteral("list_users_failed")},
                                                                   {"message", QString::fromStdString(result.error())}});
                                    }
                                    return json_response(200, to_json(result.value()));
                                });

    http_server.register_route("POST", QStringLiteral("/phase1/admin/users"),
                               [&user_admin_server, &auth_server](const PhaseOneHttpRequestContext& context) {
                                    PhaseOneHttpResponse denied;
                                    if (!require_admin(context, auth_server, &denied).has_value())
                                        return denied;
                                    bool body_ok = false;
                                   const QJsonObject body = parse_user_admin_json_body(context, &body_ok);
                                   if (!body_ok)
                                       return PhaseOneHttpJsonResponse::error(400, QStringLiteral("invalid_json"),
                                                                             QStringLiteral("Expected JSON object body."));

                                     const auto admin = require_admin(context, auth_server, &denied);
                                     if (!admin.has_value())
                                         return denied;
                                     const auto result = user_admin_server.create_user(body.value("username").toString(),
                                                                                       admin->username);
                                     if (result.is_err()) {
                                         const QString code = QString::fromStdString(result.error());
                                         const int status = (code == QStringLiteral("active_user_cap_reached") ||
                                                             code == QStringLiteral("user_conflict"))
                                                                ? 409
                                                                : 400;
                                         return json_response(status, {{"error_code", code}, {"message", code}});
                                     }
                                     return json_response(200, {});
                                });

    http_server.register_route("POST", QStringLiteral("/phase1/admin/users/set-initial-password"),
                               [&user_admin_server, &auth_server](const PhaseOneHttpRequestContext& context) {
                                    PhaseOneHttpResponse denied;
                                    if (!require_admin(context, auth_server, &denied).has_value())
                                        return denied;
                                    bool body_ok = false;
                                   const QJsonObject body = parse_user_admin_json_body(context, &body_ok);
                                   if (!body_ok)
                                       return PhaseOneHttpJsonResponse::error(400, QStringLiteral("invalid_json"),
                                                                             QStringLiteral("Expected JSON object body."));

                                     const auto admin = require_admin(context, auth_server, &denied);
                                     if (!admin.has_value())
                                         return denied;
                                     const auto result = user_admin_server.set_initial_password(body.value("user_id").toInt(),
                                                                                                body.value("password").toString(),
                                                                                                admin->username);
                                     if (result.is_err()) {
                                         const QString code = QString::fromStdString(result.error());
                                         const int status = code == QStringLiteral("user_not_found") ? 404 : 400;
                                         return json_response(status, {{"error_code", code}, {"message", code}});
                                     }
                                    return json_response(200, {});
                                });

    http_server.register_route("POST", QStringLiteral("/phase1/admin/users/disable"),
                               [&user_admin_server, &auth_server](const PhaseOneHttpRequestContext& context) {
                                    PhaseOneHttpResponse denied;
                                    if (!require_admin(context, auth_server, &denied).has_value())
                                        return denied;
                                    bool body_ok = false;
                                   const QJsonObject body = parse_user_admin_json_body(context, &body_ok);
                                   if (!body_ok)
                                       return PhaseOneHttpJsonResponse::error(400, QStringLiteral("invalid_json"),
                                                                             QStringLiteral("Expected JSON object body."));

                                     const auto admin = require_admin(context, auth_server, &denied);
                                     if (!admin.has_value())
                                         return denied;
                                     const auto result = user_admin_server.disable_user(body.value("user_id").toInt(),
                                                                                         admin->username);
                                    if (result.is_err()) {
                                        const QString code = QString::fromStdString(result.error());
                                        const int status = code == QStringLiteral("user_not_found") ? 404 : 409;
                                        return json_response(status, {{"error_code", code}, {"message", code}});
                                    }
                                    return json_response(200, {});
                                });

    http_server.register_route("POST", QStringLiteral("/phase1/admin/users/transfer-admin"),
                               [&user_admin_server, &auth_server](const PhaseOneHttpRequestContext& context) {
                                    PhaseOneHttpResponse denied;
                                    if (!require_admin(context, auth_server, &denied).has_value())
                                        return denied;
                                    bool body_ok = false;
                                   const QJsonObject body = parse_user_admin_json_body(context, &body_ok);
                                   if (!body_ok)
                                       return PhaseOneHttpJsonResponse::error(400, QStringLiteral("invalid_json"),
                                                                             QStringLiteral("Expected JSON object body."));

                                     const auto admin = require_admin(context, auth_server, &denied);
                                     if (!admin.has_value())
                                         return denied;
                                     const auto result = user_admin_server.transfer_admin(body.value("target_user_id").toInt(),
                                                                                            admin->username);
                                    if (result.is_err()) {
                                        const QString code = QString::fromStdString(result.error());
                                        const int status = code == QStringLiteral("user_not_found") ? 404 : 409;
                                        return json_response(status, {{"error_code", code}, {"message", code}});
                                    }
                                    return json_response(200, {});
                                });
}

} // namespace fincept::multiuser
