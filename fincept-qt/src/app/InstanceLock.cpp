#include "app/InstanceLock.h"

#include "core/config/EditionNames.h"
#include "core/logging/Logger.h"

#include <QByteArray>
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

// Upper bound on one framed argv payload. Real payloads are well under 1 KiB;
// the length prefix comes straight off the socket, so without a cap a bogus
// prefix (up to 4 GiB) would make the primary buffer-wait on that client forever.
constexpr quint32 kInstanceLockMaxFrameBytes = 1u << 20; // 1 MiB

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
    // Edition prefix + 16 hex digits of a hash of the key and the OS user
    // (EditionNames.h): stable across runs, distinct per edition, profile and
    // account, and short enough for the macOS sun_path cap under $TMPDIR.
    // Windows named pipes share ONE machine-wide namespace and the Linux socket
    // file sits in the shared /tmp, so without the user a second account's launch
    // could neither connect to nor replace the first account's server and ran
    // without single-instance protection.
    return edition::instance_socket_name(key, edition::current_user_scope());
}

QString InstanceLock::legacy_socket_name_for(const QString& key) {
    return edition::legacy_instance_socket_name(key);
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
        // Only this OS user may connect: the socket file gets owner-only
        // permissions (Unix), the named pipe a DACL for the user's SID (Windows;
        // the default DACL lets every account open it for reading). An elevated
        // and a normal start of the same user share the SID and still meet.
        server_->setSocketOptions(QLocalServer::UserAccessOption);
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
                if (len > kInstanceLockMaxFrameBytes) {
                    LOG_WARN(kTag, QString("Rejecting IPC frame of %1 bytes (cap %2)")
                                       .arg(len)
                                       .arg(kInstanceLockMaxFrameBytes));
                    client->disconnectFromServer();
                    return;
                }
                client->setProperty(kPendingLenProp, len);
            }

            if (client->bytesAvailable() < len)
                return; // wait for the rest of the payload

            const QByteArray payload = client->read(len);
            client->setProperty(kPendingLenProp, QVariant{});

            const auto args = parse_args(payload);
            LOG_INFO(kTag, QString("Secondary connected: %1 args").arg(args.size()));
            // Defer the emit to the next event-loop turn. Consumers open a modal
            // monitor picker (QDialog::exec spins a NESTED event loop); doing that
            // synchronously inside this QLocalSocket::readyRead slot re-enters
            // socket delivery, and the client's disconnected→deleteLater then frees
            // the socket mid-emit → doActivate crashes (EXC_BAD_ACCESS). Posting the
            // emit lets this slot fully unwind before any nested loop runs.
            QMetaObject::invokeMethod(this, [this, args]() { emit message_received(args); }, Qt::QueuedConnection);
        });

        connect(client, &QLocalSocket::disconnected, client, &QLocalSocket::deleteLater);
    }
}

} // namespace fincept
