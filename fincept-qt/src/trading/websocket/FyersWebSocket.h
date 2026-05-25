#pragma once
#include "network/websocket/WebSocketClient.h"
#include "trading/websocket/FyersTickTypes.h"

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVector>

namespace fincept::trading {

/// Fyers HSM WebSocket client for real-time market data.
///
/// Protocol: wss://socket.fyers.in/hsm/v1-5/prod
///   - Auth via binary message containing hsm_key extracted from JWT
///   - Subscribe via binary TLV messages with HSM topic names
///   - Tick data as Big-Endian binary (batched, snapshot + incremental updates)
///   - Client sends binary ping [0x00, 0x01, 0x0B] every 10 seconds
///   - Must send ACK every N messages (ack_interval from auth response)
class FyersWebSocket : public QObject {
    Q_OBJECT
  public:
    explicit FyersWebSocket(const QString& client_id, const QString& access_token,
                            QObject* parent = nullptr);

    void open();
    void close();
    bool is_connected() const;

    void subscribe(const QStringList& symbols);
    void unsubscribe(const QStringList& symbols);
    void set_subscriptions(const QStringList& symbols);
    void clear_subscriptions();

  signals:
    void tick_received(const fincept::trading::FyersTick& tick);
    void depth_received(const QString& symbol,
                        const QVector<QPair<double, double>>& bids,
                        const QVector<QPair<double, double>>& asks);
    void connected();
    void disconnected();
    void error_occurred(const QString& error);

  private slots:
    void on_ws_connected();
    void on_binary_message(const QByteArray& data);
    void on_text_message(const QString& msg);
    void on_ws_disconnected();

  private:
    struct TopicState {
        QString topic_name;
        QString symbol;
        QString exchange;
        quint16 multiplier = 1;
        quint8 precision = 2;
        QVector<qint32> fields;
    };

    // Auth & mode
    static QString extract_hsm_key(const QString& jwt);
    QByteArray build_auth_message() const;
    QByteArray build_mode_message(bool lite) const;

    // Subscribe
    QByteArray build_subscribe_message(const QStringList& topics, bool unsub = false) const;
    QByteArray build_ack_message(quint32 msg_number) const;
    void resolve_and_subscribe(const QStringList& symbols);
    void send_ping();

    // Parsing
    void parse_data_feed(const QByteArray& data, int offset);
    void parse_snapshot(const uchar* buf, int sz, int& pos);
    void parse_update(const uchar* buf, int sz, int& pos);
    void parse_lite_update(const uchar* buf, int sz, int& pos);
    void emit_tick(quint16 topic_id);
    void emit_depth(quint16 topic_id);

    // BE read helpers
    static quint64 read_u64_be(const uchar* p);
    static quint32 read_u32_be(const uchar* p);
    static qint32  read_i32_be(const uchar* p);
    static quint16 read_u16_be(const uchar* p);
    static quint16 read_u16_le(const uchar* p);
    static void    write_u16_be(uchar* p, quint16 v);
    static void    write_u32_be(uchar* p, quint32 v);
    static void    write_u64_be(uchar* p, quint64 v);

    double field_to_price(qint32 raw, const TopicState& ts) const;

    QString client_id_;
    QString access_token_;
    QString hsm_key_;
    WebSocketClient* ws_;
    QTimer* ping_timer_;

    QStringList subscribed_symbols_;
    QStringList subscribed_topics_;
    QHash<quint16, TopicState> topics_;
    quint32 ack_interval_ = 50;
    quint32 msg_count_ = 0;
    bool authenticated_ = false;

    static constexpr int kPingIntervalMs = 10000;
    static constexpr qint32 kSentinel = -2147483647 - 1; // INT32_MIN
    static constexpr const char* kWsUrl = "wss://socket.fyers.in/hsm/v1-5/prod";
};

} // namespace fincept::trading
