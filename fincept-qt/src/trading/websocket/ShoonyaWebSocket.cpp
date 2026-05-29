#include "trading/websocket/ShoonyaWebSocket.h"

namespace fincept::trading {

ShoonyaWebSocket::ShoonyaWebSocket(const QString& user_id, const QString& account_id,
                                   const QString& susertoken, QObject* parent)
    // Shoonya uses the NorenWS protocol verbatim — no susertoken transform and
    // no "_API" suffix on uid/actid, so the NorenWebSocket defaults apply.
    : NorenWebSocket(QString::fromLatin1(kWsUrl), user_id, account_id, susertoken, parent) {
    set_default_exchange("NSE");
}

} // namespace fincept::trading
