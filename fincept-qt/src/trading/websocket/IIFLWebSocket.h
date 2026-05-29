#pragma once
// IIFLWebSocket — IIFL XTS market-data adapter (Socket.IO / Engine.IO v4).
//
// Port of OpenAlgo's iifl_websocket.py + iifl_adapter.py onto the Fincept
// BrokerWebSocketBase contract.
//
// IMPORTANT — how XTS streaming actually works (verified against the py source):
//   - The realtime channel is Socket.IO (Engine.IO v4) over a raw WebSocket. We
//     speak the Engine.IO/Socket.IO framing directly on QWebSocket (no SIO lib):
//       open:    server sends "0{json}" (sid + pingInterval/pingTimeout)
//       connect: client sends "40"; server replies "40{...sid...}"
//       ping:    server sends "2"  → client replies "3" (pong)
//       event:   server sends "42[\"<event>\",<data>]"
//   - SUBSCRIPTION IS NOT A SOCKET.IO EMIT. XTS subscribes via an HTTP POST to
//       {MARKET_DATA_URL}/instruments/subscription
//     with header  Authorization: <market_token>  and body
//       { "instruments":[{exchangeSegment,exchangeInstrumentID}], "xtsMessageCode":<code> }
//     Unsubscribe is the same endpoint via HTTP PUT. The Socket.IO connection is
//     used only to *receive* the resulting data stream.
//   - Market data arrives as Socket.IO events named per XTS message code:
//       1512 = LTP, 1501 = Quote/Touchline, 1502 = Market Depth, 1510 = OI.
//     Each fires as "<code>-json-full" / "<code>-json-partial"; the payload is a
//     JSON object (or JSON string) describing the instrument + fields.
//
// Auth: XTS market-data streaming needs a market-data session token and the
// resolved userID. The Fincept broker layer resolves these (via the XTS
// marketdata login) and passes them to the constructor, so this adapter does
// not perform the login itself.
//
// Subscribe contract: tokens are exchangeInstrumentIDs. The exchangeSegment is
// resolved per token from InstrumentService (brexchange → XTS segment code);
// tokens with no mapping default to set_default_exchange().

#include "trading/websocket/BrokerWebSocketBase.h"

#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

class QNetworkAccessManager;

namespace fincept {

class WebSocketClient;

namespace trading {

class IIFLWebSocket : public BrokerWebSocketBase {
    Q_OBJECT
  public:
    /// @param market_token  XTS market-data session token (sent as the WS `token`
    ///                       query param and as the subscription Authorization header)
    /// @param user_id        resolved XTS userID (sent as the WS `userID` query param)
    explicit IIFLWebSocket(const QString& market_token, const QString& user_id,
                           QObject* parent = nullptr);
    ~IIFLWebSocket() override;

    void open() override;
    void close() override;
    bool is_connected() const override;

    /// Subscribe by exchangeInstrumentID. Subscribes Quote (1501) + Depth (1502)
    /// so consumers receive both a quote stream and the order book.
    void subscribe(const QVector<qint64>& tokens) override;

    void unsubscribe() override;
    void unsubscribe(const QVector<qint64>& tokens);

    /// Default exchange used when a token has no InstrumentService mapping.
    void set_default_exchange(const QString& exchange) { default_exchange_ = exchange; }

  protected:
    void on_data_stall() override;

  private slots:
    void on_ws_connected();
    void on_ws_message(const QString& message);
    void on_ws_disconnected();

  private:
    // Engine.IO / Socket.IO framing.
    void handle_engineio(const QString& packet);
    void handle_socketio_event(const QString& payload); // payload after the "42" prefix

    // HTTP subscription (POST = subscribe, PUT = unsubscribe).
    void http_subscription(const QVector<qint64>& tokens, bool subscribe);

    // Map Fincept/broker exchange → XTS exchangeSegment code.
    static int xts_segment(const QString& exchange);
    // Map XTS exchangeSegment code → normalised exchange string.
    static QString segment_to_exchange(int segment);

    int segment_for_token(qint64 token) const;

    // Parse one XTS market-data JSON object and emit tick/depth.
    void handle_market_data(const QString& event, const class QJsonValue& data);

    QString market_token_;
    QString user_id_;
    QString default_exchange_ = "NSE";

    WebSocketClient* ws_ = nullptr;
    QNetworkAccessManager* nam_ = nullptr; // dedicated NAM for the per-request Authorization header
    bool sio_connected_ = false;

    QSet<qint64> subscribed_tokens_;

    // XTS message codes.
    static constexpr int kCodeLtp = 1512;
    static constexpr int kCodeQuote = 1501;
    static constexpr int kCodeDepth = 1502;
    static constexpr int kCodeOi = 1510;

    static constexpr const char* kBaseUrl = "https://ttblaze.iifl.com";
    static constexpr const char* kMarketDataPath = "/apimarketdata";
    static constexpr const char* kSubscriptionUrl =
        "https://ttblaze.iifl.com/apimarketdata/instruments/subscription";
};

} // namespace trading
} // namespace fincept
