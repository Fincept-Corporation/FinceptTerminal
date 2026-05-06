#include "app/InstanceLock.h"

#include "core/logging/Logger.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDataStream>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocalSocket>

namespace fincept {

namespace {
constexpr const char* kTag = "InstanceLock";

// 200 ms is enough for a hot kernel + same-host connect. Cold starts on a
// loaded box may need more, but if the connect actually times out we fall
// through to "no primary running" and become primary ourselves — degraded
// but not broken.
constexpr int kConnectTimeoutMs = 200;

// 1 second to flush our argv to the primary. Argv is tiny (<1 KiB normally)
// so this is generous; only matters on a hung primary.
constexpr int kSendTimeoutMs = 1000;

// Property name we stash the partial-frame length on, while waiting for the
// rest of the bytes on a slow connection.
constexpr const char* kPendingLenProp = "_pendingLen";

QByteArray serialise_args(const QStringList& args) {
    QJsonArray arr;
    for (const auto& a : args)
        arr.append(a);
    QJsonObject obj;
    obj["args"] = arr;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QStringList parse_args(const QByteArray& payload) {
    QJsonParseError err{};
    auto doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        LOG_WARN(kTag, QString("Invalid IPC payload: %1").arg(err.errorString()));
        return {};
    }
    QStringList out;
    const auto arr = doc.object()["args"].toArray();
    out.reserve(arr.size());
    for (const auto& v : arr)
        out.append(v.toString());
    return out;
}

void write_framed(QLocalSocket* sock, const QByteArray& payload) {
    QByteArray block;
    {
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_0);
        out << static_cast<quint32>(payload.size());
    }
    block.append(payload);
    sock->write(block);
}
} // namespace

QString InstanceLock::socket_name_for(const QString& key) {
    // SHA-1 of the key in hex, truncated to 16 chars. Stable across runs of
    // the same key, unique enough that two profile keys won't collide, and
    // short enough that even on macOS (where local-socket paths are subject
    // to OS-level length caps via $TMPDIR) we won't trip the cap.
    const auto hash = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QStringLiteral("fincept-%1").arg(QString::fromLatin1(hash).left(16));
}

InstanceLock::InstanceLock(QObject* parent) : QObject(parent) {}

InstanceLock::~InstanceLock() {
    if (server_) {
        server_->close();
        // QLocalServer's destructor unlinks the socket file on POSIX. On
        // Windows the named pipe is reference-counted and goes away when
        // the last handle closes.
    }
}

InstanceLock::Status InstanceLock::acquire(const QString& key, const QStringList& args) {
    name_ = socket_name_for(key);

    // 1. Probe: is a primary already listening for this key?
    if (send_to_existing(name_, args)) {
        LOG_INFO(kTag, QString("Secondary handed args to primary at %1").arg(name_));
        return Status::Secondary;
    }

    // 2. Become primary. If listen() fails (most often: stale socket file
    //    from a previously-crashed primary), wipe the address and retry.
    if (!try_listen(name_)) {
        LOG_WARN(kTag, QString("Initial listen failed on %1 (%2) — clearing stale socket")
                           .arg(name_, server_ ? server_->errorString() : QStringLiteral("?")));
        QLocalServer::removeServer(name_);
        if (!try_listen(name_)) {
            LOG_ERROR(kTag, QString("Failed to bind socket %1 after retry: %2")
                                .arg(name_, server_ ? server_->errorString() : QStringLiteral("?")));
            // Degrade gracefully: every launch becomes a primary, no IPC.
            // Better than refusing to start (the SingleApplication failure
            // mode that #234 reports).
            return Status::Primary;
        }
    }

    LOG_INFO(kTag, QString("Primary instance bound to %1").arg(name_));
    return Status::Primary;
}

bool InstanceLock::try_listen(const QString& name) {
    if (!server_) {
        server_ = new QLocalServer(this);
        // Per docs, WorldAccessOption is the right choice for cross-user-
        // session apps; for our case (single user, multi-instance) the
        // default is fine and slightly safer.
        connect(server_, &QLocalServer::newConnection, this, &InstanceLock::on_new_connection);
    }
    return server_->listen(name);
}

bool InstanceLock::send_to_existing(const QString& name, const QStringList& args) {
    QLocalSocket sock;
    sock.connectToServer(name);
    if (!sock.waitForConnected(kConnectTimeoutMs))
        return false; // No one listening, or timed out — caller becomes primary.

    write_framed(&sock, serialise_args(args));
    sock.waitForBytesWritten(kSendTimeoutMs);
    sock.disconnectFromServer();
    if (sock.state() != QLocalSocket::UnconnectedState)
        sock.waitForDisconnected(kSendTimeoutMs);
    return true;
}

void InstanceLock::on_new_connection() {
    while (auto* client = server_->nextPendingConnection()) {
        client->setParent(this);

        // Each client may send its frame across multiple readyRead emissions
        // on slow networks (rare for local sockets, but possible). Buffer the
        // length prefix on the socket itself via a property so the lambda
        // can resume mid-frame without per-client state outside the socket.
        connect(client, &QLocalSocket::readyRead, this, [this, client]() {
            QVariant pending = client->property(kPendingLenProp);
            quint32 len = pending.isValid() ? pending.toUInt() : 0;

            if (len == 0) {
                if (client->bytesAvailable() < static_cast<qint64>(sizeof(quint32)))
                    return; // wait for at least the length prefix
                QDataStream in(client);
                in.setVersion(QDataStream::Qt_6_0);
                in >> len;
                client->setProperty(kPendingLenProp, len);
            }

            if (client->bytesAvailable() < len)
                return; // wait for the rest of the payload

            const QByteArray payload = client->read(len);
            client->setProperty(kPendingLenProp, QVariant{});

            const auto args = parse_args(payload);
            LOG_INFO(kTag, QString("Secondary connected: %1 args").arg(args.size()));
            emit message_received(args);
        });

        connect(client, &QLocalSocket::disconnected, client, &QLocalSocket::deleteLater);
    }
}

} // namespace fincept
