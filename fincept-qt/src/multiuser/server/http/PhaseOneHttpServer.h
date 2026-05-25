#pragma once

#include "core/result/Result.h"
#include "multiuser/server/http/PhaseOneHttpJsonResponse.h"
#include "multiuser/server/http/PhaseOneHttpRequestContext.h"

#include <QHostAddress>
#include <QMap>
#include <QTcpServer>

#include <functional>

namespace fincept::multiuser {

class PhaseOneHttpServer {
  public:
    using RouteHandler = std::function<PhaseOneHttpResponse(const PhaseOneHttpRequestContext&)>;

    void register_route(const QByteArray& method, const QString& path, RouteHandler handler);
    fincept::Result<void> bind(const QHostAddress& address, quint16 port);
    fincept::Result<void> start();
    fincept::Result<PhaseOneHttpResponse> dispatch(const QByteArray& raw_request) const;

  private:
    QByteArray serialize_response(const PhaseOneHttpResponse& response) const;
    void handle_new_connection();
    void handle_socket_ready_read(QTcpSocket* socket);
    bool request_is_complete(const QByteArray& raw_request) const;

    QString route_key(const QByteArray& method, const QString& path) const;

    QMap<QString, RouteHandler> routes_;
    QTcpServer server_;
    QHostAddress address_;
    quint16 port_ = 0;
    bool bound_ = false;
    bool started_ = false;
};

} // namespace fincept::multiuser
