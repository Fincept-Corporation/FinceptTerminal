#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QMap>

namespace fincept::multiuser {

struct PhaseOneHttpResponse {
    int status_code = 0;
    QMap<QByteArray, QByteArray> headers;
    QByteArray body;
};

class PhaseOneHttpJsonResponse {
  public:
    static PhaseOneHttpResponse success(int status_code, const QJsonObject& payload = {});
    static PhaseOneHttpResponse error(int status_code, const QString& error_code, const QString& message);
};

} // namespace fincept::multiuser
