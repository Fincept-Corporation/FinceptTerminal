#include "auth/GoogleDesktopLogin.h"

#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"

#include <QByteArray>
#include <QDesktopServices>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <memory>

namespace fincept::auth {

GoogleDesktopLogin::GoogleDesktopLogin(QObject* parent) : QObject(parent) {}

GoogleDesktopLogin::~GoogleDesktopLogin() = default;

void GoogleDesktopLogin::start() {
    server_ = new QTcpServer(this);
    connect(server_, &QTcpServer::newConnection, this, &GoogleDesktopLogin::on_new_connection);

    // Loopback only, ephemeral port — the website's open-redirect guard accepts
    // exactly 127.0.0.1 / localhost / [::1].
    if (!server_->listen(QHostAddress::LocalHost, 0)) {
        finish_error(tr("Could not start the local login listener."));
        return;
    }

    const quint16 port = server_->serverPort();
    const QString callback = QString("http://127.0.0.1:%1").arg(port);

    // Open the API's server-side OAuth start endpoint (it redirects to Google and
    // does the token exchange). Same base as HttpClient so dev/staging overrides
    // apply. QUrlQuery percent-encodes desktop_cb on serialization.
    QString base = AppConfig::instance().api_base_url();
    if (base.endsWith('/'))
        base.chop(1);
    QUrl start_url(base + QStringLiteral("/user/auth/google/start"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("desktop_cb"), callback);
    start_url.setQuery(query);

    timeout_ = new QTimer(this);
    timeout_->setSingleShot(true);
    connect(timeout_, &QTimer::timeout, this,
            [this]() { finish_error(tr("Login timed out. Please try again.")); });
    timeout_->start(kTimeoutMs);

    LOG_INFO("GoogleLogin", QString("Loopback listening on %1; opening browser").arg(callback));

    if (!QDesktopServices::openUrl(start_url))
        finish_error(tr("Could not open the browser for Google sign-in."));
}

void GoogleDesktopLogin::on_new_connection() {
    QTcpSocket* sock = server_->nextPendingConnection();
    if (!sock)
        return;
    connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);

    auto buffer = std::make_shared<QByteArray>();
    connect(sock, &QTcpSocket::readyRead, this, [this, sock, buffer]() {
        buffer->append(sock->readAll());

        // We only need the request line: "GET /?code=<code> HTTP/1.1".
        const int line_end = buffer->indexOf("\r\n");
        if (line_end < 0)
            return; // wait for the full request line

        const QByteArray request_line = buffer->left(line_end);
        const QList<QByteArray> parts = request_line.split(' ');
        if (parts.size() < 2) {
            send_page(sock, tr("Fincept"), tr("Malformed request."));
            sock->disconnectFromHost();
            return;
        }

        const QUrl url(QStringLiteral("http://127.0.0.1") + QString::fromUtf8(parts[1]));
        const QString code = QUrlQuery(url).queryItemValue("code");

        if (code.isEmpty()) {
            // Secondary request (e.g. /favicon.ico) or callback without a code.
            // Keep the listener alive — the real callback may still be coming.
            send_page(sock, tr("Fincept Terminal"), tr("Waiting for sign-in to complete…"));
            sock->disconnectFromHost();
            return;
        }

        if (done_) {
            send_page(sock, tr("Fincept Terminal"), tr("Login already completed — you can close this tab."));
            sock->disconnectFromHost();
            return;
        }
        finish_success(code, sock);
    });
}

void GoogleDesktopLogin::finish_success(const QString& code, QTcpSocket* sock) {
    done_ = true;
    send_page(sock, tr("Login complete"), tr("You can close this tab and return to Fincept Terminal."));
    sock->disconnectFromHost();
    LOG_INFO("GoogleLogin", "Handoff code received via loopback");
    emit code_received(code);
    cleanup();
}

void GoogleDesktopLogin::finish_error(const QString& message) {
    if (done_)
        return;
    done_ = true;
    LOG_WARN("GoogleLogin", "Login failed: " + message);
    emit failed(message);
    cleanup();
}

void GoogleDesktopLogin::send_page(QTcpSocket* sock, const QString& title, const QString& body) {
    const QString html =
        QStringLiteral(
            "<!doctype html><html><head><meta charset='utf-8'><title>%1</title>"
            "<style>body{font-family:-apple-system,Segoe UI,Roboto,sans-serif;background:#0d0d0d;color:#e6e6e6;"
            "display:flex;align-items:center;justify-content:center;height:100vh;margin:0}"
            ".card{text-align:center;padding:32px 44px;border:1px solid #2a2a2a;border-radius:8px;background:#161616}"
            "h1{color:#f5a623;font-size:18px;margin:0 0 10px;letter-spacing:1px}"
            "p{color:#9a9a9a;font-size:13px;margin:0}</style></head>"
            "<body><div class='card'><h1>%1</h1><p>%2</p></div></body></html>")
            .arg(title.toHtmlEscaped(), body.toHtmlEscaped());

    const QByteArray body_bytes = html.toUtf8();
    QByteArray resp;
    resp += "HTTP/1.1 200 OK\r\n";
    resp += "Content-Type: text/html; charset=utf-8\r\n";
    resp += "Content-Length: " + QByteArray::number(body_bytes.size()) + "\r\n";
    resp += "Connection: close\r\n\r\n";
    resp += body_bytes;

    sock->write(resp);
    sock->flush();
    sock->waitForBytesWritten(1000); // tiny local write — ensure the page lands before teardown
}

void GoogleDesktopLogin::cleanup() {
    if (timeout_)
        timeout_->stop();
    if (server_)
        server_->close();
    deleteLater();
}

} // namespace fincept::auth
