#pragma once
// IciciDirectWebSocket — ICICI Direct (Breeze) live market-data adapter.
//
// Breeze streams over Socket.IO (Engine.IO v4) on WebSocket transport, unlike
// the raw-WebSocket brokers. The handshake/framing mirrors IIFLWebSocket; the
// differences are:
//   • auth travels in the Socket.IO CONNECT packet as {"user":id,"token":key}
//     (not the URL query string),
//   • subscribe/unsubscribe are Socket.IO emits — 42["join", <tokens>] /
//     42["leave", <tokens>] — where each token is "<exch>.1!<instrument_token>",
//   • ticks arrive on the "stock" event as a POSITIONAL list (not an object).
//
// Tokens come from AccountDataStream as numeric ICICI instrument tokens; we map
// token↔symbol/exchange via InstrumentService ("icicidirect").

#include "trading/websocket/BrokerWebSocketBase.h"

#include <QJsonArray>
#include <QSet>
#include <QString>

namespace fincept {
class WebSocketClient;
}

namespace fincept::trading {

class IciciDirectWebSocket : public BrokerWebSocketBase {
    Q_OBJECT
  public:
    /// `session_token_b64` is the base64 "user_id:session_key" stored as the
    /// broker access_token; `user_id` is the ICICI user id (either may seed the
    /// other — we decode the base64 to recover the raw session_key for auth).
    IciciDirectWebSocket(const QString& session_token_b64, const QString& user_id, QObject* parent = nullptr);
    ~IciciDirectWebSocket() override;

    void open() override;
    void close() override;
    bool is_connected() const override;
    void subscribe(const QVector<qint64>& tokens) override;
    void unsubscribe() override;
    void unsubscribe(const QVector<qint64>& tokens);

  protected:
    void on_data_stall() override;

  private slots:
    void on_ws_connected();
    void on_ws_disconnected();
    void on_ws_message(const QString& message);

  private:
    void handle_engineio(const QString& packet);
    void handle_socketio_event(const QString& payload);
    void handle_tick(const QJsonArray& data);
    void send_join(const QVector<qint64>& tokens);
    void send_leave(const QVector<qint64>& tokens);
    QString stock_token_for(qint64 token) const;

    fincept::WebSocketClient* ws_ = nullptr;
    QString user_;
    QString session_key_;
    bool sio_connected_ = false;
    QSet<qint64> subscribed_tokens_;
};

} // namespace fincept::trading
