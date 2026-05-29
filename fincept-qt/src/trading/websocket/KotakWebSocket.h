#pragma once
// KotakWebSocket — Kotak Neo (HSI / High-Speed Interface) market-data adapter.
//
// Port of OpenAlgo's HSWebSocketLib.py + kotak_websocket.py + kotak_adapter.py
// onto the Fincept BrokerWebSocketBase contract.
//
// IMPORTANT — protocol reality (verified against HSWebSocketLib.py):
//   The HSI protocol is NOT JSON-over-WebSocket. It is a *binary* protocol in
//   BOTH directions, framed big-endian:
//     - The connection handshake and every subscribe/unsubscribe frame is a
//       packed binary buffer (see prepareConnectionRequest2 / prepareSubsUnSubsRequest).
//     - Inbound market data arrives as binary frames decoded by HSWrapper.parseData
//       into per-topic field arrays; FLOAT32 fields are integers that must be
//       divided by (multiplier * 10^precision).
//   The Python `kotak_websocket.py` layer only *looks* JSON because hs_send()
//   accepts a JSON dict then translates it to the binary wire format internally.
//   This C++ port replicates that binary framing directly on QWebSocket.
//
// Connection handshake (binary):
//   On open we send a CONNECTION_TYPE frame built from (auth_token, sid) where
//   auth_token is the JWT ("Authorization") and sid is the redis key ("Sid").
//   The broker replies with a CONNECTION_TYPE ack; on "K" (OK) we flush queued
//   subscriptions.
//
// Subscribe (binary):
//   Scrips are "<exchange>|<token>" joined by '&', each prefixed with the feed
//   type prefix ("sf"=scrip/quote, "if"=index, "dp"=depth). Max 100 scrips per
//   frame. A 50ms debounce coalesces a burst of subscribe() calls.
//
// Inbound (binary):
//   parse_data() decodes DATA_TYPE frames into SNAP/UPDATE topic records and
//   emits BrokerQuote (scrip/index) via merge_tick → tick_received, and
//   MarketDepth (depth) via depth_received.

#include "trading/websocket/BrokerWebSocketBase.h"

#include <QByteArray>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

namespace fincept {

class WebSocketClient;

namespace trading {

class KotakWebSocket : public BrokerWebSocketBase {
    Q_OBJECT
  public:
    /// Credentials are the 4 parts of the packed Kotak token
    /// "auth_token:::sid:::hs_server_id:::access_token".
    ///   auth_token  — JWT, sent as the "Authorization" field of the cn frame
    ///   sid         — redis session id, sent as the "Sid" field of the cn frame
    ///   hs_server_id, access_token — retained for completeness (not needed on the
    ///                 streaming socket itself; the URL/host already carries auth).
    explicit KotakWebSocket(const QString& auth_token, const QString& sid,
                            const QString& hs_server_id, const QString& access_token,
                            QObject* parent = nullptr);

    void open() override;
    void close() override;
    bool is_connected() const override;

    /// Token-based subscribe (BrokerWebSocketBase contract). The exchange for each
    /// token is resolved via InstrumentService; tokens with no mapping default to
    /// the exchange set by set_default_exchange(). Subscribes the scrip ("sf")
    /// feed for quotes and the depth ("dp") feed for the order book.
    void subscribe(const QVector<qint64>& tokens) override;

    void unsubscribe() override;
    void unsubscribe(const QVector<qint64>& tokens);

    /// Default exchange ("NSE"/"NFO"/"BSE"/...) used when a token has no
    /// InstrumentService mapping.
    void set_default_exchange(const QString& exchange) { default_exchange_ = exchange; }

  protected:
    void flush_subscribe_queue() override;
    void on_data_stall() override;

  private slots:
    void on_connected();
    void on_binary_message(const QByteArray& data);
    void on_disconnected();

  private:
    // ── Wire builders (binary, big-endian) ──────────────────────────────────
    static QByteArray build_connection_request(const QString& jwt, const QString& sid);
    // subscribe_type: 4 = SUBSCRIBE_TYPE, 5 = UNSUBSCRIBE_TYPE.
    // prefix: "sf" / "if" / "dp". channel_num: HSI channel (default 1).
    static QByteArray build_subs_request(const QString& scrips_amp, int subscribe_type,
                                         const QString& prefix, int channel_num);
    static QByteArray build_ack_request(qint32 msg_num);

    // ── Inbound decode ──────────────────────────────────────────────────────
    void parse_data(const QByteArray& data);
    void send_subscribe(const QVector<qint64>& tokens, bool subscribe);
    void resubscribe_all();

    // Resolve "<exchange>|<token>" for a numeric token via InstrumentService.
    QString scrip_for_token(qint64 token) const;

    QString auth_token_;
    QString sid_;
    QString hs_server_id_;
    QString access_token_;
    QString default_exchange_ = "NSE";

    WebSocketClient* ws_ = nullptr;
    bool auth_ok_ = false;

    QSet<qint64> subscribed_tokens_;

    // Per-topic decode state (topic_id → record), mirrors HSWrapper.topic_list.
    // Holds the last-known field array so UPDATE frames (which omit unchanged
    // string fields like symbol/exchange) can be back-filled. Capped to avoid
    // unbounded growth from unsolicited topics.
    struct TopicRecord {
        QString feed_type;  // "sf" / "if" / "dp"
        QString exchange;
        QString token;      // numeric token as string (the "tk" field)
        QString tsymbol;    // trading symbol name (the "ts" field)
        double multiplier = 1.0;
        double precision_value = 100.0; // 10^precision
        int precision = 2;
        QHash<int, qint64> fields; // field index → raw long value (last seen)
    };
    QHash<qint64, TopicRecord> topics_;
    int ack_num_ = 0;
    int ack_counter_ = 0;

    static constexpr int kMaxScripsPerFrame = 100; // HSI MAX_SCRIPS
    static constexpr int kMaxTopics = 5000;
    static constexpr const char* kWsUrl = "wss://mlhsm.kotaksecurities.com";
};

} // namespace trading
} // namespace fincept
