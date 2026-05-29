#pragma once
#include "trading/websocket/NorenWebSocket.h"

#include <QObject>
#include <QString>

namespace fincept::trading {

/// AliceBlueWebSocket — Noren streaming adapter for AliceBlue.
///
/// AliceBlue runs the standard NorenWS protocol (see NorenWebSocket) with two
/// broker-specific quirks handled here:
///   1. susertoken = SHA256(SHA256(jwt)) hex  (the raw session token is the
///      login JWT; the WS connect frame wants the double-hashed digest).
///   2. uid / actid in the connect frame are "<client_id>_API".
///
/// Usage:
///   auto* ws = new AliceBlueWebSocket(client_id, jwt, this);
///   connect(ws, &AliceBlueWebSocket::tick_received, this, &MyWidget::on_tick);
///   ws->subscribe({"NSE|22", "NSE|2885"});
///   ws->open();
class AliceBlueWebSocket : public NorenWebSocket {
    Q_OBJECT
  public:
    /// @param client_id  numeric UCC / client id (used for uid + actid)
    /// @param jwt         login JWT session token (hashed for susertoken)
    AliceBlueWebSocket(const QString& client_id, const QString& jwt, QObject* parent = nullptr);

  protected:
    QString derive_susertoken(const QString& raw) const override;
    QString connect_uid() const override { return client_id_ + "_API"; }
    QString connect_actid() const override { return client_id_ + "_API"; }

  private:
    QString client_id_;

    static constexpr const char* kWsUrl = "wss://ws1.aliceblueonline.com/NorenWS/";
};

} // namespace fincept::trading
