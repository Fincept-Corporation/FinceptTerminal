#include "services/alpha_arena/SecretRedemptionServer.h"

#include "core/logging/Logger.h"
#include "storage/secure/SecureStorage.h"

#include <QByteArray>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>

#include <cstdint>

namespace fincept::services::alpha_arena {

namespace {

QByteArray length_prefix(const QByteArray& body) {
    QByteArray out;
    QDataStream s(&out, QIODevice::WriteOnly);
    s.setByteOrder(QDataStream::LittleEndian);
    s << static_cast<quint32>(body.size());
    out.append(body);
    return out;
}

QByteArray reply_error(const QString& msg) {
    QJsonObject o;
    o["error"] = msg;
    return length_prefix(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QByteArray reply_ok(const QString& api_key) {
    QJsonObject o;
    o["api_key"] = api_key;
    return length_prefix(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

} // namespace

SecretRedemptionServer::SecretRedemptionServer(QObject* parent)
    : QObject(parent), server_(new QLocalServer(this)) {
    connect(server_, &QLocalServer::newConnection, this, &SecretRedemptionServer::on_new_connection);
}

SecretRedemptionServer::~SecretRedemptionServer() {
    stop();
}

bool SecretRedemptionServer::start(const QString& pipe_name) {
    stop();
    redeemed_.clear();
    // Remove any stale endpoint with the same name (Linux/macOS unix-socket
    // file may persist across crashes).
    QLocalServer::removeServer(pipe_name);
    if (!server_->listen(pipe_name)) {
        LOG_ERROR("AlphaArena.Secret",
                  QString("listen on %1 failed: %2").arg(pipe_name, server_->errorString()));
        return false;
    }
    pipe_name_ = pipe_name;
    LOG_INFO("AlphaArena.Secret", QString("listening on %1").arg(pipe_name));
    return true;
}

void SecretRedemptionServer::stop() {
    if (server_->isListening()) {
        server_->close();
        LOG_INFO("AlphaArena.Secret", QString("closed %1").arg(pipe_name_));
    }
    pipe_name_.clear();
}

void SecretRedemptionServer::on_new_connection() {
    while (auto* sock = server_->nextPendingConnection()) {
        connect(sock, &QLocalSocket::readyRead, this, &SecretRedemptionServer::on_socket_ready_read);
        // Auto-cleanup
        connect(sock, &QLocalSocket::disconnected, sock, &QObject::deleteLater);
    }
}

void SecretRedemptionServer::on_socket_ready_read() {
    auto* sock = qobject_cast<QLocalSocket*>(sender());
    if (!sock) return;

    // Wait for at least 4 bytes (length prefix).
    if (sock->bytesAvailable() < 4) return;

    QByteArray hdr = sock->read(4);
    QDataStream hs(hdr);
    hs.setByteOrder(QDataStream::LittleEndian);
    quint32 len = 0;
    hs >> len;
    if (len == 0 || len > 64 * 1024) {
        sock->write(reply_error(QStringLiteral("bad length prefix")));
        sock->disconnectFromServer();
        return;
    }

    // Read body. The subprocess sends a small JSON object (~100 bytes) so we
    // can afford the synchronous loop here without yielding.
    QByteArray body;
    while (body.size() < static_cast<int>(len)) {
        if (sock->bytesAvailable() == 0 && !sock->waitForReadyRead(500)) break;
        body.append(sock->read(static_cast<int>(len) - body.size()));
    }
    if (body.size() < static_cast<int>(len)) {
        sock->write(reply_error(QStringLiteral("short body")));
        sock->disconnectFromServer();
        return;
    }

    auto doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        sock->write(reply_error(QStringLiteral("malformed JSON")));
        sock->disconnectFromServer();
        return;
    }
    const QString handle = doc.object().value("handle").toString();
    if (handle.isEmpty()) {
        sock->write(reply_error(QStringLiteral("missing handle")));
        sock->disconnectFromServer();
        return;
    }
    if (redeemed_.contains(handle)) {
        sock->write(reply_error(QStringLiteral("handle_already_redeemed")));
        sock->disconnectFromServer();
        return;
    }

    auto r = SecureStorage::instance().retrieve(handle);
    if (r.is_err()) {
        LOG_WARN("AlphaArena.Secret", QString("redeem failed for handle: %1")
                                          .arg(QString::fromStdString(r.error())));
        sock->write(reply_error(QStringLiteral("not_found")));
        sock->disconnectFromServer();
        return;
    }

    redeemed_.insert(handle);
    sock->write(reply_ok(r.value()));
    sock->flush();
    sock->disconnectFromServer();
}

} // namespace fincept::services::alpha_arena
