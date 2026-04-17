#include "trading/auth/RedirectServer.h"

#include "core/logging/Logger.h"

#include <QHostAddress>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

namespace fincept::trading::auth {

namespace {
constexpr const char* kResponseHtml =
    "<!doctype html><html><head><meta charset='utf-8'>"
    "<title>Fincept - Zerodha login</title>"
    "<style>body{font-family:system-ui;background:#0f172a;color:#e2e8f0;"
    "display:flex;align-items:center;justify-content:center;height:100vh;margin:0}"
    "div{text-align:center}h1{color:#d97706;font-size:20px}p{color:#94a3b8}</style>"
    "</head><body><div><h1>Login captured</h1>"
    "<p>You can close this tab and return to Fincept Terminal.</p></div></body></html>";
}

RedirectServer::RedirectServer(QObject* parent)
    : QObject(parent),
      server_(new QTcpServer(this)),
      timeout_timer_(new QTimer(this)) {
    timeout_timer_->setSingleShot(true);
    connect(server_, &QTcpServer::newConnection, this, &RedirectServer::handle_new_connection);
    connect(timeout_timer_, &QTimer::timeout, this, &RedirectServer::handle_timeout);
}

RedirectServer::~RedirectServer() { stop(); }

bool RedirectServer::start(quint16 preferred_port, int timeout_seconds) {
    if (server_->isListening())
        server_->close();

    if (!server_->listen(QHostAddress::LocalHost, preferred_port)) {
        LOG_WARN("RedirectServer",
                 QString("Port %1 busy - falling back to ephemeral").arg(preferred_port));
        if (!server_->listen(QHostAddress::LocalHost, 0)) {
            LOG_ERROR("RedirectServer",
                      QString("Failed to bind loopback: %1").arg(server_->errorString()));
            return false;
        }
    }
    port_ = server_->serverPort();
    timeout_timer_->start(timeout_seconds * 1000);
    LOG_INFO("RedirectServer", QString("Listening on 127.0.0.1:%1").arg(port_));
    return true;
}

void RedirectServer::stop() {
    timeout_timer_->stop();
    if (server_->isListening())
        server_->close();
}

void RedirectServer::handle_new_connection() {
    QTcpSocket* sock = server_->nextPendingConnection();
    if (!sock)
        return;

    connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
        const QByteArray request = sock->readAll();
        const int line_end = request.indexOf("\r\n");
        const QByteArray first_line = (line_end >= 0) ? request.left(line_end) : request;
        const QList<QByteArray> parts = first_line.split(' ');

        QString token;
        QString error_msg;
        if (parts.size() >= 2) {
            const QUrl url(QStringLiteral("http://localhost") + QString::fromLatin1(parts.at(1)));
            const QUrlQuery q(url);
            token = q.queryItemValue("request_token");
            if (token.isEmpty()) {
                const QString err = q.queryItemValue("error");
                error_msg = err.isEmpty() ? QStringLiteral("request_token missing in redirect") : err;
            }
        } else {
            error_msg = QStringLiteral("Malformed request");
        }

        const QByteArray body(kResponseHtml);
        QByteArray response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/html; charset=utf-8\r\n";
        response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
        response += "Connection: close\r\n\r\n";
        response += body;
        sock->write(response);
        sock->flush();
        sock->disconnectFromHost();
        connect(sock, &QTcpSocket::disconnected, sock, &QTcpSocket::deleteLater);

        timeout_timer_->stop();
        if (!token.isEmpty()) {
            LOG_INFO("RedirectServer", "Captured request_token");
            server_->close();
            emit request_token_received(token);
        } else {
            LOG_WARN("RedirectServer", "No token in redirect: " + error_msg);
            emit error(error_msg);
        }
    });
}

void RedirectServer::handle_timeout() {
    LOG_WARN("RedirectServer", "Browser login timed out");
    server_->close();
    emit timeout();
}

} // namespace fincept::trading::auth
