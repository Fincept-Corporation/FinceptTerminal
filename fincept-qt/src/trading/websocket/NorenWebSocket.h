#pragma once
#include "network/websocket/WebSocketClient.h"
#include "trading/websocket/BrokerWebSocketBase.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

namespace fincept::trading {

/// NorenWebSocket — shared base for brokers on the Noren / Omnesys platform.
///
/// AliceBlue and Shoonya both expose the same JSON-over-WebSocket "NorenWS"
/// streaming protocol, so the wire handling lives here once. Concrete brokers
/// subclass this and supply only the WS URL and (for AliceBlue) the susertoken
/// derivation.
///
/// Wire protocol (text frames, JSON):
///   Connect:    {"t":"c","uid":<uid>,"actid":<actid>,"susertoken":<tok>,"source":"API"}
///   Ack:        {"t":"ck"|"cf"|"ak","s":"OK"} (connect ok) — then we flush subs.
///   Touchline:  {"t":"t","k":"NSE|12345#NSE|67890"}  (# separated EXCH|TOKEN)
///   Depth:      {"t":"d","k":"NSE|12345"}
///   Unsub:      {"t":"u","k":...} touchline / {"t":"ud","k":...} depth
///   Heartbeat:  {"t":"h"} every ~30s (broker requires one within ~50s)
///   Ticks:      t = "tk"/"tf" (touchline full/partial), "dk"/"df" (depth full/partial)
///               tk=token, e=exchange, ts=symbol, lp=ltp, o/h/l/c, v=volume,
///               ap=avg, pc=pct change, toi=OI, bp1..bp5/bq1..bq5/bo1..bo5 (bid),
///               sp1..sp5/sq1..sq5/so1..so5 (ask). Partial (tf/df) frames carry
///               only changed fields — filled via BrokerWebSocketBase::merge_tick().
///
/// Usage:
///   auto* ws = new AliceBlueWebSocket(uid, actid, jwt, this);  // or ShoonyaWebSocket
///   connect(ws, &NorenWebSocket::tick_received, this, &MyWidget::on_tick);
///   ws->subscribe({"NSE|22", "NSE|2885"});
///   ws->open();
class NorenWebSocket : public BrokerWebSocketBase {
    Q_OBJECT
  public:
    /// @param ws_url       broker NorenWS endpoint
    /// @param user_id       uid sent in the connect frame
    /// @param account_id    actid sent in the connect frame
    /// @param susertoken    session token (raw); derive_susertoken() may transform it
    NorenWebSocket(const QString& ws_url, const QString& user_id, const QString& account_id,
                   const QString& susertoken, QObject* parent = nullptr);

    void open() override;
    void close() override;
    bool is_connected() const override;

    // Symbol-based subscribe — scrips are "EXCHANGE|TOKEN" strings (Noren native form).
    // Subscribes both touchline and depth so consumers get quotes + order book.
    void subscribe(const QStringList& scrips);

    // Token-based subscribe (BrokerWebSocketBase contract). Tokens are mapped to
    // scrips via the exchange supplied with set_exchange_for_tokens(); when no
    // mapping exists they default to the exchange set by set_default_exchange().
    void subscribe(const QVector<qint64>& tokens) override;

    void unsubscribe() override;
    void unsubscribe(const QStringList& scrips);

    /// Default exchange used when a token-based subscribe has no per-token mapping.
    void set_default_exchange(const QString& exchange) { default_exchange_ = exchange; }

    /// Provide the exchange for specific tokens so subscribe(QVector<qint64>) can
    /// build "EXCHANGE|TOKEN" scrips.
    void set_exchange_for_token(qint64 token, const QString& exchange);

  protected:
    /// Derive the susertoken sent in the connect frame from the raw session
    /// token. Default is identity (Shoonya). AliceBlue overrides with double
    /// SHA256 of the JWT.
    virtual QString derive_susertoken(const QString& raw) const { return raw; }

    /// uid value for the connect frame. AliceBlue overrides to append "_API".
    virtual QString connect_uid() const { return user_id_; }

    /// actid value for the connect frame. AliceBlue overrides to append "_API".
    virtual QString connect_actid() const { return account_id_; }

    void on_data_stall() override;
    void flush_subscribe_queue() override;

  private slots:
    void on_ws_connected();
    void on_ws_disconnected();
    void on_ws_message(const QString& message);

  private:
    void send_connect_frame();
    void send_heartbeat();
    void resubscribe_all();
    void handle_touchline(const QJsonObject& obj, bool full);
    void handle_depth(const QJsonObject& obj, bool full);
    QString scrip_for_token(qint64 token) const;

    QString ws_url_;
    QString user_id_;
    QString account_id_;
    QString susertoken_raw_;
    QString default_exchange_ = "NSE";

    WebSocketClient* ws_ = nullptr;
    QTimer heartbeat_timer_;
    bool auth_ok_ = false;

    QSet<QString> subscribed_scrips_;          // all "EXCHANGE|TOKEN" we want active
    QHash<qint64, QString> token_exchange_;    // token → exchange override

    static constexpr int kHeartbeatMs = 30000;     // send {"t":"h"} every 30s
    static constexpr int kSubscribeBatch = 100;    // # scrips per touchline/depth frame
};

} // namespace fincept::trading
