#pragma once
#include "trading/websocket/NorenWebSocket.h"

#include <QObject>
#include <QString>

namespace fincept::trading {

/// ShoonyaWebSocket — Noren streaming adapter for Shoonya (Finvasia).
///
/// Shoonya runs the standard NorenWS protocol (see NorenWebSocket) with no
/// broker-specific transforms: the susertoken sent in the connect frame is the
/// session token returned at login (used verbatim), and uid / actid are the
/// login user id and account id as-is.
///
/// Usage:
///   auto* ws = new ShoonyaWebSocket(user_id, account_id, susertoken, this);
///   connect(ws, &ShoonyaWebSocket::tick_received, this, &MyWidget::on_tick);
///   ws->subscribe({"NSE|22", "NSE|2885"});
///   ws->open();
class ShoonyaWebSocket : public NorenWebSocket {
    Q_OBJECT
  public:
    /// @param user_id     Shoonya login user id (uid)
    /// @param account_id  Shoonya account id (actid)
    /// @param susertoken  session token from login (sent verbatim)
    ShoonyaWebSocket(const QString& user_id, const QString& account_id, const QString& susertoken,
                     QObject* parent = nullptr);

  private:
    static constexpr const char* kWsUrl = "wss://api.shoonya.com/NorenWSTP/";
};

} // namespace fincept::trading
