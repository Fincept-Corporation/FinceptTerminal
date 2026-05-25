#include "multiuser/server/http/PhaseOneAuditHttpRoutes.h"

#include "multiuser/server/PhaseOneAuthServer.h"
#include "multiuser/server/PhaseOneAuditServer.h"
#include "multiuser/server/http/PhaseOneAuthHttpRoutes.h"
#include "multiuser/server/http/PhaseOneHttpJsonResponse.h"
#include "multiuser/server/http/PhaseOneHttpRequestContext.h"
#include "multiuser/server/http/PhaseOneHttpServer.h"

#include <QJsonArray>
#include <QUrlQuery>

namespace fincept::multiuser {

namespace {

PhaseOneHttpResponse audit_json_response(int status_code, const QJsonObject& body) {
    return {status_code, {{"Content-Type", "application/json"}}, QJsonDocument(body).toJson(QJsonDocument::Compact)};
}

QJsonObject to_json(const PhaseOneAuditEvent& event) {
    return QJsonObject{{"timestamp", event.timestamp},
                       {"user_identity", event.user_identity},
                       {"action_type", event.action_type},
                       {"target", event.target},
                       {"result_status", event.result_status}};
}

QJsonObject to_json(const PhaseOneAuditListResponse& response) {
    QJsonArray audit_events;
    for (const PhaseOneAuditEvent& event : response.audit_events)
        audit_events.append(to_json(event));
    return QJsonObject{{"audit_events", audit_events}};
}

PhaseOneAuditFilter parse_filter(const QString& path) {
    PhaseOneAuditFilter filter;
    const QString query_string = path.section('?', 1);
    if (query_string.isEmpty())
        return filter;

    const QUrlQuery query(query_string);
    filter.user_identity = query.queryItemValue(QStringLiteral("user_identity"));
    filter.action_type = query.queryItemValue(QStringLiteral("action_type"));
    return filter;
}

bool audit_require_admin(const PhaseOneHttpRequestContext& context, PhaseOneAuthServer& auth_server,
                         PhaseOneHttpResponse* response) {
    const auto session = auth_server.current_session(context.bearer_session_token());
    if (session.is_err()) {
        *response = audit_json_response(401, QJsonObject{{"error_code", QString::fromStdString(session.error())},
                                                         {"message", map_auth_error_code_to_message(QString::fromStdString(session.error()))}});
        return false;
    }
    if (session.value().role != QStringLiteral("admin")) {
        *response = audit_json_response(403, QJsonObject{{"error_code", QStringLiteral("forbidden")},
                                                         {"message", map_auth_error_code_to_message(QStringLiteral("forbidden"))}});
        return false;
    }
    return true;
}

} // namespace

void register_phase_one_audit_routes(PhaseOneHttpServer& http_server) {
    static PhaseOneAuditServer audit_server;
    static PhaseOneAuthServer auth_server;
    register_phase_one_audit_routes(http_server, audit_server, auth_server);
}

void register_phase_one_audit_routes(PhaseOneHttpServer& http_server, PhaseOneAuditServer& audit_server) {
    static PhaseOneAuthServer auth_server;
    register_phase_one_audit_routes(http_server, audit_server, auth_server);
}

void register_phase_one_audit_routes(PhaseOneHttpServer& http_server, PhaseOneAuditServer& audit_server,
                                     PhaseOneAuthServer& auth_server) {
    http_server.register_route("GET", QStringLiteral("/phase1/admin/audit-events"),
                               [&audit_server, &auth_server](const PhaseOneHttpRequestContext& context) {
                                     PhaseOneHttpResponse denied;
                                     if (!audit_require_admin(context, auth_server, &denied))
                                         return denied;
                                     const auto result = audit_server.list_audit_events(parse_filter(context.path()));
                                     if (result.is_err()) {
                                         return audit_json_response(500, {{"error_code", QStringLiteral("audit_lookup_failed")},
                                                                          {"message", QString::fromStdString(result.error())}});
                                     }
                                     return audit_json_response(200, to_json(result.value()));
                                 });
}

} // namespace fincept::multiuser
