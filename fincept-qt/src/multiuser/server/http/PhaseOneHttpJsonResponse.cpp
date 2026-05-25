#include "multiuser/server/http/PhaseOneHttpJsonResponse.h"

#include <QJsonDocument>

namespace fincept::multiuser {

namespace {

PhaseOneHttpResponse make_json_response(int status_code, const QJsonObject& object) {
    PhaseOneHttpResponse response;
    response.status_code = status_code;
    response.body = QJsonDocument(object).toJson(QJsonDocument::Compact);
    response.headers.insert("Content-Type", "application/json");
    response.headers.insert("Content-Length", QByteArray::number(response.body.size()));
    return response;
}

} // namespace

PhaseOneHttpResponse PhaseOneHttpJsonResponse::success(int status_code, const QJsonObject& payload) {
    return make_json_response(status_code, QJsonObject{{"ok", true}, {"data", payload}});
}

PhaseOneHttpResponse PhaseOneHttpJsonResponse::error(int status_code, const QString& error_code, const QString& message) {
    return make_json_response(status_code,
                              QJsonObject{{"ok", false}, {"error_code", error_code}, {"message", message}});
}

} // namespace fincept::multiuser
