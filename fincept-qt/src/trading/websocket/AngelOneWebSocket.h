#pragma once
#include "network/websocket/WebSocketClient.h"
#include "trading/websocket/AngelOneTickTypes.h"

#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVector>

namespace fincept::trading {

/// Angel One SmartStream WebSocket client (v2).
///
/// Implements the SmartStream binary tick protocol over a standard WebSocket.
/// Authentication uses feed_token + client_code sent as a JSON connect message
/// immediately after the connection is established.
///
/// Packet format: Little-Endian binary (opposite of Zerodha which is Big-Endian).
///
/// Usage:
///   auto* ws = new AngelOneWebSocket(api_key, client_code, feed_token, this);
///   ws->subscribe({{"2885", AoExchangeType::NSE_CM}}, AoSubMode::SnapQuote);
///   connect(ws, &AngelOneWebSocket::tick_received, this, &MyWidget::on_tick);
///   ws->open();
///
/// On reconnect, all subscriptions are automatically re-sent.
class AngelOneWebSocket : public QObject {
    Q_OBJECT
  public:
    struct Subscription {
        QString        token;
        AoExchangeType exchange_type;
    };

    explicit AngelOneWebSocket(const QString& api_key,
                                const QString& client_code,
                                const QString& feed_token,
                                QObject* parent = nullptr);

    /// Open the WebSocket connection.
    void open();

    /// Close the connection (subscriptions are preserved for reconnect).
    void close();

    bool is_connected() const;

    /// Subscribe tokens in the given mode. Deduplicates internally.
    /// If already connected, sends subscribe message immediately.
    void subscribe(const QVector<Subscription>& subs, AoSubMode mode = AoSubMode::SnapQuote);

    /// Unsubscribe tokens.
    void unsubscribe(const QVector<Subscription>& subs, AoSubMode mode = AoSubMode::SnapQuote);

    /// Replace entire subscription set and re-send if connected.
    void set_subscriptions(const QVector<Subscription>& subs, AoSubMode mode = AoSubMode::SnapQuote);

    /// Clear all subscriptions.
    void clear_subscriptions();

    /// Current mode (applies to all subscriptions — simplified for equity trading).
    AoSubMode mode() const { return mode_; }

  signals:
    void tick_received(const fincept::trading::AoTick& tick);
    void connected();
    void disconnected();
    void error_occurred(const QString& error);

  private slots:
    void on_connected();
    void on_binary_message(const QByteArray& data);
    void on_text_message(const QString& msg);
    void on_disconnected();

  private:
    void send_auth();
    void send_subscribe(const QVector<Subscription>& subs, AoSubMode mode);
    void send_unsubscribe(const QVector<Subscription>& subs, AoSubMode mode);
    void resubscribe_all();

    // Binary parsing (Little-Endian — opposite of Zerodha)
    AoTick parse_tick(const QByteArray& data) const;

    // Little-endian read helpers
    static qint64  read_i64_le(const uchar* p);
    static double  read_d64_le(const uchar* p);
    static quint16 read_u16_le(const uchar* p);
    static double  paise_to_rupees(qint64 paise);
    static QString parse_token_string(const uchar* p, int max_len);
    static quint8  exchange_type_to_int(AoExchangeType t);

    QString api_key_;
    QString client_code_;
    QString feed_token_;
    AoSubMode mode_ = AoSubMode::SnapQuote;

    WebSocketClient* ws_;

    // token key = "exchange_type:token" e.g. "1:2885"
    QSet<QString> subscribed_keys_;
    QVector<Subscription> subscriptions_;

    static constexpr const char* kWsUrl = "wss://smartapisocket.angelone.in/smart-stream";
};

} // namespace fincept::trading
