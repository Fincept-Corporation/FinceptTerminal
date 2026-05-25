#include "multiuser/server/http/PhaseOneHttpServer.h"

#include <QTcpSocket>

namespace fincept::multiuser {

namespace {

QByteArray reason_phrase(int status_code) {
    switch (status_code) {
    case 200: return "OK";
    case 201: return "Created";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 404: return "Not Found";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    default:  return "OK";
    }
}

} // namespace

void PhaseOneHttpServer::register_route(const QByteArray& method, const QString& path, RouteHandler handler) {
    routes_.insert(route_key(method, path), std::move(handler));
}

fincept::Result<void> PhaseOneHttpServer::bind(const QHostAddress& address, quint16 port) {
    if (address.isNull())
        return fincept::Result<void>::err("invalid bind address");
    if (port == 0)
        return fincept::Result<void>::err("invalid bind port");

    address_ = address;
    port_ = port;
    bound_ = true;
    return fincept::Result<void>::ok();
}

fincept::Result<void> PhaseOneHttpServer::start() {
    if (!bound_)
        return fincept::Result<void>::err("server must be bound before start");
    if (started_)
        return fincept::Result<void>::ok();
    if (!server_.listen(address_, port_))
        return fincept::Result<void>::err(server_.errorString().toStdString());

    QObject::connect(&server_, &QTcpServer::newConnection, &server_, [this]() { handle_new_connection(); });
    started_ = true;
    return fincept::Result<void>::ok();
}

fincept::Result<PhaseOneHttpResponse> PhaseOneHttpServer::dispatch(const QByteArray& raw_request) const {
    const auto parsed = PhaseOneHttpRequestContext::parse(raw_request);
    if (parsed.is_err())
        return fincept::Result<PhaseOneHttpResponse>::err(parsed.error());

    const PhaseOneHttpRequestContext& context = parsed.value();
    const auto it = routes_.constFind(route_key(context.method(), context.path()));
    if (it == routes_.cend()) {
        return fincept::Result<PhaseOneHttpResponse>::ok(
            PhaseOneHttpJsonResponse::error(404, QStringLiteral("route_not_found"), QStringLiteral("Route not found.")));
    }

    return fincept::Result<PhaseOneHttpResponse>::ok(it.value()(context));
}

QByteArray PhaseOneHttpServer::serialize_response(const PhaseOneHttpResponse& response) const {
    QByteArray bytes;
    bytes += "HTTP/1.1 ";
    bytes += QByteArray::number(response.status_code);
    bytes += ' ';
    bytes += reason_phrase(response.status_code);
    bytes += "\r\n";

    auto headers = response.headers;
    if (!headers.contains("Content-Length"))
        headers.insert("Content-Length", QByteArray::number(response.body.size()));
    if (!headers.contains("Connection"))
        headers.insert("Connection", "close");

    for (auto it = headers.cbegin(); it != headers.cend(); ++it) {
        bytes += it.key();
        bytes += ": ";
        bytes += it.value();
        bytes += "\r\n";
    }
    bytes += "\r\n";
    bytes += response.body;
    return bytes;
}

void PhaseOneHttpServer::handle_new_connection() {
    while (server_.hasPendingConnections()) {
        QTcpSocket* socket = server_.nextPendingConnection();
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() {
            const auto response = dispatch(socket->readAll());
            PhaseOneHttpResponse http_response;
            if (response.is_err()) {
                http_response = PhaseOneHttpJsonResponse::error(400, QStringLiteral("invalid_request"),
                                                                QString::fromStdString(response.error()));
            } else {
                http_response = response.value();
            }
            socket->write(serialize_response(http_response));
            socket->disconnectFromHost();
        });
        QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }
}

QString PhaseOneHttpServer::route_key(const QByteArray& method, const QString& path) const {
    return QString::fromUtf8(method.toUpper()) + QStringLiteral(" ") + path.section('?', 0, 0);
}

} // namespace fincept::multiuser
