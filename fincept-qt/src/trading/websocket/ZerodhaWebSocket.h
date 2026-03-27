#pragma once
#include "network/websocket/WebSocketClient.h"
#include "trading/websocket/ZerodhaTickTypes.h"

#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVector>

namespace fincept::trading {

/// Zerodha KiteTicker WebSocket client.
///
/// Wraps WebSocketClient to handle Zerodha's binary tick protocol.
/// Manages subscriptions in batches of 200 tokens (Kite API limit).
///
/// Usage:
///   auto* ws = new ZerodhaWebSocket(api_key, access_token, this);
///   ws->subscribe({256265, 260105});  // instrument tokens
///   connect(ws, &ZerodhaWebSocket::tick_received, this, &MyWidget::on_tick);
///   ws->open();
///
/// Reconnect: exponential backoff via WebSocketClient; on reconnect all
/// subscribed tokens are re-sent automatically.
class ZerodhaWebSocket : public QObject {
    Q_OBJECT
  public:
    explicit ZerodhaWebSocket(const QString& api_key, const QString& access_token,
                               QObject* parent = nullptr);

    /// Open the WebSocket connection.
    void open();

    /// Close the connection (does not clear subscription list).
    void close();

    bool is_connected() const;

    /// Add tokens to the subscription set and (re)subscribe if connected.
    /// Mode is always "full" — we need depth for order matching.
    void subscribe(const QVector<quint32>& tokens);

    /// Remove tokens from the subscription set.
    void unsubscribe(const QVector<quint32>& tokens);

    /// Replace the full subscription set.
    void set_subscriptions(const QVector<quint32>& tokens);

    /// Clear all subscriptions.
    void clear_subscriptions();

  signals:
    void tick_received(const fincept::trading::ZerodhaTick& tick);
    void connected();
    void disconnected();
    void error_occurred(const QString& error);

  private slots:
    void on_connected();
    void on_binary_message(const QByteArray& data);
    void on_disconnected();

  private:
    void send_subscribe(const QVector<quint32>& tokens);
    void send_unsubscribe(const QVector<quint32>& tokens);
    void send_mode(const QVector<quint32>& tokens);
    void resubscribe_all();

    // Binary packet parsing helpers
    ZerodhaTick parse_ltp_packet(const uchar* data) const;
    ZerodhaTick parse_quote_packet(const uchar* data) const;
    ZerodhaTick parse_full_packet(const uchar* data) const;

    static quint32 read_u32(const uchar* p);
    static qint32  read_i32(const uchar* p);
    static quint16 read_u16(const uchar* p);
    static double  paise_to_rupees(qint32 paise);

    QString api_key_;
    QString access_token_;
    WebSocketClient* ws_;
    QSet<quint32> subscribed_tokens_;

    static constexpr int kBatchSize = 200;
    static constexpr const char* kWsUrl = "wss://ws.kite.trade";
};

} // namespace fincept::trading
