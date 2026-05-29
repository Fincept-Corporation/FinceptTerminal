#include "trading/websocket/AliceBlueWebSocket.h"

#include <QCryptographicHash>

namespace fincept::trading {

AliceBlueWebSocket::AliceBlueWebSocket(const QString& client_id, const QString& jwt, QObject* parent)
    // user_id / account_id are passed as the bare client_id; connect_uid() /
    // connect_actid() append "_API" for the actual frame. The raw susertoken
    // is the JWT, double-hashed in derive_susertoken().
    : NorenWebSocket(QString::fromLatin1(kWsUrl), client_id, client_id, jwt, parent),
      client_id_(client_id) {
    set_default_exchange("NSE");
}

QString AliceBlueWebSocket::derive_susertoken(const QString& raw) const {
    // susertoken = SHA256(SHA256(jwt)) as lowercase hex, matching the official
    // AliceBlue client: first hash the JWT, then hash the hex digest string.
    const QByteArray first =
        QCryptographicHash::hash(raw.toUtf8(), QCryptographicHash::Sha256).toHex();
    const QByteArray second =
        QCryptographicHash::hash(first, QCryptographicHash::Sha256).toHex();
    return QString::fromLatin1(second);
}

} // namespace fincept::trading
