#include "auth/GoogleDesktopLogin.h"

#include "auth/ConstantTime.h"
#include "auth/LoopbackGuard.h"
#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"

#include <QByteArray>
#include <QDesktopServices>
#include <QHostAddress>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <memory>

namespace fincept::auth {

namespace {
// 128 bits from the OS CSPRNG. QRandomGenerator::system() — the plain
// QRandomGenerator is deterministically seeded and unusable for a nonce.
QString make_state_nonce() {
    quint32 words[4];
    QRandomGenerator::system()->fillRange(words);
    const QByteArray raw(reinterpret_cast<const char*>(words), static_cast<qsizetype>(sizeof(words)));
    return QString::fromLatin1(raw.toHex());
}
} // namespace

GoogleDesktopLogin::GoogleDesktopLogin(QObject* parent) : QObject(parent) {}

GoogleDesktopLogin::~GoogleDesktopLogin() = default;

void GoogleDesktopLogin::start() {
    server_ = new QTcpServer(this);
    connect(server_, &QTcpServer::newConnection, this, &GoogleDesktopLogin::on_new_connection);

    // Loopback only, ephemeral port — the website's open-redirect guard accepts
    // exactly 127.0.0.1 / localhost / [::1]. Do not widen this bind.
    if (!server_->listen(QHostAddress::LocalHost, 0)) {
        finish_error(tr("Could not start the local login listener."));
        return;
    }

    port_ = server_->serverPort();
    state_ = make_state_nonce();
    // Carry the CSRF nonce on the callback URL itself. The API appends
    // ?code=... to desktop_cb, so a correct implementation preserves an
    // existing query and echoes state back to us. Verified in
    // on_new_connection(); see the note there about the tolerant fallback.
    const QString callback = QString("http://127.0.0.1:%1/?state=%2").arg(port_).arg(state_);

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
    connect(timeout_, &QTimer::timeout, this, [this]() { finish_error(tr("Login timed out. Please try again.")); });
    timeout_->start(kTimeoutMs);

    // Log the port only — `callback` carries the state nonce.
    LOG_INFO("GoogleLogin", QString("Loopback listening on 127.0.0.1:%1; opening browser").arg(port_));

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

        // Wait for the whole header block — the guards below read headers, not
        // just the request line.
        if (buffer->indexOf("\r\n\r\n") < 0 && buffer->indexOf("\n\n") < 0) {
            if (buffer->size() < 16384)
                return;
            send_page(sock, tr("Fincept"), tr("Malformed request."));
            sock->disconnectFromHost();
            return;
        }

        const qsizetype line_end = buffer->indexOf('\n');
        QByteArray request_line = buffer->left(line_end);
        if (request_line.endsWith('\r'))
            request_line.chop(1);
        const QList<QByteArray> parts = request_line.split(' ');
        if (parts.size() < 2 || parts.at(0) != "GET") {
            send_page(sock, tr("Fincept"), tr("Malformed request."));
            sock->disconnectFromHost();
            return;
        }

        // Host + fetch-metadata guard: rejects DNS rebinding and any
        // cross-origin subresource injection (an `<img src>` / `fetch()` from
        // some other page the user has open). See auth/LoopbackGuard.h.
        const auto check = fincept::auth::check_loopback_request(*buffer, port_);
        if (!check.allowed) {
            LOG_WARN("GoogleLogin", "Rejected loopback callback: " + check.reason);
            send_page(sock, tr("Fincept"), tr("Request rejected."));
            sock->disconnectFromHost();
            return;
        }

        // desktop_cb now carries `?state=<nonce>`, so the API may end up
        // appending its own `?code=...` instead of `&code=...`. Normalise any
        // second '?' to '&' before parsing so a naive concatenation on the
        // server side still yields a readable `code`.
        QByteArray target = parts[1];
        const qsizetype qmark = target.indexOf('?');
        if (qmark >= 0) {
            for (qsizetype i = qmark + 1; i < target.size(); ++i) {
                if (target[i] == '?')
                    target[i] = '&';
            }
        }

        const QUrl url(QStringLiteral("http://127.0.0.1") + QString::fromUtf8(target));
        const QUrlQuery query(url);
        const QString code = query.queryItemValue("code");

        // CSRF state check. A cross-site *navigation* to this port is
        // header-indistinguishable from the real callback, so the nonce is the
        // only gate that stops it.
        //
        // Tolerant when the nonce is absent rather than hard-failing: the API
        // owns the desktop_cb → redirect construction, and if it rebuilds the
        // URL instead of appending to it, `state` never comes back and a strict
        // check would lock every user out of Google sign-in. The warning below
        // fires on the very first login if that happens — once the round trip
        // is confirmed in production, delete the `echoed.isEmpty()` branch and
        // make a missing state a rejection.
        if (!code.isEmpty()) {
            const QString echoed = query.queryItemValue("state");
            if (echoed.isEmpty()) {
                LOG_WARN("GoogleLogin", "Callback carried no state nonce — the API is not echoing desktop_cb's "
                                        "query back. CSRF protection is degraded until it does.");
            } else if (!fincept::auth::constant_time_equals(echoed, state_)) {
                LOG_WARN("GoogleLogin", "Rejected loopback callback: state nonce mismatch");
                send_page(sock, tr("Fincept"), tr("Request rejected."));
                sock->disconnectFromHost();
                return;
            }
        }

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
